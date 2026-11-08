#!/usr/bin/env python3
"""Update UI references in data scripts and remove verified legacy copies.

Dry-run by default. Use --apply after deploying an executable using explicit UI paths.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data_root', type=Path)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = args.data_root.resolve()
    manifest = args.manifest or root / 'tools/ui-migration/manifest.json'
    rows = json.loads(manifest.read_text())
    mapping = {}
    links = []
    for row in rows:
        old, new = root / row['source'], root / row['destination']
        if not old.resolve().is_relative_to(root) or not new.resolve().is_relative_to(root):
            raise RuntimeError('Manifest path escapes data root')
        if hashlib.sha256(new.read_bytes()).hexdigest() != row['sha256']:
            raise RuntimeError(f'UI asset changed: {new}')
        if old.exists():
            if hashlib.sha256(old.read_bytes()).hexdigest() != row['sha256']:
                raise RuntimeError(f'Refusing to remove a modified legacy asset: {old}')
            links.append(old)
        mapping.setdefault(old.name.upper(), []).append(row)

    def replace(match):
        name = match[1].decode('ascii', errors='replace').upper()
        options = mapping.get(name)
        if not options:
            return match[0]
        origin = 'COMMON' if name.startswith('UI_LORE') else 'PRELOAD'
        selected = next((r for r in options if r['source'].startswith(origin + '/')), options[0])
        return b'"' + selected['destination'].removeprefix('UI/').encode('ascii') + b'"'

    edits = []
    for folder in ('LEVELS', 'PRELOAD', 'COMMON', 'SCRIPTS'):
        for path in (root / folder).rglob('*'):
            if not path.is_file() or path.suffix.upper() != '.TXT':
                continue
            original = path.read_bytes()
            updated = re.sub(rb'"([^"\r\n]*)"', replace, original)
            if updated != original:
                edits.append((path, updated))
    print(f'{len(links)} legacy copies; {len(edits)} data scripts to update')
    if not args.apply:
        print('Dry run. No files changed.')
        return

    backup = root / '.ui-migration-backup'
    if backup.exists():
        raise RuntimeError(f'Backup already exists: {backup}')
    backup.mkdir()
    # Back up every change before modifying data scripts or removing old paths.
    for path in links + [p for p, _ in edits]:
        dest = backup / path.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dest)
    (backup / 'manifest.json').write_text(json.dumps(rows, indent=2) + '\n')
    for path, updated in edits:
        temp = path.with_name(path.name + '.ui-migration-tmp')
        with temp.open('xb') as f:
            f.write(updated)
        shutil.copymode(path, temp)
        temp.replace(path)
    for path in links:
        path.unlink()
    print(f'Applied. Recovery copies: {backup}')


if __name__ == '__main__':
    main()
