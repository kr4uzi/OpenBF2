#!/usr/bin/env python3
"""[TEMP] Headless request routing with real headers and verbatim production bodies.

Synthetic squads, game transport and commander assets observe the selected boundary;
this does not start a client/server or validate asset creation in a loaded level.
Run on macOS after configuring: python3 Code/Tests/commander_requests_runtime.py BUILD_DIR
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BF2 = ROOT / 'Code/BF2'
AI = 'AI/AIDLL/AI/'
spec = importlib.util.spec_from_file_location('helpers', ROOT / 'Code/Tests/aidll/sai_deny_runtime.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)


def body(relative, signature):
    source = (BF2 / relative).read_text()
    matches = re.findall(r'^' + signature + r'[^\{]*\{.*?^}', source, re.M | re.S)
    if len(matches) != 1:
        raise RuntimeError((relative, signature, len(matches)))
    return matches[0]


def generate(out):
    selected = [
        ('Game/GameLogic/SquadManager/SquadManager.cpp', r'void SquadManager::issueRequest\('),
        ('Game/GameLogic/SquadManager/Squad.cpp', r'void Squad::issueRequest\('),
    ]
    includes = ['Game/GameLogic/SquadManager/SquadManager.h', 'Game/GameLogic/SquadManager/Squad.h',
                'Game/GameEvents/IssueSquadOrderEvent.h', 'Game/MapInfo.h', 'Game/IGame.h', 'Core/EventManager.h',
                'Scene/Object/PlayerManager.h',
                'Core/SettingsRepostitory.h', 'Core/StringUtil.h', 'AI/AIInterface/AIInstance.h',
                AI + 'Main/AIEventCourier.h', AI + 'Main/AIMain.h', AI + 'Main/AIRandom.h',
                AI + 'Main/CommonSingletonServer.h', AI + 'AIStrategic/SAI.h',
                AI + 'AIBot/SquadManager.h', 'AI/AIInterface/BFKnowledge/BFTimer.h',
                'AI/AIInterface/BFKnowledge/BFCommander.h']
    header = ['// [TEMP] Real declarations and typed synthetic collaborators.', '#pragma once',
              '#include <cstdio>', '#include <cstdlib>', '#include <cstring>']
    header += ['#include "' + str(BF2 / p) + '"' for p in includes]
    header += ['using namespace dice::hfe;', 'using namespace dice::hfe::ai;',
               '[[noreturn]] void unexpected(const char* name);',
               'void broadcast(GameEvent*);', 'void remote(int, GameEvent*);',
               'SAI* activeSAI(int);', 'double now();',
               'void useAsset(int, int, int, Vec2);', 'bool artilleryAvailable();',
               'const std::string& currentGameMode();']
    specs = [
        ('IGame', 'Game/IGame.h', 'ObservedGame', {
            'getCurrentMapGameMode': 'return currentGameMode();',
            'sendGameEventToAllConnected': 'broadcast(event);',
            'sendGameEventIfConnected': 'remote(playerId, event);'}),
        ('IAIMain', AI + 'Main/AIMain.h', 'ObservedMain', {'getSAI': 'return activeSAI(side);'}),
        ('IBFTimer', 'AI/AIInterface/BFKnowledge/BFTimer.h', 'ObservedTimer', {'getGameTime': 'return now();'}),
        ('IBFCommander', 'AI/AIInterface/BFKnowledge/BFCommander.h', 'ObservedCommander', {
            'fireArtillery': 'useAsset(0, a, b, position); return 0;',
            'sendUav': 'useAsset(1, a, b, position);',
            'dropSupply': 'useAsset(3, a, b, position);',
            'dropVehicle': 'useAsset(2, a, b, position);',
            'gameHasArtillery': 'return artilleryAvailable();',
            'anyArtilleryPiecesLeft': 'return artilleryAvailable();'}),
    ]
    # Keep the original parameter names except the two transport declarations, whose
    # headers leave their arguments unnamed.
    signatures = {
        'sendGameEventToAllConnected': 'void sendGameEventToAllConnected(GameEvent* event)',
        'sendGameEventIfConnected': 'void sendGameEventIfConnected(unsigned int playerId, GameEvent* event)',
        'getSAI': 'SAI* getSAI(int side)',
    }
    for base, relative, name, methods in specs:
        header += ['struct ' + name + ' : ' + base + ' {']
        decls = list(helpers.methods(base, relative))
        for method, declaration in decls:
            declaration = signatures.get(method, declaration)
            implementation = methods.get(method, 'unexpected("' + base + '::' + method + '");')
            header += [declaration + ' override { ' + implementation + ' }']
        header += ['};']
    (out / 'request_observers.h').write_text('\n'.join(header) + '\n')
    actual = ['// [TEMP] Verbatim production bodies; no statement substitution.',
              '#include "request_observers.h"', 'namespace dice { namespace hfe {']
    actual += [body(*s) for s in selected]
    actual += ['namespace ai {']
    for name in ['Artillery', 'Uav', 'Supply', 'VehicleDrop']:
        actual += [body(AI + 'Main/AIEventCourier.cpp',
                        r'int AIEventCourier::event_received' + name + r'Request\(')]
    for name in ['Artillery', 'Uav', 'Supply', 'VehicleDrop']:
        actual += [body(AI + 'AIStrategic/SAI.cpp', r'bool SAI::playerRequests' + name + r'\(')]
    actual += [body(AI + 'AIStrategic/SAI.cpp', r'void SAI::setPlayerIsCommander\(')]
    actual += ['} } }']
    (out / 'request_actual.cpp').write_text('\n'.join(actual) + '\n')


def run(command, cwd=None):
    result = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(shlex.join(map(str, command)) + '\n' + result.stdout[-12000:])
    return result.stdout


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('build', type=Path)
    a = p.parse_args()
    build = a.build.resolve()
    entry = next(e for e in json.loads((build / 'compile_commands.json').read_text())
                 if e['file'].endswith('/Game/GameLogic/SquadManager/SquadManager.cpp')
                 and 'bf2_engine_objects' in e.get('command', ''))
    args = shlex.split(entry['command'])
    compiler, flags, skip = args[0], [], False
    for arg in args[1:]:
        if skip:
            skip = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif arg not in ('-c', '-MD', '-MMD', entry['file']) and not arg.startswith('-DDEDICATED='):
            flags.append(arg)
    for image in (0, 1):
        out = build / ('commander-requests-' + str(image))
        out.mkdir(exist_ok=True)
        generate(out)
        sources = [out / 'request_actual.cpp', ROOT / 'Code/Tests/commander_requests_runtime.cpp',
                   BF2 / 'AI/AIInterface/Main/AIEventListener.cpp', BF2 / 'Game/Common/GameEvent.cpp',
                   BF2 / 'Game/GameEvents/IssueSquadOrderEvent.cpp', BF2 / 'Game/GameEvents/UAVEvent.cpp',
                   BF2 / 'Game/GameEvents/ArtilleryEvent.cpp', BF2 / 'Game/GameEvents/SupplyDropEvent.cpp',
                   BF2 / 'Game/GameEvents/DropVehicleEvent.cpp',
                   BF2 / 'Core/SettingsRepostitory.cpp', BF2 / 'Core/StringUtil.cpp',
                   BF2 / (AI + 'Main/AIRandom.cpp')]
        objects = []
        compile_flags = flags + ['-DDEDICATED=' + str(image), '-I' + str(out)]
        for index, source in enumerate(sources):
            obj = out / (str(index) + '.o')
            run([compiler] + compile_flags + ['-c', str(source), '-o', str(obj)], entry['directory'])
            objects.append(obj)
        stubs = out / 'unused.cpp'
        run(['python3', '-I', str(ROOT / 'tools/macos/link_stubs.py'), str(stubs)] + list(map(str, objects)))
        unused = out / 'unused.o'
        run([compiler] + compile_flags + ['-c', str(stubs), '-o', str(unused)], entry['directory'])
        exe = out / 'check'
        run([compiler, '-arch', 'x86_64', '-Wl,-dead_strip', '-o', str(exe)] + list(map(str, objects)) + [str(unused)])
        output = run([str(exe)])
        (out / 'result.txt').write_text(output)
        print('DEDICATED=' + str(image) + ': ' + output.strip())


if __name__ == '__main__':
    main()
