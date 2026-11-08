#!/usr/bin/env python3
"""Offline, versioned migrations for Area 51 saves (Python 3.10+, stdlib only)."""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import re
import stat
import struct
import sys
import tempfile
from typing import Callable
import zlib

HEADER = struct.Struct("<4sHBQI")
DATES = struct.Struct("<QQ")
UNIX_EPOCH = 116444736000000000
MAX_DATE = 2650467743999999999
PROFILE_V1_SIZE = 53206  # Fixed-width SerializeProfile layout.
PROFILE_V2_SIZE = PROFILE_V1_SIZE + DATES.size
PROFILE_V3_BINDINGS = struct.Struct("<60i")  # 30 actions, primary then secondary.
PROFILE_V3_DEFAULT_KEYS = (
    143, 0,  # move_forward
    157, 0,  # move_backward
    156, 0,  # strafe_left
    158, 0,  # strafe_right
    183, 0,  # jump
    155, 0,  # crouch
    145, 0,  # reload
    171, 0,  # mutation
    141, 0,  # use
    159, 0,  # flashlight
    160, 0,  # throw_grenade
    173, 0,  # melee_attack
    128, 0,  # switch_to_scanner
    129, 0,  # switch_to_pistol
    130, 0,  # switch_to_smp
    131, 0,  # switch_to_shotgun
    132, 0,  # switch_to_sniper_rifle
    133, 0,  # switch_to_bbg
    134, 0,  # switch_to_meson_cannon
    185, 0,  # vote_menu_on
    185, 0,  # vote_menu_off
    186, 0,  # vote_yes
    187, 0,  # vote_no
    188, 0,  # vote_abstain
    146, 0,  # chat
    142, 0,  # lean_left
    144, 0,  # lean_right
    147, 0,  # talk_mode_toggle
    175, 0,  # drop_flag
    141, 0,  # scoreboard
)
PROFILE_V3_VALID_KEYS = frozenset((
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
    158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
    173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
    188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
    203, 204, 205, 206, 207, 208, 209, 212, 213, 214, 226, 227, 228, 238, 241,
    247, 249, 251, 252, 267, 270, 271, 272, 273, 274, 275, 276, 277, 279, 282,
    283, 286, 287, 288, 290, 300, 302, 304, 305, 307, 309, 310, 323, 325, 326,
    327, 329, 331, 333, 334, 335, 336, 337, 345, 346, 347, 348, 349, 353, 355,
    356, 357, 358, 359, 360, 361, 362, 363,
))
PROFILE_V4_STATS = struct.Struct("<11i")
HOST_SETTINGS_SIZE = 240
MULTIPLAYER_PREFERENCES_SIZE = HOST_SETTINGS_SIZE + 4 + 2 * 260 + 2 * (64 + 32) + 8 * 4
CURRENT_VERSIONS = {b"A51P": 5, b"A51S": 12}
SAVE_NAME = re.compile(r"AREA-51-(?:PROFILE[0-9]{5}|SETTINGS)\Z")


class MigrationError(ValueError):
    """Unsupported or invalid save data; the original must remain intact."""


@dataclass(frozen=True)
class SaveFile:
    magic: bytes
    version: int
    payload: bytes


@dataclass(frozen=True)
class Migration:
    target_version: int
    convert: Callable[[bytes, os.stat_result], bytes]


def filetime(unix_ns: int) -> int:
    value = UNIX_EPOCH + unix_ns // 100
    if not UNIX_EPOCH <= value <= MAX_DATE:
        raise MigrationError("filesystem timestamp is outside the supported range")
    return value


def creation_ns(info: os.stat_result) -> int:
    # Windows exposes birth time (st_ctime on Python < 3.12). Linux ctime is
    # metadata-change time, so it must never be mistaken for creation time.
    if hasattr(info, "st_birthtime_ns"):
        return info.st_birthtime_ns
    if hasattr(info, "st_birthtime"):
        return int(info.st_birthtime * 1_000_000_000)
    if os.name == "nt":
        return info.st_ctime_ns
    return info.st_mtime_ns


SETTINGS_JOIN_OFFSET = 8 + 5 * 4 + 4 * 4 + 16384 + 4 + (4 * 4 + 35 * 4) + (24 * 2 + 16 + 8 * 4 + 35 * 4 + 4)
SETTINGS_V6_SIZE = SETTINGS_JOIN_OFFSET + 5 * 4 + 35 * 4 + 22 * 4
SETTINGS_HOST_OFFSET = SETTINGS_JOIN_OFFSET - HOST_SETTINGS_SIZE


