#!/usr/bin/env python3
"""Compress large UI images and level slideshow images into platform texture packs."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    parser.add_argument('base_pack', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tool', required=True, type=Path)
    args = parser.parse_args()
    root, base, output = (p.resolve() for p in (args.assets, args.base_pack, args.output))
    if output.exists():
        raise RuntimeError('Output must be a new directory')
    manifest = json.loads((base / 'manifest.json').read_text())
    records = {record['path']: record for record in manifest['resources']}
    slides = set()
    for script in sorted((root / 'Main-Assets/LEVELS').rglob('SLIDESHOWSCRIPT.TXT')):
        parent = script.parent.relative_to(root / 'Main-Assets')
        files = {p.name.upper(): p for p in (root / 'Uncompressed-Textures' / parent).iterdir() if p.is_file()}
        for name in re.findall(r'"([^"\r\n]+\.XBMP)"', script.read_text(errors='replace'), re.I):
            source = files.get(name.upper())
            if source is None:
                raise RuntimeError(f'Missing slideshow texture: {script}: {name}')
            slides.add(source.relative_to(root / 'Uncompressed-Textures'))
    ui = {p.relative_to(root / 'Main-Assets') for folder in ('LORE', 'LOADING', 'SCREENS')
          for p in (root / 'Main-Assets/UI' / folder).rglob('*') if p.is_file() and p.suffix.upper() == '.XBMP'}
    selected = sorted(slides | ui)
    sources = {relative: root / ('Uncompressed-Textures' if relative in slides else 'Main-Assets') / relative
               for relative in selected}
    for relative, source in sources.items():
        if str(relative) in records and digest(source) != records[str(relative)]['source_sha256']:
            raise RuntimeError(f'Original changed: {source}')
    output.mkdir(parents=True)
    for folder in ('PC-Textures', 'Android-Textures', 'Uncompressed-Textures'):
        shutil.copytree(base / folder, output / folder)
    work = output / 'work'
    work.mkdir()
    jobs = []
    for relative in selected:
        source = sources[relative]
        backup = output / 'Uncompressed-Textures' / relative
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copyfile(source, backup)
        if digest(backup) != digest(source):
            raise RuntimeError(f'Backup mismatch: {backup}')
        record = records.setdefault(str(relative), {})
        record.update({'path': str(relative), 'source_sha256': digest(source), 'original_bytes': source.stat().st_size,
                       'mode': 'compressed', 'reason': '', 'mips': 'none', 'preserved_source_mips': [1],
                       'profile': 'UI: DXT1/DXT5, ASTC 4x4 quality 60'})
        targets = [output / folder / relative for folder in ('PC-Textures', 'Android-Textures')]
        for target in targets:
            if target.exists():
                target.unlink()
        jobs.append((source, *targets, 1, 'none', 4))
    print(f'UI: {len(ui)} images; slideshow: {len(slides)} images; total: {len(selected)}', flush=True)
    def run(index):
        job_file = work / f'jobs_{index}.tsv'
        job_file.write_text(''.join('\t'.join(map(str, job)) + '\n' for job in jobs[index::8]))
        with (work / f'worker_{index}.log').open('w') as log:
            subprocess.run([str(args.tool.resolve()), str(job_file), str(work / f'report_{index}.tsv')],
                           stdout=log, stderr=log, check=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        list(executor.map(run, range(8)))
    for relative in selected:
        record = records[str(relative)]
        for platform, folder, formats in (('pc', 'PC-Textures', {73, 77}), ('android', 'Android-Textures', {79})):
            path = output / folder / relative
            with path.open('rb') as stream:
                size, clut, width, height, pw, flags, mips, fmt = struct.unpack('<8I', stream.read(32))
            if fmt not in formats or mips != 0 or path.stat().st_size != 32 + size + clut:
                raise RuntimeError(f'Invalid UI product: {path}')
            record[platform] = {'bytes': path.stat().st_size, 'sha256': digest(path),
                                'faces': [{'format': fmt, 'width': width, 'height': height, 'mip_count': 1}]}
        if digest(sources[relative]) != record['source_sha256']:
            raise RuntimeError(f'Original changed during build: {relative}')
    manifest['resources'] = [records[path] for path in sorted(records)]
    manifest['settings']['ui'] = {'desktop': 'DXT1 opaque / DXT5 alpha', 'mobile': 'ASTC 4x4 quality 60',
                                  'mips': 'None', 'images': len(ui), 'slides': len(slides)}
    for platform, folder in (('pc', 'PC-Textures'), ('android', 'Android-Textures'), ('source', 'Uncompressed-Textures')):
        actual = {str(p.relative_to(output / folder)) for p in (output / folder).rglob('*') if p.is_file()}
        if actual != set(records):
            raise RuntimeError(f'Unexpected file set: {folder}')
        for record in records.values():
            expected = record['source_sha256'] if platform == 'source' else record[platform]['sha256']
            if digest(output / folder / record['path']) != expected:
                raise RuntimeError(f'Hash mismatch: {folder}/{record["path"]}')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for platform in ('pc', 'android'):
        before = sum(records[str(relative)]['original_bytes'] for relative in selected)
        after = sum(records[str(relative)][platform]['bytes'] for relative in selected)
        print(f'{platform}: selected images {before} -> {after} bytes; total pack {sum(r[platform]["bytes"] for r in records.values())} bytes', flush=True)
    print(f'Verified {len(records)} resources in each of the three packs: {output}', flush=True)


if __name__ == '__main__':
    main()
