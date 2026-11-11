#!/usr/bin/env python3
"""Group level preload scripts by resource type between their RESOURCE mounts."""
import argparse
from collections import Counter
from pathlib import Path
import re


COMMAND = re.compile(rb'^[ \t]*"([^"]+)"[ \t]+"([^"\r\n]*)"[^\r\n]*(?:\r?\n|$)', re.M)
ORDER = {'.XBMP': 0, '.ENVMAP': 0, '.ANIM': 1, '.RIGIDGEOM': 2,
         '.SKINGEOM': 2, '.FXO': 3, '.DECALPKG': 4, '.AUDIOPKG': 5}


def organize(original, resource):
    matches = list(COMMAND.finditer(original))
    if not matches:
        return original
    commands = [(m[1], m[2]) for m in matches]
    normalized = lambda path: path.replace(b'\\', b'/').upper()
    mounts = [c for c in commands if c[0] == b'mount_dfs' and normalized(c[1]) == normalized(resource)]
    unmounts = [c for c in commands if c[0] == b'unmount_dfs' and normalized(c[1]) == normalized(resource)]
    if len(mounts) != 1 or len(unmounts) != 1:
        raise RuntimeError('Expected exactly one RESOURCE mount/unmount pair')
    middle = [c for c in commands if c not in (mounts[0], unmounts[0])]
    newline = b'\r\n' if b'\r\n' in original else b'\n'
    argument_column = max(len(c[0]) + 2 for c in commands) + 4
    argument_width = max(len(c[1]) + 2 for c in commands)
    rule = b'// ' + b'-' * (argument_column - 4) + b' ' + b'-' * argument_width + newline
    count = re.search(rb'loadscript\s*:\s*(\d+)', original, re.I)
    if not count or int(count[1]) != len(commands):
        raise RuntimeError('Invalid command count')
    schema = b'{ command:s'.ljust(argument_column) + b'arguments:s'.ljust(argument_width) + b' }' + newline
    repeated = b'// command:s'.ljust(argument_column) + b'arguments:s' + newline
    output = [b'[ loadscript : ' + count[1] + b' ]' + newline, schema]

    def section():
        if len(output) == 2:
            output.append(rule)
        else:
            output.extend([rule, repeated, rule])

    def emit(command):
        output.append((b'"' + command[0] + b'"').ljust(argument_column) + b'"' + command[1] + b'"' + newline)

    def flush(resources):
        previous = None
        for command in sorted(resources, key=lambda c: (ORDER.get(Path(c[1].decode()).suffix.upper(), 6), c[1].upper())):
            group = ORDER.get(Path(command[1].decode()).suffix.upper(), 6)
            if group != previous:
                section()
            emit(command)
            previous = group
        resources.clear()

    section()
    emit(mounts[0])
    resources = []
    for command in middle:
        if command[0] == b'load_resource':
            resources.append(command)
        else:
            flush(resources)
            section()
            emit(command)
    flush(resources)
    section()
    emit(unmounts[0])
    updated = b''.join(output)
    assert Counter(commands) == Counter((m[1], m[2]) for m in COMMAND.finditer(updated))
    return updated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data_root', type=Path)
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = args.data_root.resolve()
    edits = []
    for script in sorted((root / 'LEVELS').glob('*/*/LEVEL/LOADSCRIPT.TXT')):
        resource = str(script.parent.parent.relative_to(root) / 'RESOURCE').encode()
        original = script.read_bytes()
        updated = organize(original, resource)
        if updated != original:
            edits.append((script, original, updated))
    print(f'{len(edits)} scripts to organize')
    if not args.apply or not edits:
        return
    if args.backup is None:
        raise RuntimeError('--backup outside game data is required')
    backup = args.backup.resolve()
    if backup.exists() or backup.is_relative_to(root):
        raise RuntimeError('Backup must be a new directory outside game data')
    backup.mkdir(parents=True)
    for path, original, _ in edits:
        dest = backup / path.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(original)
    for path, _, updated in edits:
        path.write_bytes(updated)


if __name__ == '__main__':
    main()
