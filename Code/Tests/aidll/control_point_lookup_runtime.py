"""[TEMP] Generate exact BFGame lookup bodies with typed, aborting collaborators."""
from pathlib import Path
import json
import re
import sys

sys.dont_write_bytecode = True
from bf_game_counts_runtime import extract, virtuals


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    header = Path(__file__).with_name('bf_game_counter_runtime.h').as_posix()
    actual = ['// [TEMP] Production lookup definitions, compiled against real headers.',
              '#include "' + header + '"',
              '#include "' + (Path(__file__).resolve().parents[2] /
                             'BF2/Game/Objects/ControlPointTemplate.h').as_posix() + '"',
              'namespace dice { namespace hfe { namespace ai {']
    records = []
    tested = ('getOwnerOfControlPoint', 'getControlPointTakeOverCounter', 'getControlPointInfo')
    for method in tested:
        body, record = extract('AI/AIInterface/BFKnowledge/BFGame.cpp',
                               r'^(?:int|float|bool) BFGame::' + method + r'\([^\n]*\n\{.*?^}')
        actual.append(body)
        records.append(record)
    actual.append('} } }')
    (out / 'lookup_actual.cpp').write_text('\n'.join(actual) + '\n')
    (out / 'lookup_source.json').write_text(json.dumps(records, indent=2) + '\n')

    mocks = ['// [TEMP] Unused virtuals stop; tested methods are defined only by production text.',
             'namespace dice { namespace hfe { namespace ai {']
    for name, declaration, _ in virtuals('BFGame', 'AI/AIInterface/BFKnowledge/BFGame.h'):
        if name not in tested:
            signature = re.sub(r'\b' + name + r'(?=\s*\()', 'BFGame::' + name, declaration, count=1)
            mocks.append(signature + ' { unexpected("BFGame::' + name + '"); }')
    mocks.append('} namespace world {')
    for owner, relative in [('IObject', 'Game/Objects/IObject.h'), ('Object', 'Game/Objects/Object.h'),
                            ('SimpleObject', 'Game/Objects/Base/SimpleObject.h'),
                            ('Bundle', 'Game/Objects/Base/Bundle.h'),
                            ('ControlPoint', 'Game/Objects/Game/ControlPoint.h')]:
        for name, declaration, pure in virtuals(owner, relative):
            if not pure:
                signature = re.sub(r'\b' + name + r'(?=\s*\()', owner + '::' + name, declaration, count=1)
                mocks.append(signature + ' { unexpected("' + owner + '::' + name + '"); }')
    mocks.append('} class LookupLogic : public IGameLogic { public: '
                 'std::map<unsigned int, world::IObject*> points; mutable int calls = 0; '
                 'std::map<unsigned int, world::IObject*>* getControlPointMap() const override '
                 '{ ++calls; return const_cast<std::map<unsigned int, world::IObject*>*>(&points); }')
    for name, declaration, _ in virtuals('IGameLogic', 'Game/IGameLogic.h'):
        if name != 'getControlPointMap':
            mocks.append(declaration + ' override { unexpected("IGameLogic::' + name + '"); }')
    mocks.append('}; } }')
    (out / 'lookup_collaborators.inl').write_text('\n'.join(mocks) + '\n')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] != 'generate':
        raise SystemExit('Usage: control_point_lookup_runtime.py generate OUTPUT')
    generate(Path(sys.argv[2]).resolve())
