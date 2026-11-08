#!/usr/bin/env python3
"""Export TEXTURES and LEVELS images with paths, alpha and original mip levels."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data_root', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tool', required=True, type=Path)
    args = parser.parse_args()
    root = args.data_root.resolve()
    output = args.output.resolve()
    if output.exists() or output.is_relative_to(root):
        raise RuntimeError('Output must be a new directory outside game data')
    sources = sorted(p for folder in ('TEXTURES', 'LEVELS') for p in (root / folder).rglob('*')
                     if p.is_file() and p.suffix.upper() in {'.XBMP', '.ENVMAP'})
    output.mkdir(parents=True)
    manifest = {'version': 1, 'source_root': str(root), 'resources': []}
    jobs = []
    with tempfile.TemporaryDirectory(prefix='a51-image-export-') as temp:
        work = Path(temp)
        for index, source in enumerate(sources):
            relative = source.relative_to(root)
            entry = {'source': str(relative), 'sha256': digest(source), 'images': []}
            manifest['resources'].append(entry)
            if source.suffix.upper() == '.XBMP':
                jobs.append((source, output / relative.with_suffix('.tga'), entry, None))
                continue
            data = source.read_bytes()
            offset = 0
            for face in range(6):
                if offset + 32 > len(data):
                    raise RuntimeError(f'Truncated cubemap: {source}')
                size, clut = struct.unpack_from('<ii', data, offset)
                end = offset + 32 + size + clut
                if size <= 0 or clut < 0 or end > len(data):
                    raise RuntimeError(f'Invalid cubemap face: {source}')
                temporary = work / f'cube_{index}_face_{face}.XBMP'
                temporary.write_bytes(data[offset:end])
                jobs.append((temporary, output / relative / f'face_{face}.tga', entry, face))
                offset = end
            if offset != len(data):
                raise RuntimeError(f'Unexpected trailing cubemap data: {source}')
        job_file = work / 'jobs.tsv'
        job_file.write_text(''.join(f'{source}\t{target}\n' for source, target, _, _ in jobs))
        report = work / 'report.tsv'
        subprocess.run([str(args.tool.resolve()), str(job_file), str(report)], check=True)
        results = report.read_text().splitlines()
        if len(results) != len(jobs):
            raise RuntimeError('Incomplete export report')
        for (_, target, entry, face), line in zip(jobs, results):
            fields = line.split('\t')
            if fields[0] != str(target):
                raise RuntimeError('Unexpected export order')
            width, height, fmt, flags, count, alpha = map(int, fields[1:])
            image = {'width': width, 'height': height, 'source_format': fmt,
                     'source_flags': flags, 'has_alpha': bool(alpha), 'mips': []}
            if face is not None:
                image['face'] = face
            entry['images'].append(image)
            for mip in range(count):
                path = target if mip == 0 else target.parent / (target.stem + '.mips') / f'mip_{mip}.tga'
                content = path.read_bytes()
                if len(content) < 18 or content[2] != 2 or content[16:18] != b'\x20\x28':
                    raise RuntimeError(f'Unexpected TGA encoding: {path}')
                mip_width, mip_height = struct.unpack_from('<HH', content, 12)
                if (mip_width, mip_height) != (max(1, width >> mip), max(1, height >> mip)):
                    raise RuntimeError(f'Wrong mip dimensions: {path}')
                if len(content) != 18 + mip_width * mip_height * 4:
                    raise RuntimeError(f'Truncated TGA: {path}')
                image['mips'].append({'level': mip, 'image': str(path.relative_to(output)),
                                      'width': mip_width, 'height': mip_height,
                                      'sha256': hashlib.sha256(content).hexdigest()})
    for entry in manifest['resources']:
        if digest(root / entry['source']) != entry['sha256']:
            raise RuntimeError(f'Source changed: {entry["source"]}')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'{len(sources)} source resources exported to {output}')
    print(f'{sum(len(image["mips"]) for entry in manifest["resources"] for image in entry["images"])} TGA images, including mip levels and cubemap faces')


if __name__ == '__main__':
    main()