def settings_v5_to_v6(payload: bytes, info: os.stat_result) -> bytes:
    return payload + struct.pack("<i", 5)


def settings_v6_to_v7(payload: bytes, info: os.stat_result) -> bytes:
    return payload[:SETTINGS_JOIN_OFFSET] + payload[SETTINGS_JOIN_OFFSET + 5 * 4:]


def settings_v7_to_v8(payload: bytes, info: os.stat_result) -> bytes:
    return payload[:SETTINGS_HOST_OFFSET] + payload[SETTINGS_JOIN_OFFSET:]


def settings_v8_to_v9(payload: bytes, info: os.stat_result) -> bytes:
    return payload


def settings_v9_to_v10(payload: bytes, info: os.stat_result) -> bytes:
    offset = len(payload) - 4 * 4
    return payload[:offset] + struct.pack("<i", 1) + payload[offset:]


def settings_v10_to_v11(payload: bytes, info: os.stat_result) -> bytes:
    offset = len(payload) - 5 * 4
    return payload[:offset] + payload[offset + 4:]


def settings_v11_to_v12(payload: bytes, info: os.stat_result) -> bytes:
    return payload + struct.pack("<i", 3)


def validate_settings(payload: bytes, size: int) -> None:
    if len(payload) != size:
        raise MigrationError("settings payload has an invalid size")


def profile_v1_to_v2(payload: bytes, info: os.stat_result) -> bytes:
    return payload + DATES.pack(filetime(creation_ns(info)), filetime(info.st_mtime_ns))


def profile_v2_to_v3(payload: bytes, info: os.stat_result) -> bytes:
    return payload + PROFILE_V3_BINDINGS.pack(*PROFILE_V3_DEFAULT_KEYS)


def profile_v3_to_v4(payload: bytes, info: os.stat_result) -> bytes:
    flags_offset = PROFILE_V1_SIZE - 4
    old_flags = struct.unpack_from("<I", payload, flags_offset)[0]
    retained_bits = (0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16)
    flags = sum(((old_flags >> bit) & 1) << index for index, bit in enumerate(retained_bits))
    return (payload[:flags_offset] + struct.pack("<I", flags) + payload[flags_offset + 4:]
            + PROFILE_V4_STATS.pack(*([0] * 11)))


def profile_v4_to_v5(payload: bytes, info: os.stat_result) -> bytes:
    host = (bytes(24 * 2 + 16)
            + struct.pack("<8i", 0, -1, -1, 16, 50, 0, 0x202, 0)
            + struct.pack("<35i", *([-1] * 32 + [0, 0, 1]))
            + struct.pack("<i", 0))
    preferences = (host + struct.pack("<i", 3658) + bytes(2 * 260 + 2 * (64 + 32))
                   + struct.pack("<8i", 3, -1, -1, 0, 0, 0, 0, 0))
    return payload + preferences


def validate_profile_v5(payload: bytes) -> None:
    size = PROFILE_V2_SIZE + PROFILE_V3_BINDINGS.size + PROFILE_V4_STATS.size
    if len(payload) != size + MULTIPLAYER_PREFERENCES_SIZE:
        raise MigrationError("profile v5 payload has an invalid size")
    validate_profile_v4(payload[:size])
    preferences = payload[size:]
    game, score, time, players, vote, fire, flags, mutation = struct.unpack_from("<8i", preferences, 64)
    index, count, use_default, skill, port = struct.unpack_from("<5i", preferences, 224)
    source, mode, ping, sort, descending, full, empty, password = struct.unpack_from("<8i", preferences, 956)
    if not (0 <= game <= 5 and score >= -1 and time >= -1 and 1 <= players <= 32
            and 0 <= vote <= 100 and 0 <= fire <= 100 and 0 <= mutation <= 5
            and 0 <= skill <= 3 and 0 <= count <= 32 and 0 <= index < max(count, 1)
            and 1 <= port <= 65535 and 1 <= source <= 3 and -1 <= mode <= 5
            and ping in (-1, 50, 100, 150, 250) and 0 <= sort < 6):
        raise MigrationError("profile contains invalid multiplayer preferences")
    for offset, capacity in ((48, 16), (244, 260), (504, 260)):
        if b"\0" not in preferences[offset:offset + capacity]:
            raise MigrationError("profile contains an unterminated multiplayer string")
    for offset, capacity in ((0, 24), (764, 64), (892, 32)):
        if not any(preferences[i:i + 2] == b"\0\0" for i in range(offset, offset + capacity * 2, 2)):
            raise MigrationError("profile contains an unterminated multiplayer string")


