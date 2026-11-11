#!/usr/bin/env python3
"""Build desktop and mobile texture overlays from the exported texture manifest."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sources', type=Path)
    parser.add_argument('originals', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tool', required=True, type=Path)
    parser.add_argument('--workers', type=int, default=8)
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--regenerate-detail-mips', action='store_true')
    args = parser.parse_args()
    sources, originals, output = (p.resolve() for p in (args.sources, args.originals, args.output))
    manifest = json.loads((sources / 'manifest.json').read_text())
    if output.exists() and not args.resume:
        raise RuntimeError('Output must be a new directory')
    for entry in manifest['resources']:
        if digest(originals / entry['source']) != entry['sha256']:
            raise RuntimeError(f'Original changed: {entry["source"]}')
        for image in entry['images']:
            for mip in image['mips']:
                if digest(sources / mip['image']) != mip['sha256']:
                    raise RuntimeError(f'Export changed: {mip["image"]}')
    pc, mobile = (output / p for p in ('PC-Textures', 'Android-Textures'))
    work = output / 'work'
    work.mkdir(parents=True, exist_ok=args.resume)
    jobs, records, cubes = [], [], []
    for entry in manifest['resources']:
        relative = Path(entry['source'])
        name = relative.name.upper()
        reason = 'level UI' if 'LEVEL' in relative.parts else ''
        if not reason and any(word in name for word in ('FOG', 'FLASHLIGHT', 'SHAD', 'COOKIE', 'PROJ', 'RAMP', 'FALLOFF')):
            reason = 'CPU palette/projection candidate'
        regenerate = args.regenerate_detail_mips and '[D0000]' in name and not reason
        record = {'path': str(relative), 'source_sha256': entry['sha256'],
                  'original_bytes': (originals / relative).stat().st_size,
                  'mode': 'original' if reason else 'compressed', 'reason': reason,
                  'mips': 'original' if reason else 'regenerate' if regenerate else 'full',
                  'preserved_source_mips': [1 if regenerate else len(image['mips']) for image in entry['images']]}
        records.append(record)
        if reason:
            for folder in (pc, mobile):
                target = folder / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(originals / relative, target)
            continue
        for image in entry['images']:
            base = image['mips'][0]
            if relative.suffix.upper() == '.ENVMAP':
                target = Path('faces') / relative / f'face_{image["face"]}.XBMP'
                targets = [work / platform / target for platform in ('pc', 'mobile')]
            else:
                targets = [folder / relative for folder in (pc, mobile)]
            jobs.append((sources / base['image'], *targets, 1 if regenerate else len(image['mips']), record['mips']))
        if relative.suffix.upper() == '.ENVMAP':
            cubes.append(relative)
    workers = max(1, min(args.workers, len(jobs)))
    def run(index):
        job_file = work / f'jobs_{index}.tsv'
        report = work / f'report_{index}.tsv'
        job_file.write_text(''.join('\t'.join(map(str, job)) + '\n' for job in jobs[index::workers]))
        with (work / f'worker_{index}.log').open('w') as log:
            subprocess.run([str(args.tool.resolve()), str(job_file), str(report)], stdout=log, stderr=log, check=True)
    print(f'{len(records)} resources: {sum(r["mode"] == "compressed" for r in records)} compressed, '
          f'{sum(r["mode"] == "original" for r in records)} preserved; {workers} workers', flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        list(executor.map(run, range(workers)))
    for relative in cubes:
        for platform, folder in (('pc', pc), ('mobile', mobile)):
            parts = [work / platform / 'faces' / relative / f'face_{face}.XBMP' for face in range(6)]
            target = folder / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('wb') as stream:
                for part in parts:
                    stream.write(part.read_bytes())
    for record in records:
        for platform, folder, formats in (('pc', pc, {73, 77}), ('android', mobile, {83})):
            data = (folder / record['path']).read_bytes()
            offset, faces = 0, []
            while offset < len(data):
                size, clut, width, height, pw, flags, mips, fmt = struct.unpack_from('<8I', data, offset)
                end = offset + 32 + size + clut
                if end > len(data) or (record['mode'] == 'compressed' and (fmt not in formats or flags & (1 << 14))):
                    raise RuntimeError(f'Invalid product: {record["path"]}')
                faces.append({'format': fmt, 'width': width, 'height': height, 'mip_count': mips + 1})
                offset = end
            expected = 6 if record['path'].upper().endswith('.ENVMAP') else 1
            if offset != len(data) or len(faces) != expected:
                raise RuntimeError(f'Invalid face count: {record["path"]}')
            entry = next(e for e in manifest['resources'] if e['source'] == record['path'])
            for face, source in zip(faces, entry['images']):
                expected_mips = len(source['mips']) if record['mips'] in ('original', 'preserve') else max(source['width'], source['height']).bit_length()
                if face['mip_count'] != expected_mips:
                    raise RuntimeError(f'Wrong mip count: {record["path"]}')
            if record['mode'] == 'original' and hashlib.sha256(data).hexdigest() != record['source_sha256']:
                raise RuntimeError(f'Preserved file changed: {record["path"]}')
            record[platform] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'faces': faces}
        if digest(originals / record['path']) != record['source_sha256']:
            raise RuntimeError(f'Original changed during build: {record["path"]}')
    summary = {'version': 1, 'sources': str(sources), 'originals': str(originals),
               'settings': {'desktop': 'DXT1 opaque / DXT5 alpha', 'mobile': 'ASTC 6x6 quality 60',
                            'color_space': 'Linear (matching existing UNORM uploads)', 'mips': 'Full chain to 1x1',
                            'detail_mips': 'Regenerated from base image' if args.regenerate_detail_mips else 'Original mip pixels preserved'},
               'resources': records}
    (output / 'manifest.json').write_text(json.dumps(summary, indent=2) + '\n')
    for folder in (pc, mobile):
        if len([p for p in folder.rglob('*') if p.is_file()]) != len(records):
            raise RuntimeError(f'Unexpected file count: {folder}')
    for platform in ('pc', 'android'):
        total = sum(r[platform]['bytes'] for r in records)
        print(f'{platform}: {len(records)} resources, {total} bytes', flush=True)
    print(f'Output: {output}', flush=True)


if __name__ == '__main__':
    main()
