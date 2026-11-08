#!/usr/bin/env python3
"""Check Support audio calls for synchronous service requests."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/')
CALL = re.compile(r'\bg_AudioMgr\s*\.\s*(\w+)\s*\(')
DEFINITION = re.compile(r'^[^\n;{}()]*?\b(\w+::[~\w]+)\s*\(', re.MULTILINE)
SYNCHRONOUS = {
    'IsValidVoiceId', 'IsVoiceReady', 'IsReleasing',
    'GetVoiceState', 'GetLengthSeconds', 'GetVoiceDescriptor', 'GetVolume',
    'GetPan', 'GetPitch', 'GetPosition', 'GetEffectSend', 'HasLipSync',
    'GetLipSync', 'GetBreakPoints', 'GetCurrentPlayTime', 'GetPriority',
    'CreateEar', 'GetFirstEar', 'GetNextEar', 'LoadPackage', 'UnloadPackage',
    'ReserveStreams', 'UnReserveStreams', 'GetDebugStats',
}
LIFECYCLE = {
    ('Support/AudioMgr/AudioMgr.cpp', 'audiopkg_loader::PreLoad', 'LoadPackage'),
    ('Support/AudioMgr/AudioMgr.cpp', 'audiopkg_loader::Unload', 'UnloadPackage'),
    ('Support/Objects/Player/PlayerObject.cpp', 'player::player', 'CreateEar'),
    ('xCore/Entropy/Audio/audio_stream_controller.cpp', 'audio_stream_controller::Kill', 'UnReserveStreams'),
    ('xCore/Entropy/Audio/audio_stream_controller.cpp', 'audio_stream_controller::ReserveStreams', 'ReserveStreams'),
    ('xCore/Entropy/Audio/audio_stream_controller.cpp', 'audio_stream_controller::ReserveStreams', 'UnReserveStreams'),
}


def remove_comments(source):
    return TOKENS.sub(lambda m: re.sub(r'[^\n]', ' ', m[0])
                      if m[0].startswith('/') else m[0], source)


def arguments(source, start):
    depth = 1
    quote = None
    end = start
    while end < len(source) and depth:
        char = source[end]
        if quote:
            if char == '\\':
                end += 1
            elif char == quote:
                quote = None
        elif char in '\"\'':
            quote = char
        elif char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
        end += 1
    return source[start:end - 1].strip()


def check(path):
    source = remove_comments(path.read_text(encoding='utf-8-sig', errors='replace'))
    relative = path.relative_to(ROOT).as_posix()
    code = TOKENS.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]), source)
    definitions = list(DEFINITION.finditer(code))
    errors = []
    for call in CALL.finditer(code):
        method = call[1]
        if method not in SYNCHRONOUS:
            continue
        owner = next((d[1] for d in reversed(definitions) if d.start() < call.start()), '')
        if (relative, owner, method) in LIFECYCLE:
            continue
        args = arguments(source, call.end())
        if method == 'GetLengthSeconds':
            if args.startswith('"') or (relative == 'Support/Objects/Cinema.cpp' and args == 'm_Descriptor'):
                continue
        if method == 'GetPriority' and relative == 'xCore/Entropy/Audio/audio_stream_controller.cpp' and args == 'pIdentifier':
            continue
        if method == 'GetPriority' and relative == 'Support/Characters/Character.cpp':
            if args in {'dialogName', 'Descriptor'}:
                continue
        line = source.count('\n', 0, call.start()) + 1
        errors.append(f'{relative}:{line}: synchronous audio call {method} in {owner}')
    return errors


def check_playback_implementation():
    path = ROOT / 'xCore/Entropy/Audio/audio_mgr.cpp'
    source = remove_comments(path.read_text())
    code = TOKENS.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]), source)
    errors = []
    queued = {'audio_mgr::Play', 'audio_mgr::PlayInternal', 'audio_mgr::PlayConfigured',
              'audio_mgr::Start', 'audio_mgr::Segue', 'audio_mgr::GetIsReady', 'audio_mgr::ResumeAll', 'audio_mgr::PauseAll', 'audio_mgr::ReleaseAll'}
    for definition in DEFINITION.finditer(code):
        if definition[1] not in queued:
            continue
        start = code.index('{', definition.end())
        end = start + 1
        depth = 1
        while depth:
            depth += (code[end] == '{') - (code[end] == '}')
            end += 1
        body = code[start:end]
        if re.search(r'\bSubmit\w*Sync\s*\(', body) or 'PlayInternalNow' in body or 'x_DelayThread' in body:
            errors.append(f'{path.relative_to(ROOT)}: blocking playback implementation in {definition[1]}')
    return errors


def main():
    paths = sorted(p for p in (ROOT / 'Support').rglob('*')
                   if p.suffix.lower() in {'.cpp', '.hpp', '.h', '.inl'})
    paths.append(ROOT / 'xCore/Entropy/Audio/audio_stream_controller.cpp')
    errors = [error for path in paths for error in check(path)]
    errors.extend(check_playback_implementation())
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print('PASS: no synchronous audio service calls outside the lifecycle allowlist')
    return 0


if __name__ == '__main__':
    sys.exit(main())
