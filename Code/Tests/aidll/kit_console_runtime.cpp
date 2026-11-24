// [TEMP] Native chosen ClassManager/OldConsole lifetimes supply only singleton
// publication and the current source filename. Actual bindings, enum parsers,
// console registry/base, manager and template implementations are unmodified.
// This does not run the script interpreter, module lifecycle or full BFConsole.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <windows.h>
#include <crtdbg.h>
#include "../../BF2/Core/ClassManager.h"
#include "../../BF2/Core/ConsoleObjects.h"
#include "../../BF2/Core/ConsoleObjectBase.h"
#include "../../BF2/IO/Console/Console.h"
#include "../../BF2/AI/AIInterface/Module/BFConsole.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/KitTemplate.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/KitTemplateManager.h"
#include "../../BF2/AI/AIInterface/SingletonServer.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"

using namespace dice::hfe;
using namespace dice::hfe::ai;
static unsigned checks;
static void check(bool pass, const char* message) {
    ++checks;
    if (!pass) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(2); }
}
[[noreturn]] static void unexpected(const char* method) {
    std::fprintf(stderr, "UNEXPECTED %s\n", method); std::abort();
}
// [TEMP]
// AI object runtime is omitted; unexpected calls reach the shared aborting link seam.
#include "ai_runtime_link_seam.inl"
static void* registeredKit;
static std::string workingFile = "Kits/ai/Objects.ai";
namespace dice { namespace hfe {
ClassManager* classManager;
ClassManager::ClassManager() {}
ClassManager::~ClassManager() {}
void ClassManager::registerSimpleSingleton(const std::string& name, void* value) {
    check(name == "IKitTemplateManager", "actual manager publication name"); registeredKit = value;
}
void ClassManager::unregisterSimpleSingleton(const std::string& name) {
    check(name == "IKitTemplateManager", "actual manager unpublication name"); registeredKit = nullptr;
}
namespace io {
OldConsole* mainConsole;
OldConsole::Level::Level() {}
OldConsole::OldConsole() {}
OldConsole::~OldConsole() {}
const std::string& OldConsole::getWorkingFile() const { return workingFile; }
}
namespace ai {
void* CommonSingletonServer::m_iKitTemplateManager;
// Unused singleton references remain null.
void* CommonSingletonServer::m_iaiSettings;
void* CommonSingletonServer::m_iaiPathfindingCourier;
void* CommonSingletonServer::m_iaiGame;
void BFSingletonServer::updateStaticMethods() {
    CommonSingletonServer::m_iKitTemplateManager = static_cast<IKitTemplateManager*>(registeredKit);
}
enum CombatStrengthType : int;
std::istream& operator>>(std::istream&, KitType&);
std::istream& operator>>(std::istream&, CombatStrengthType&);
} } }
#include "kit_collaborators.inl"
#include "stock_commands.inl"