def validate_profile_v4(payload: bytes) -> None:
    if len(payload) != PROFILE_V2_SIZE + PROFILE_V3_BINDINGS.size + PROFILE_V4_STATS.size:
        raise MigrationError("profile v4 payload has an invalid size")
    validate_profile_v3(payload[:-PROFILE_V4_STATS.size])
    if any(value < 0 for value in PROFILE_V4_STATS.unpack_from(payload, len(payload) - PROFILE_V4_STATS.size)):
        raise MigrationError("profile contains negative career statistics")


def validate_profile_v3(payload: bytes) -> None:
    if len(payload) != PROFILE_V2_SIZE + PROFILE_V3_BINDINGS.size:
        raise MigrationError("profile v3 payload has an invalid size")
    validate_profile_v2(payload[:PROFILE_V2_SIZE])
    keys = PROFILE_V3_BINDINGS.unpack_from(payload, PROFILE_V2_SIZE)
    if any(key != 0 and key not in PROFILE_V3_VALID_KEYS for key in keys):
        raise MigrationError("profile contains invalid keyboard bindings")


def validate_profile_v1(payload: bytes) -> None:
    if len(payload) != PROFILE_V1_SIZE:
        raise MigrationError(f"profile v1 payload must be {PROFILE_V1_SIZE} bytes")


def validate_profile_v2(payload: bytes) -> None:
    if len(payload) != PROFILE_V1_SIZE + DATES.size:
        raise MigrationError(f"profile v2 payload must be {PROFILE_V1_SIZE + DATES.size} bytes")
    if any(not UNIX_EPOCH <= date <= MAX_DATE for date in DATES.unpack(payload[-DATES.size:])):
        raise MigrationError("profile contains invalid timestamps")


# Add future steps here, together with a validator for their resulting format.
# The game contains no migration logic. Every step consumes exactly one version.
MIGRATIONS = {
    (b"A51P", 1): Migration(2, profile_v1_to_v2),
    (b"A51P", 2): Migration(3, profile_v2_to_v3),
    (b"A51P", 3): Migration(4, profile_v3_to_v4),
    (b"A51P", 4): Migration(5, profile_v4_to_v5),
    (b"A51S", 5): Migration(6, settings_v5_to_v6),
    (b"A51S", 6): Migration(7, settings_v6_to_v7),
    (b"A51S", 7): Migration(8, settings_v7_to_v8),
    (b"A51S", 8): Migration(9, settings_v8_to_v9),
    (b"A51S", 9): Migration(10, settings_v9_to_v10),
    (b"A51S", 10): Migration(11, settings_v10_to_v11),
    (b"A51S", 11): Migration(12, settings_v11_to_v12),
}
VALIDATORS: dict[tuple[bytes, int], Callable[[bytes], None]] = {
    (b"A51P", 1): validate_profile_v1,
    (b"A51P", 2): validate_profile_v2,
    (b"A51P", 3): validate_profile_v3,
    (b"A51P", 4): validate_profile_v4,
    (b"A51P", 5): validate_profile_v5,
    (b"A51S", 5): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 4),
    (b"A51S", 6): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE),
    (b"A51S", 7): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 20),
    (b"A51S", 8): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 20 - HOST_SETTINGS_SIZE),
    (b"A51S", 9): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 20 - HOST_SETTINGS_SIZE),
    (b"A51S", 10): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 20 - HOST_SETTINGS_SIZE + 4),
    (b"A51S", 11): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 20 - HOST_SETTINGS_SIZE),
    (b"A51S", 12): lambda payload: validate_settings(payload, SETTINGS_V6_SIZE - 20 - HOST_SETTINGS_SIZE + 4),
}


def validate(save: SaveFile) -> None:
    validator = VALIDATORS.get((save.magic, save.version))
    if validator is None:
        raise MigrationError(f"unsupported {save.magic!r} version {save.version}")
    validator(save.payload)


def decode(data: bytes) -> SaveFile:
    if len(data) < HEADER.size:
        raise MigrationError("file is smaller than its header")
    magic, version, header_size, payload_size, checksum = HEADER.unpack_from(data)
    if magic not in CURRENT_VERSIONS:
        raise MigrationError(f"unknown signature {magic!r}")
    if header_size != HEADER.size:
        raise MigrationError(f"unsupported header size {header_size}")
    if payload_size > 0x7FFFFFFF or payload_size != len(data) - HEADER.size:
        raise MigrationError("payload size does not match the file size")
    payload = data[HEADER.size:]
    if zlib.crc32(payload) != checksum:
        raise MigrationError("payload checksum does not match")
    save = SaveFile(magic, version, payload)
    validate(save)
    return save


