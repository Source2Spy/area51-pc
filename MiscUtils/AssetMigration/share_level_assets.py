#!/usr/bin/env python3
"""Extract identical level assets and preserve each level's preload set."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data_root', type=Path)
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--types', choices=('textures', 'resources'), default='textures')
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = args.data_root.resolve()
    containers = {'.XBMP': 'TEXTURES'} if args.types == 'textures' else {
        '.ANIM': 'ANIMS',
        '.RIGIDGEOM': 'MODELS',
        '.SKINGEOM': 'MODELS',
        '.FXO': 'EFFECTS',
        '.DECALPKG': 'DECALS',
    }
    supported = {'.XBMP', '.RIGIDGEOM', '.SKINGEOM', '.ANIM', '.DECALPKG',
                 '.ENVMAP', '.RIGIDCOLOR', '.STRING', '.FXO', '.AUDIOPKG', '.FONT'}
    groups = defaultdict(list)
    for folder in ('LEVELS', 'PRELOAD', 'COMMON', 'UI', 'FONTS', 'TEXTURES',
                   'ANIMS', 'MODELS', 'EFFECTS', 'DECALS'):
        for path in (root / folder).rglob('*'):
            if path.is_file() and path.suffix.upper() in containers:
                groups[path.name.upper()].append((path, digest(path)))
    textures = []
    levels = defaultdict(list)
    conflicts = []
    for name, entries in sorted(groups.items()):
        copies = [p for p, _ in entries if p.is_relative_to(root / 'LEVELS') and p.parent.name == 'RESOURCE']
        if len(copies) < 2:
            continue
        if len({sha for _, sha in entries}) != 1:
            conflicts.append(name)
            continue
        resident = next((p for p, _ in entries if p.parent == root / 'PRELOAD'), None)
        destination = resident or root / containers[copies[0].suffix.upper()] / name
        if destination.exists() and digest(destination) != entries[0][1]:
            raise RuntimeError(f'Destination collision: {destination}')
        textures.append({
            'name': name,
            'sha256': entries[0][1],
            'size': copies[0].stat().st_size,
            'destination': str(destination.relative_to(root)),
            'sources': [str(p.relative_to(root)) for p in sorted(copies)],
            'resident': resident is not None,
        })
        for path in copies:
            levels[path.parent.parent].append(name)
    scripts = []
    for level, names in sorted(levels.items()):
        script = level / 'LEVEL' / 'LOADSCRIPT.TXT'
        original = script.read_bytes()
        header = re.search(rb'(\[\s*loadscript\s*:\s*)(\d+)(\s*\])', original, re.I)
        commands = list(re.finditer(rb'^[ \t]*"([^"]+)"[ \t]+"([^"\r\n]*)"[^\r\n]*(?:\r?\n|$)', original, re.M))
        if header is None or int(header[2]) != len(commands):
            raise RuntimeError(f'Invalid command count: {script}')
        resource_path = str(level.relative_to(root) / 'RESOURCE').upper()
        anchors = [m for m in commands if m[1].lower() == b'load_dfs' and m[2].decode().replace('\\', '/').upper() == resource_path]
        if len(anchors) != 1:
            raise RuntimeError(f'Expected one resource preload: {script}')
        newline = b'\r\n' if b'\r\n' in original else b'\n'
        anchor = anchors[0]
        if args.types == 'textures':
            rows = b''.join(b'   "load_resource"   "' + name.encode('ascii') + b'"' + newline for name in sorted(names))
            updated = original[:anchor.start()] + rows + original[anchor.start():]
            count = len(commands) + len(names)
        else:
            local = sorted(p.name for p in (level / 'RESOURCE').iterdir()
                           if p.is_file() and p.suffix.upper() in supported
                           and p.name.upper() not in names)
            preload = sorted(names) + local
            rows = b'   "mount_dfs"      "' + anchor[2] + b'"' + newline
            rows += b''.join(b'   "load_resource"   "' + name.encode('ascii') + b'"' + newline for name in preload)
            rows += b'   "unmount_dfs"    "' + anchor[2] + b'"' + newline
            updated = original[:anchor.start()] + rows + original[anchor.end():]
            count = len(commands) - 1 + len(preload) + 2
        updated = updated[:header.start(2)] + str(count).encode() + updated[header.end(2):]
        scripts.append((script, original, updated))
    before = sum(t['size'] * len(t['sources']) for t in textures)
    added = sum(t['size'] for t in textures if not t['resident'])
    report = {
        'assets': textures,
        'excluded_conflicts': conflicts,
        'level_count': len(scripts),
        'removed_copies': sum(len(t['sources']) for t in textures),
        'new_shared_files': sum(not t['resident'] for t in textures),
        'reused_preload_files': sum(t['resident'] for t in textures),
        'saved_bytes': before - added,
    }
    print(json.dumps({k: v for k, v in report.items() if k not in ('assets', 'excluded_conflicts')}, indent=2))
    if not args.apply:
        print('Dry run. No files changed.')
        return
    if not textures:
        print('No repeated level assets remain.')
        return
    if args.backup is None:
        raise RuntimeError('--backup outside the game data directory is required')
    backup = args.backup.resolve()
    if backup.is_relative_to(root) or backup.exists():
        raise RuntimeError('Backup must be a new directory outside game data')
    backup.mkdir(parents=True)
    (backup / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    for texture in textures:
        target = backup / 'assets' / texture['sha256']
        target.parent.mkdir(exist_ok=True)
        if not target.exists():
            shutil.copy2(root / texture['sources'][0], target)
    for script, original, _ in scripts:
        dest = backup / 'scripts' / script.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(original)
    created = []
    try:
        for texture in textures:
            dest = root / texture['destination']
            if not dest.exists():
                dest.parent.mkdir(exist_ok=True)
                shutil.copy2(backup / 'assets' / texture['sha256'], dest)
                created.append(dest)
            if digest(dest) != texture['sha256']:
                raise RuntimeError(f'Asset verification failed: {dest}')
        for script, _, updated in scripts:
            temp = script.with_name(script.name + '.assets-tmp')
            with temp.open('xb') as file:
                file.write(updated)
            shutil.copymode(script, temp)
            temp.replace(script)
        for texture in textures:
            for relative in texture['sources']:
                old = root / relative
                if digest(old) != texture['sha256']:
                    raise RuntimeError(f'Source changed during migration: {old}')
                old.unlink()
    except Exception:
        for texture in textures:
            for relative in texture['sources']:
                dest = root / relative
                if not dest.exists():
                    shutil.copy2(backup / 'assets' / texture['sha256'], dest)
        for script, original, _ in scripts:
            script.write_bytes(original)
            script.with_name(script.name + '.assets-tmp').unlink(missing_ok=True)
        for path in created:
            path.unlink()
        raise
    receipt = root / ('tools/level-textures' if args.types == 'textures' else 'tools/level-resources')
    receipt.mkdir(parents=True, exist_ok=True)
    (receipt / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Applied. Recovery files: {backup}')


if __name__ == '__main__':
    main()
