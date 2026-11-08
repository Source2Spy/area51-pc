#!/usr/bin/env python3
"""Remove original texture copies from Main-Assets after verifying the extracted originals."""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    packs = Path(__file__).resolve().parents[2] / 'build/asset-packs'
    default_manifest = packs / 'RC1-ui-compressed/manifest.json'
    if not default_manifest.is_file():
        default_manifest = packs / 'RC1-full-mips/manifest.json'
    parser.add_argument('--manifest', type=Path, default=default_manifest)
    parser.add_argument('--delete', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    records = json.loads(args.manifest.read_text())['resources']
    candidates = []
    for record in records:
        relative = Path(record['path'])
        if relative.is_absolute() or '..' in relative.parts or relative.suffix.upper() not in {'.XBMP', '.ENVMAP'}:
            raise RuntimeError(f'Invalid resource path: {relative}')
        original = root / 'Main-Assets' / relative
        extracted = root / 'Uncompressed-Textures' / relative
        expected = record['source_sha256']
        if not extracted.is_file() or digest(extracted) != expected:
            raise RuntimeError(f'Extracted original is missing or changed: {extracted}')
        if original.is_file():
            if digest(original) != expected:
                raise RuntimeError(f'Main-Assets file differs from the extracted original: {original}')
            candidates.append(original)
    size = sum(path.stat().st_size for path in candidates)
    print(f'Verified duplicates: {len(candidates)} files, {size} bytes ({size / 1048576:.2f} MiB)')
    if not args.delete:
        print('Preview only. Use --delete to remove these files from Main-Assets.')
        return
    removed = 0
    for path in candidates:
        try:
            path.unlink()
        except OSError as error:
            raise SystemExit(f'Removed {removed} files. Cannot delete {path}: {error.strerror}') from error
        removed += 1
    print(f'Removed {removed} texture copies from Main-Assets.')


if __name__ == '__main__':
    main()
