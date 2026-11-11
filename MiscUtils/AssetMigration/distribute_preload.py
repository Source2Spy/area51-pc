#!/usr/bin/env python3
"""Move PRELOAD assets into existing folders and preserve their startup load order."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()



def format_resource_list(resources, newline=b'\r\n'):
    order = {'.XBMP': 0, '.ENVMAP': 1, '.ANIM': 2, '.RIGIDGEOM': 3,
             '.SKINGEOM': 4, '.FXO': 5, '.DECALPKG': 6, '.AUDIOPKG': 7}
    names = sorted(resources, key=lambda name: (order.get(Path(name).suffix.upper(), 8), name.upper()))
    width = max(len(name.encode('ascii')) + 2 for name in names)
    rule = b'// ' + b'-' * width + newline
    output = [b'[ resources : ' + str(len(names)).encode() + b' ]' + newline,
              b'{ name:s'.ljust(width + 3) + b' }' + newline, rule]
    previous = None
    for name in names:
        extension = Path(name).suffix.upper()
        if previous is not None and extension != previous:
            output.extend([rule, b'// name:s' + newline, rule])
        output.append(b'"' + name.encode('ascii') + b'"' + newline)
        previous = extension
    return b''.join(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data_root', type=Path)
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = args.data_root.resolve()
    folders = {'.XBMP': 'TEXTURES', '.ENVMAP': 'TEXTURES', '.ANIM': 'ANIMS',
               '.RIGIDGEOM': 'MODELS', '.SKINGEOM': 'MODELS', '.FXO': 'EFFECTS',
               '.DECALPKG': 'DECALS'}
    entries = []
    for old in (root / 'PRELOAD').iterdir():
        folder = folders.get(old.suffix.upper())
        if old.name.upper() in {'SFX_FRONTEND.AUDIOPKG', 'DREAMLAND.AUDIOPKG'}:
            folder = 'UI/AUDIO'
        if not old.is_file() or not folder or not (root / folder).is_dir():
            continue
        destination = root / folder / old.name
        sha = digest(old)
        if destination.exists() and digest(destination) != sha:
            raise RuntimeError(f'Destination collision: {destination}')
        entries.append({'source': str(old.relative_to(root)),
                        'destination': str(destination.relative_to(root)),
                        'resource': ('AUDIO/' if folder == 'UI/AUDIO' else '') + old.name,
                        'sha256': sha, 'size': old.stat().st_size})
    print(json.dumps(dict(Counter(str(Path(x['destination']).parent) for x in entries)), indent=2))
    if not entries:
        print('No matching PRELOAD assets remain.')
        return
    list_file = root / 'PRELOAD/PRELOAD_RESOURCES.TXT'
    if list_file.exists():
        raise RuntimeError(f'Resource list already exists: {list_file}')
    manifest = format_resource_list([x['resource'] for x in entries])
    audio = {Path(x['source']).name.upper(): x['resource'] for x in entries if x['destination'].startswith('UI/AUDIO/')}
    def replace_audio(match):
        name = match[1].decode(errors='replace').upper()
        if name in audio:
            return b'"' + audio[name].encode() + b'"'
        return match[0]

    edits = []
    for folder in ('LEVELS', 'PRELOAD', 'SCRIPTS'):
        for path in (root / folder).rglob('*'):
            if not path.is_file() or path.suffix.upper() != '.TXT':
                continue
            old = path.read_bytes()
            new = re.sub(rb'"([^"\r\n]*)"', replace_audio, old)
            if new != old:
                edits.append((path, old, new))
    destinations = {x['source']: x['destination'] for x in entries}
    for name, key in [('level-textures', 'textures'), ('level-resources', 'assets')]:
        path = root / 'tools' / name / 'manifest.json'
        if not path.exists():
            continue
        old = path.read_bytes()
        doc = json.loads(old)
        changed = False
        for item in doc[key]:
            if item['destination'] in destinations:
                item['destination'] = destinations[item['destination']]
                changed = True
        if changed:
            edits.append((path, old, (json.dumps(doc, indent=2) + '\n').encode()))
    print(f'{len(entries)} startup resources; {len(edits)} data files to update')
    if not args.apply:
        print('Dry run. No files changed.')
        return
    if args.backup is None:
        raise RuntimeError('--backup outside game data is required')
    backup = args.backup.resolve()
    if backup.exists() or backup.is_relative_to(root):
        raise RuntimeError('Backup must be a new directory outside game data')
    backup.mkdir(parents=True)
    (backup / 'manifest.json').write_text(json.dumps(entries, indent=2) + '\n')
    (backup / 'assets').mkdir()
    for item in entries:
        recovery = backup / 'assets' / item['sha256']
        if not recovery.exists():
            shutil.copy2(root / item['source'], recovery)
    for path, original, _ in edits:
        recovery = backup / 'files' / path.relative_to(root)
        recovery.parent.mkdir(parents=True, exist_ok=True)
        recovery.write_bytes(original)
    created = []
    try:
        for item in entries:
            dest = root / item['destination']
            if not dest.exists():
                shutil.copy2(root / item['source'], dest)
                created.append(dest)
            if digest(dest) != item['sha256']:
                raise RuntimeError(f'Asset verification failed: {dest}')
        list_file.write_bytes(manifest)
        for path, _, updated in edits:
            path.write_bytes(updated)
        for item in entries:
            old = root / item['source']
            if digest(old) != item['sha256']:
                raise RuntimeError(f'Asset changed during migration: {old}')
            old.unlink()
    except Exception:
        for item in entries:
            old = root / item['source']
            if not old.exists():
                shutil.copy2(backup / 'assets' / item['sha256'], old)
        for path, original, _ in edits:
            path.write_bytes(original)
        list_file.unlink(missing_ok=True)
        for path in created:
            path.unlink()
        raise
    receipt = root / 'tools/preload-distribution'
    receipt.mkdir(parents=True, exist_ok=True)
    (receipt / 'manifest.json').write_text(json.dumps(entries, indent=2) + '\n')
    print(f'Applied. Recovery files: {backup}')


if __name__ == '__main__':
    main()