// Find kit bindings by both object and function name; other objects also expose create.
static ConsoleObject* row(const char* name) {
    for (ConsoleObject* object : kitTemplateConsoleObjects())
        if (std::strcmp(object->getObjectName(), "kitTemplate") == 0 && std::strcmp(object->getFunctionName(), name) == 0)
            return object;
    unexpected(name);
}
static unsigned kitRows() {
    unsigned count = 0;
    for (ConsoleObject* object : kitTemplateConsoleObjects())
        if (std::strcmp(object->getObjectName(), "kitTemplate") == 0) ++count;
    return count;
}
static std::string execute(const char* name, const char* first, const char* second) {
    ConsoleObject* object = mainConsoleObjects->getConsoleObject("kitTemplate", name);
    check(object != nullptr, "active registered command resolved");
    std::string args[] = {first, second};
    return object->execute(args, 2);
}
static void parserKit(const std::string& token, int expected) {
    KitType value = static_cast<KitType>(99);
    std::istringstream stream(token);
    check(&(dice::hfe::ai::operator>>(stream, value)) == &stream, "kit parser returns input stream");
    check(static_cast<int>(value) == expected, "kit parser value");
}
static void parserCombat(const std::string& token, int expected) {
    CombatStrengthType value = static_cast<CombatStrengthType>(99);
    std::istringstream stream(token);
    check(&(dice::hfe::ai::operator>>(stream, value)) == &stream, "combat parser returns input stream");
    check(static_cast<int>(value) == expected, "combat parser value");
}
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) return 2;
    const std::string mode(argv[1]);
    mainConsoleObjects->registerConsoleObjects(kitTemplateConsoleObjects());
    check(kitRows() == 4 && kitTemplateConsoleObjects().size() == 101, "exact recovered subset (4 kit rows of 101)");
    if (mode == "metadata") {
        const char* names[] = {"create", "setStrategicStrength", "setBattleStrength"};
        const char* args0[] = {"std::string", "int", "CombatStrengthType"};
        const char* args1[] = {"KitType", "int", "int"};
        for (int i = 0; i < 3; ++i) {
            ConsoleObject* p = row(names[i]);
            check(std::strcmp(p->getObjectName(), "kitTemplate") == 0, "object name");
            check(p->getType() == 0 && p->getAccess() == 2 && p->getCategory() == 0, "access/type/category");
            check(p->getMinNoArgs() == 2 && p->getMaxNoArgs() == 2, "exact arity");
            check(p->isDynamic() == (i == 0) && !p->hasReturnValue(), "dynamic and initial return flag");
            check(std::strcmp(p->getArgType(0), args0[i]) == 0 && std::strcmp(p->getArgType(1), args1[i]) == 0, "argument metadata");
            check(std::strcmp(p->getReturnType(), i ? "void" : "std::string") == 0, "return metadata");
            check(!p->hasRange(), "unranged command");
        }
        ConsoleObject* type = row("getType");
        check(type->getType() == 0 && type->getAccess() == 2 && type->getCategory() == 0,
              "getType access/type/category");
        check(type->getMinNoArgs() == 0 && type->getMaxNoArgs() == 0 && type->isDynamic(),
              "getType zero-argument dynamic method");
        check(std::strcmp(type->getReturnType(), "KitType") == 0, "getType named return type");
    } else if (mode == "kit_parser") {
        const char* suffix[] = {"Specops", "Sniper", "Assault", "Support", "Engineer", "Medic", "AT", "Extended1", "Extended2", "Extended3", "Extended4", "Extended5", "Extended6"};
        for (int i = 0; i < 13; ++i) {
            parserKit(std::string("KT") + suffix[i], i); parserKit(suffix[i], i); parserKit(std::to_string(i), i);
        }
        for (const char* s : {"", "13", "-1", "00", "6.0", "specops", "kTMedic", "unknown"}) parserKit(s, 13);
        parserKit("  Medic trailing", 5);
    } else if (mode == "combat_parser") {
        const char* suffix[] = {"Infantry", "LightArmour", "HeavyArmour", "NavalArmour", "Helicopter", "AirPlane"};
        const char* mixed[] = {"iNFANTRY", "lIGHTaRMOUR", "hEAVYaRMOUR", "nAVALaRMOUR", "hELICOPTER", "aIRpLANE"};
        for (int i = 0; i < 6; ++i) {
            parserCombat(std::string("CST") + suffix[i], i); parserCombat(suffix[i], i);
            parserCombat(std::to_string(i), i); parserCombat(mixed[i], i);
        }
        parserCombat("CSTInfantery", 0); parserCombat("INFANTERY", 0); parserCombat(" Infantry tail", 0);
        for (const char* s : {"", "-1", "6", "7", "0.0", "cstInfantry", "CSTNumberOfTypes", "All", "garbage"}) parserCombat(s, 6);
    } else {
        check(!row("create")->isActive() && !row("setBattleStrength")->isActive()
              && !row("getType")->isActive(), "null manager guards");
        check(!mainConsoleObjects->getConsoleObject("kitTemplate", "create"), "inactive registry refusal");
        ClassManager classes; classManager = &classes;
        dice::hfe::io::OldConsole console; dice::hfe::io::mainConsole = &console;
        {
            KitTemplateManager manager;
            check(row("create")->isActive() && !row("setBattleStrength")->isActive(), "empty manager guards");
            if (mode == "stock") {
                for (const char* line : stockCommands) {
                    std::istringstream parse(line); std::string command, a, b; parse >> command >> a >> b;
                    execute(command.substr(command.find('.') + 1).c_str(), a.c_str(), b.c_str());
                }
                const char* names[] = {"Specops", "Sniper", "Assault", "Support", "Engineer", "Medic", "AT"};
                const int expected[7][8] = {{2,2,6,3,3,0,2,1},{2,1,3,2,0,0,0,0},{2,3,8,2,1,0,2,2},{2,3,12,5,0,0,3,2},{1,2,4,2,0,0,0,0},{1,2,6,3,1,0,0,0},{2,2,2,8,4,0,0,0}};
                const auto& all = manager.getAllTemplates(); check(all.size() == 13, "retail type vector size");
                for (int i = 0; i < 7; ++i) {
                    KitTemplate* kit = manager.getTemplate(std::string(names[i]));
                    check(kit && all[i] == kit && static_cast<int>(kit->getType()) == i, "seven stock kits populated");
                    for (int j = 0; j < 2; ++j) check(kit->getStrategicStrength(j) == expected[i][j], "stock strategic strength");
                    for (int j = 0; j < 6; ++j) check(kit->getBattleStrength(j) == expected[i][j+2], "stock battle strength");
                    check(manager.getWorkingFileAndPath(kit) == workingFile, "stock metadata");
                }
                for (int i = 7; i < 13; ++i) check(!all[i], "unconfigured extended types stay null");
            } else if (mode == "arity") {
                std::string args[] = {"Bad", "Medic", "ignored"};
                row("create")->execute(args, 1); row("create")->execute(args, 3);
                check(!manager.getActiveTemplate(), "wrong create argc has no effect");
                execute("create", "Good", "Medic");
                std::string strengths[] = {"0", "17", "ignored"};
                row("setStrategicStrength")->execute(strengths, 1); row("setStrategicStrength")->execute(strengths, 3);
                check(manager.getActiveTemplate()->getStrategicStrength(0) == 0, "wrong setter argc has no effect");
            } else {
                check(execute("create", "A kit with spaces", "KTMedic") == "KitTemplate 'A kit with spaces' successfully created", "create result and raw string");
                KitTemplate* first = manager.getActiveTemplate();
                check(first && static_cast<int>(first->getType()) == 5, "typed real kit");
                if (mode == "availability") {
                    check(row("setStrategicStrength")->isActive(), "selected receiver active");
                    manager.setActive("missing"); check(!row("setStrategicStrength")->isActive(), "missing active refused");
                    manager.setActive("A kit with spaces"); check(row("setStrategicStrength")->isActive(), "restored active");
                } else if (mode == "creation") {
                    check(execute("create", "Empty type", "") == "Error: Template 'Empty type' is of type 'KTNoKit'", "failed enum extraction overwrites previous valid static value");
                    check(manager.getActiveTemplate() == first && !manager.getTemplate(std::string("Empty type")), "empty enum refusal preserves active and creates nothing");
                    check(execute("create", "Invalid", "medic") == "Error: Template 'Invalid' is of type 'KTNoKit'", "invalid enum real refusal");
                    check(manager.getActiveTemplate() == first, "invalid type preserves active");
                    check(execute("create", "A kit with spaces", "0") == "Error: Template 'A kit with spaces' already exists", "duplicate real refusal");
                    check(!manager.getActiveTemplate() && !row("setBattleStrength")->isActive(), "duplicate clears active");
                } else if (mode == "strengths") {
                    check(execute("setBattleStrength", "iNFANTRY", "6.0").empty(), "void setter output");
                    execute("setBattleStrength", "CSTAirPlane", "-7.75");
                    execute("setStrategicStrength", "1", "2147483647");
                    check(first->getBattleStrength(0) == 6 && first->getBattleStrength(5) == -7, "retail integer prefix parsing");
                    check(first->getStrategicStrength(1) == 2147483647 && !row("setBattleStrength")->hasReturnValue(), "full signed int and void flag");
                    execute("setStrategicStrength", "0", "");
                    check(first->getStrategicStrength(0) == 2147483647, "numeric failure still retains previous value");
                    execute("create", "Second", "Specops"); execute("setBattleStrength", "Infantery", "19");
                    check(first->getBattleStrength(0) == 6 && manager.getActiveTemplate()->getBattleStrength(0) == 19, "fresh active target");
                } else unexpected(mode.c_str());
            }
        }
        check(!KitTemplateManager::m_instance && !CommonSingletonServer::m_iKitTemplateManager, "real manager teardown");
    }
    mainConsoleObjects->unregisterConsoleObjects(kitTemplateConsoleObjects());
    check(mainConsoleObjects->getConsoleObjects().empty(), "list unregistered");
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
}
