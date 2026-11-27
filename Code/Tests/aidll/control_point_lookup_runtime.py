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
                            ('ControlPoint', 'Game/Objects/Game/ControlPoint.h'),
                            ('IObjectTemplate', 'Game/Objects/IObjectTemplate.h'),
                            ('ObjectTemplate', 'Game/Objects/ObjectTemplate.h'),
                            ('SimpleObjectTemplate', 'Game/Objects/Base/SimpleObjectTemplate.h'),
                            ('BundleTemplate', 'Game/Objects/Base/BundleTemplate.h'),
                            ('ControlPointTemplate', 'Game/Objects/ControlPointTemplate.h')]:
        for name, declaration, pure in virtuals(owner, relative):
            if not pure:
                signature = re.sub(r'\b' + name + r'(?=\s*\()', owner + '::' + name, declaration, count=1)
                for nested in ('ComponentMap', 'TemplateInfo'):
                    signature = re.sub(r'^' + nested + r'\b', owner + '::' + nested, signature)
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

    ai = 'AI/AIDLL/AI/'
    header = Path(__file__).with_name('control_point_area_runtime.h').as_posix()
    areas = ['// [TEMP] Exact production definitions with native typed collaborators.',
             '#include "' + header + '"',
             'namespace dice { namespace hfe { namespace ai {']
    area_records = []
    for owner, relative, methods in [
        ('AIStrategicArea', ai + 'AIStrategicEntity/AIStrategicArea.cpp',
         [r'AIStrategicArea::AIStrategicArea\(const std::string& name, unsigned int',
          r'AIStrategicArea::~AIStrategicArea\(', r'bool AIStrategicArea::getControlPointPosAndRadius\(']),
        ('AIStrategicObjectManager', ai + 'AIStrategicEntity/AIStrategicObjectManager.cpp',
         [r'AIStrategicObjectManager::AIStrategicObjectManager\(',
          r'AIStrategicObjectManager::~AIStrategicObjectManager\(',
          r'void AIStrategicObjectManager::postLoadInitAreas\(',
          r'const std::vector<SAWayPoint\*>& AIStrategicObjectManager::getWaypoints\(',
          r'AIStrategicArea\* AIStrategicObjectManager::getActiveArea\(',
          r'AIStrategicObjectManager::WPContainer::WPContainer\(\)',
          r'AIStrategicObjectManager::WPContainer::WPContainer\(const',
          r'AIStrategicObjectManager::WPContainer::~WPContainer\(',
          r'std::vector<SAWayPoint\*>& AIStrategicObjectManager::WPContainer::getWayPoints\(',
          r'const std::vector<SAWayPoint\*>& AIStrategicObjectManager::WPContainer::getWayPoints\(',
          r'const std::vector<AIStrategicObject\*>& AIStrategicObjectManager::getStrategicObjects\(',
          r'AIStrategicObject\* AIStrategicObjectManager::getStrategicObject\(',
          r'const std::map<std::string, AIStrategicArea\*>& AIStrategicObjectManager::getStrategicAreas\(',
          r'AIStrategicObjectManager\* AIStrategicObjectManager::getInstance\(',
          r'AIStrategicObject\* AIStrategicObjectManager::getClosestObject\(\s*const Vec3& position, int side\) const']),
        ('SAI', ai + 'AIStrategic/SAI.cpp', [r'void SAI::update\(',
                                          r'bool SAI::isPosInsideOwnedControlPoint\('])]:
        source = (Path(__file__).resolve().parents[2] / 'BF2' / relative).read_text()
        for pattern in methods:
            # Overloads are all included; extraction ends only at a column-zero closing brace.
            bodies = re.findall('^' + pattern + r'[^\{]*\{.*?^}', source, re.M | re.S)
            if not bodies:
                raise RuntimeError('Missing production method: ' + pattern)
            for body in bodies:
                areas.append(body)
                area_records.append(body)
    areas += ['AIStrategicObjectManager* AIStrategicObjectManager::m_instance = nullptr;', '} } }']
    (out / 'areas_actual.cpp').write_text('\n'.join(areas) + '\n')
    (out / 'areas_source.json').write_text(json.dumps(area_records, indent=2) + '\n')
    mocks = ['// [TEMP] Unexercised AI virtuals abort.', 'namespace dice { namespace hfe { namespace ai {']
    for owner in ('AIStrategicObject', 'AIStrategicArea'):
        for name, declaration, pure in virtuals(owner, ai + 'AIStrategicEntity/' + owner + '.h'):
            if not pure:
                signature = re.sub(r'\b' + name + r'(?=\s*\()', owner + '::' + name, declaration, count=1)
                mocks.append(signature + ' { unexpected("' + owner + '::' + name + '"); }')
    for name in ('reset', 'resetAreas'):
        mocks.append('bool AIStrategicObjectManager::' + name + '() { unexpected("manager reset"); }')
    for name, declaration, _ in virtuals('SAI', ai + 'AIStrategic/SAI.h'):
        if name not in ('update', 'isPosInsideOwnedControlPoint'):
            signature = re.sub(r'\b' + name + r'(?=\s*\()', 'SAI::' + name, declaration, count=1)
            mocks.append(signature + ' { unexpected("SAI::' + name + '"); }')
    for method in ('updateStrengths', 'prepareUpdate', 'chooseStrategy', 'modifyStrategicObjectsTemperatures',
                   'updateBotPositions', 'updateDistribution', 'distributeResources', 'distributeDefence',
                   'distributeAttack', 'distributeArtillery', 'distributeWithNoAreas'):
        mocks.append('void SAI::' + method + '() { unexpected("SAI::' + method + '"); }')
    mocks += ['void SAI::clearStates(float*, float*) { unexpected("clearStates"); }',
              'void SAI::updateStates(float*, float*) { unexpected("updateStates"); }',
              'bool SAI::updateStrategicObjects() { unexpected("updateStrategicObjects"); }']
    for base, relative, name, methods in [
        ('IAIPathfinding', ai + 'Main/IAIPathfinding.h', 'AreaPathfinding',
         {'getNumberOfMaps': 'return 2;'}),
        ('IAIGame', 'AI/AIInterface/BFKnowledge/BFGame.h', 'AreaGame',
         {'getControlPointInfo': 'position = {}; radius = 0.0f; active = false; '
          'if (index != 101 && index != 103) return false; '
          'position = {10.0f, 20.0f, 30.0f}; radius = 50.0f; active = index == 103; return true;'})]:
        mocks.append('class ' + name + ' : public ' + base + ' { public:')
        for method, declaration, _ in virtuals(base, relative):
            body = methods.get(method, 'unexpected("' + base + '::' + method + '");')
            mocks.append(declaration + ' override { ' + body + ' }')
        mocks.append('};')
    mocks.append('} } }')
    (out / 'area_collaborators.inl').write_text('\n'.join(mocks) + '\n')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] != 'generate':
        raise SystemExit('Usage: control_point_lookup_runtime.py generate OUTPUT')
    generate(Path(sys.argv[2]).resolve())