def encode(save: SaveFile) -> bytes:
    validate(save)
    return HEADER.pack(save.magic, save.version, HEADER.size,
                       len(save.payload), zlib.crc32(save.payload)) + save.payload


def migrate(save: SaveFile, info: os.stat_result, host: bytes | None = None) -> SaveFile:
    validate(save)
    original_version = save.version
    target = CURRENT_VERSIONS[save.magic]
    while save.version != target:
        step = MIGRATIONS.get((save.magic, save.version))
        if step is None or not save.version < step.target_version <= target:
            raise MigrationError(f"no migration from {save.magic!r} v{save.version} to v{target}")
        save = SaveFile(save.magic, step.target_version, step.convert(save.payload, info))
        validate(save)
    if save.magic == b"A51P" and original_version < 5 and host is not None:
        offset = len(save.payload) - MULTIPLAYER_PREFERENCES_SIZE
        save = SaveFile(save.magic, save.version,
                        save.payload[:offset] + host + save.payload[offset + HOST_SETTINGS_SIZE:])
        validate(save)
    return save


def snapshot(info: os.stat_result) -> tuple[int, ...]:
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns


def make_backup(path: Path, data: bytes, version: int, mode: int) -> Path:
    base = path.with_name(f"{path.name}.v{version}.bak")
    backup = base
    suffix = 0
    while True:
        try:
            handle = backup.open("xb")
            break
        except FileExistsError:
            suffix += 1
            backup = base.with_name(f"{base.name}.{suffix}")
    try:
        with handle:
            os.chmod(backup, mode)
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
    except BaseException:
        backup.unlink(missing_ok=True)
        raise
    return backup


def convert_file(path: Path, *, dry_run: bool = False) -> str:
    if path.is_symlink():
        raise MigrationError("symbolic links are not supported")
    info = path.stat()
    if not stat.S_ISREG(info.st_mode):
        raise MigrationError("expected a regular save file")
    data = path.read_bytes()
    original = decode(data)
    host = None
    settings_path = path.with_name("AREA-51-SETTINGS")
    if original.magic == b"A51P" and original.version < 5 and settings_path.exists():
        settings = decode(settings_path.read_bytes())
        if settings.magic != b"A51S":
            raise MigrationError("the settings file has an invalid signature")
        if settings.version < 8:
            host = settings.payload[SETTINGS_HOST_OFFSET:SETTINGS_JOIN_OFFSET]
    converted = migrate(original, info, host)
    if snapshot(path.stat()) != snapshot(info):
        raise MigrationError("file changed while being read; close the game and retry")
    if converted.version == original.version:
        return f"{path}: already v{original.version}, unchanged"
    description = f"{path}: v{original.version} -> v{converted.version}"
    if dry_run:
        return description + " (dry run, no files written)"

    output = encode(converted)
    mode = stat.S_IMODE(info.st_mode)
    descriptor, name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as handle:
            os.chmod(temporary, mode)
            handle.write(output)
            handle.flush()
            os.utime(temporary, ns=(info.st_atime_ns, info.st_mtime_ns))
            os.fsync(handle.fileno())
        if path.is_symlink() or snapshot(path.stat()) != snapshot(info):
            raise MigrationError("file changed before replacement; close the game and retry")
        backup = make_backup(path, data, original.version, mode)
        if path.is_symlink() or snapshot(path.stat()) != snapshot(info):
            raise MigrationError(f"file changed before replacement; backup retained at {backup}")
        os.replace(temporary, path)
        if os.name == "posix":
            directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
    finally:
        temporary.unlink(missing_ok=True)
    return description + f"; backup: {backup}"


def collect_files(paths: list[Path]) -> list[Path]:
    files: dict[Path, None] = {}
    for raw in paths:
        path = raw.expanduser().absolute()
        if path.is_dir():
            matches = sorted(child for child in path.iterdir() if SAVE_NAME.fullmatch(child.name))
            if not matches:
                raise MigrationError(f"no save files found in {path}")
            files.update(dict.fromkeys(matches))
        else:
            files[path] = None
    return list(files)


def main(argv: list[str] | None = None) -> int:
    arguments = sys.argv[1:] if argv is None else argv
    if not arguments:
        print("Drag a save file or the SAVES folder onto this .py file.")
        return 0
    try:
        files = collect_files([Path(argument) for argument in arguments])
        files.sort(key=lambda path: path.name == "AREA-51-SETTINGS")
    except (OSError, MigrationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    failed = False
    for path in files:
        try:
            print(convert_file(path))
        except (OSError, MigrationError) as error:
            print(f"{path}: error: {error}", file=sys.stderr)
            failed = True
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
