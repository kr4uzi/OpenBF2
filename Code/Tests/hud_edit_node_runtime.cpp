// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: HUD-EDIT-NODE-FIXTURE]
// Tests edit-node layout and builder bindings with real storage and registry.
// Only getEditData/getStringData factories and the System glyph metrics are substituted.
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <list>
#include <windows.h>
#include "../BF2/Core/Debug.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Game/HudInformationLayer.h"
#define private public
#define protected public
#include "../BF2/Menu/Bf2HudBuilder.h"
#include "../BF2/Menu/Bf2EditNode.h"
#include "../BF2/Menu/BfEditData.h"
#include "../BF2/Menu/Bf2TransformNode.h"
#include "../BF2/Menu/BF2HudManager.h"
#undef protected
#undef private
#include "../BF2/Menu/HudBlinker.h"
#include "../BF2/External/BFMeme/System.h"
#include "../BF2/External/BFMeme/Node.h"
#include "../BF2/External/BFMeme/Data.h"
#include "../BF2/External/BFMeme/IntData.h"
#include "../BF2/External/BFMeme/Quad.h"
#include "../BF2/External/BFMeme/TypeDescriptor.h"
#include "../BF2/External/BFMeme/NamedObjectRegistry.h"
#include "../BF2/Menu/Bf2HudBuilder.cpp"

namespace dice { namespace hfe {
EventManager* eventManager = nullptr;
class ServerSettings;
ServerSettings* serverSettings = nullptr;
class IGameLogic;
IGameLogic* gameLogic = nullptr;
// ds:A10890. Only BF2Engine.cpp defines it in production; without this the
// symbol is forced to address zero and the first write through it is a silent
// access violation with no output at all.
hud::BF2HudManager* hudManager = nullptr;
} }

using namespace dice::meme;
using namespace dice::hfe;
using dice::hfe::hud::Bf2HudBuilder;
using dice::hfe::hud::BF2HudManager;

static int checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

class PlainSystem : public System
{
public:
    explicit PlainSystem(NameTreeNode& registry) : System(800, 600)
    { m_registry = &registry; }
    TypeDescriptor* getType() override { return Object::getClassType(); }
    int loadFont(const char* name) override
    { fonts.push_back(name); return 400 + int(fonts.size()); }
    // Fixed five-unit advance so the edit node's visible-start walk is defined.
    bool getGlyphMetrics(int, FontHandle, GlyphMetrics* metrics) override
    { *metrics = GlyphMetrics{1.0f, 3.0f, 1.0f, 8.0f, 0.0f}; return true; }
    std::vector<std::string> fonts;
};
class RecordingEvents : public EventManager {};
namespace dice { namespace hfe { IBase* g_hudInformationLayer = nullptr; } }
static HudInformationLayer* information;

// [TEMP] the two factories rows 68 and 69 read through, and nothing else.
struct FixtureHud : BF2HudManager
{
    BfEditData* editAnswer = nullptr;
    Data* stringAnswer = nullptr;
    int editSelector = -99, stringSelector = -99;
    int editQueries = 0, stringQueries = 0;
    BfEditData* getEditData(int selector) override
    { ++editQueries; editSelector = selector; return editAnswer; }
    Data* getStringData(int selector) override
    { ++stringQueries; stringSelector = selector; return stringAnswer; }
};
static void destroyHudStorage(BF2HudManager* singleton)
{
    delete singleton->m_field2dc;
    using W = std::wstring; using S = std::string; using L = std::list<int>;
    using SL = std::list<std::string>; using V = decltype(singleton->m_field32c);
    singleton->m_field010.~W(); singleton->m_field038.~W(); singleton->m_field060.~W();
    singleton->m_field08c.~S(); singleton->m_field0c0.~S(); singleton->m_field304.~S();
    singleton->m_field0e8.~L(); singleton->m_field2e0.~SL(); singleton->m_field33c.~SL();
    singleton->m_field32c.~V();
    ::operator delete(singleton);
}

// the class
static void editNodeCases()
{
    // 84 bytes in both images: BfEditNode is 80 with no STL member, and the only
    // thing the derived class adds is the name clone at +50.
    CHECK(sizeof(Bf2EditNode) == 84);
    CHECK(sizeof(BfEditNode) == 80);
    CHECK(std::strcmp(Bf2EditNode::getClassType()->getName(),
                      "dice::meme::Bf2EditNode") == 0);
    CHECK(std::strcmp(BfEditNode::getClassType()->getName(),
                      "dice::meme::BfEditNode") == 0);
    CHECK(Bf2EditNode::getClassType()->getBaseType() == BfEditNode::getClassType());
    CHECK(BfEditNode::getClassType()->getBaseType() == Node::getClassType());
    CHECK(Bf2EditNode::getClassType()->getInstanceSize() == 84);
    {
        Bf2EditNode node("Box");
        CHECK(std::strcmp(node.getName(), "Box") == 0);
        CHECK(node.getObjectSize() == 84);
        CHECK(node.slot_0x24());
        CHECK(node.getType() == Bf2EditNode::getClassType());
        // The base's shipped defaults survive the derived constructor.
        CHECK(node.m_editData == nullptr && node.m_string == nullptr);
        CHECK(node.m_selectAction == nullptr && node.m_font == 0);
        CHECK(node.m_maxCharacters == -1);
        CHECK(node.m_firstVisible == 0 && node.m_cursor == 0);
        CHECK(node.m_blinkTime == -0.4f);
        CHECK(!node.m_focused && !node.m_limitWidth && !node.m_outline);
        // The default colour is opaque blue, not white.
        CHECK(node.m_color.r == 0.0f && node.m_color.g == 0.0f);
        CHECK(node.m_color.b == 1.0f && node.m_color.a == 1.0f);
        CHECK(node.setName("Renamed") && std::strcmp(node.getName(), "Renamed") == 0);
        CHECK(node.setName(nullptr) && node.getName() == nullptr);
    }
}

static void editSlotCases()
{
    Bf2EditNode node("Slots");
    BfEditData data("Data");
    IntData string(0, "String");
    CHECK(node.setEditDataForHud(&data) && node.m_editData == &data);
    CHECK(node.setStringForHud(&string) && node.m_string == &string);
    // Both accept a null and still answer true - no guard of their own.
    CHECK(node.setEditDataForHud(nullptr) && node.m_editData == nullptr);
    CHECK(node.setStringForHud(nullptr) && node.m_string == nullptr);

    // One call, TWO members - and the flag is the outline bit, not anything
    // about the font, even though the console row is called setEditNodeFont.
    CHECK(!node.m_outline);
    CHECK(node.setFontForHud(77, true));
    CHECK(node.m_font == 77 && node.m_outline);
    CHECK(node.setFontForHud(78, false));
    CHECK(node.m_font == 78 && !node.m_outline);

    CHECK(node.setMaxLengthForHud(24) && node.m_maxCharacters == 24);
    CHECK(node.setMaxLengthForHud(-5) && node.m_maxCharacters == -5);   // never checked
    CHECK(node.setColorForHud(0.1f, 0.2f, 0.3f, 0.4f));
    CHECK(node.m_color.r == 0.1f && node.m_color.g == 0.2f);
    CHECK(node.m_color.b == 0.3f && node.m_color.a == 0.4f);
    // The five slots are independent storage.
    CHECK(node.m_font == 78 && node.m_maxCharacters == -5);
}

// onEvent1's key arms and paintSelf's null-string return. Painting with text
// needs a loaded font and a render context and is covered in-game instead.
static void editInputCases()
{
    NameTreeRoot registry;
    PlainSystem system(registry);
    Bf2EditNode node("Keys");
    StringData text("abc", "Text");
    const Rect rect = {0.0f, 0.0f, 100.0f, 10.0f, 0.0f};
    node.paintSelf(&system, nullptr, &rect, &rect, nullptr);   // no string: nothing drawn
    node.m_string = &text;
    char buffer[8];

    // Only event index 1 is handled; unfocused input is refused.
    CHECK(!node.onEvent1(&system, &rect, nullptr, 5, 0, 1));
    CHECK(!node.onEvent1(&system, &rect, nullptr, 5, 1, 1));
    node.m_focused = true;
    node.m_cursor = 3;
    CHECK(node.onEvent1(&system, &rect, nullptr, 5, 1, 1) && node.m_cursor == 2);  // left
    CHECK(node.onEvent1(&system, &rect, nullptr, 7, 1, 1) && node.m_cursor == 3);  // right
    CHECK(node.onEvent1(&system, &rect, nullptr, 7, 1, 1) && node.m_cursor == 3);  // at end
    CHECK(node.onEvent1(&system, &rect, nullptr, 9, 1, 1) && node.m_cursor == 0);  // home
    CHECK(node.onEvent1(&system, &rect, nullptr, 5, 1, 1) && node.m_cursor == 0);  // at start
    // End stops one before the end of the text, as shipped.
    CHECK(node.onEvent1(&system, &rect, nullptr, 10, 1, 1) && node.m_cursor == 2);
    // Backspace removes the character before the cursor and moves it back.
    CHECK(node.onEvent1(&system, &rect, nullptr, 15, 1, 1) && node.m_cursor == 1);
    text.getString(&system, buffer, sizeof(buffer));
    CHECK(std::strcmp(buffer, "ac") == 0);
    // Delete removes the character under the cursor and keeps it in place.
    CHECK(node.onEvent1(&system, &rect, nullptr, 14, 1, 1) && node.m_cursor == 1);
    text.getString(&system, buffer, sizeof(buffer));
    CHECK(std::strcmp(buffer, "a") == 0);
    CHECK(node.onEvent1(&system, &rect, nullptr, 14, 1, 1));                         // at end
    text.getString(&system, buffer, sizeof(buffer));
    CHECK(std::strcmp(buffer, "a") == 0);
    CHECK(node.onEvent1(&system, &rect, nullptr, 16, 1, 1));
    // Key 17 only clears the ignore-click flag.
    node.m_ignoreClick = true;
    CHECK(node.onEvent1(&system, &rect, nullptr, 17, 1, 1) && !node.m_ignoreClick);
    // Key 18 without a select action falls through to the focused arms.
    CHECK(node.onEvent1(&system, &rect, nullptr, 18, 1, 1) && node.m_focused);
    // An unrecognised key still reports success while focused.
    CHECK(node.onEvent1(&system, &rect, nullptr, 3, 1, 1));
    node.m_focused = false;
    node.m_string = nullptr;
}

struct Harness
{
    Harness() : system(registry)
    {
        void* allocation = ::operator new(sizeof(Bf2TransformNode));
        root = new(allocation) Bf2TransformNode("IngameHud", 0, 0, 800, 600);
        CHECK(registry.insert(nullptr, root, nullptr));
        builder.setSystem(&system);
    }
    Object* find(const char* name) { return registry.findObject(&system, name); }
    NameTreeRoot registry;
    PlainSystem system;
    Bf2TransformNode* root;
    Bf2HudBuilder builder;
};

// the builder rows
static void commandCases()
{
    {   // The stock line:
        //   createEditNode SquadCreateInfo CreateSquadNameBox 17 497 210 13
        Harness harness;
        CHECK(harness.builder.createEditNode("IngameHud", "CreateSquadNameBox",
                                             17, 497, 210, 13));
        Bf2EditNode* edit = dice::hfe::hud::builderCast<Bf2EditNode>(
            harness.find("CreateSquadNameBoxEditNode"));
        CHECK(edit != nullptr);
        Bf2TransformNode* transform = dice::hfe::hud::builderCast<Bf2TransformNode>(
            harness.find("CreateSquadNameBox"));
        CHECK(transform && transform->getChild(1) == edit);
        CHECK(transform && transform->getPosition().x == 17.0f);
        CHECK(transform && transform->getSize().y == 13.0f);
        CHECK(harness.builder.m_field014 == edit);
        // The create row hands the node nothing - unlike createListNode it
        // writes no defaults at all; the node keeps its constructor's values.
        CHECK(edit && edit->m_font == 0 && edit->m_maxCharacters == -1);
        CHECK(edit && edit->m_color.b == 1.0f);

        // Row 70 and row 71.
        CHECK(harness.builder.setEditNodeMaxLength(15));
        CHECK(edit && edit->m_maxCharacters == 15);
        CHECK(harness.builder.setEditNodeColor(1.0f, 0.5f, 0.25f, 0.75f));
        CHECK(edit && edit->m_color.r == 1.0f && edit->m_color.a == 0.75f);

        // Row 72. Its Boolean is the node's outline flag.
        CHECK(harness.builder.setEditNodeFont("Fonts/edit_6.dif", true));
        CHECK(harness.system.fonts.size() == 1);
        CHECK(harness.system.fonts.size() == 1 && harness.system.fonts[0] == "Fonts/edit_6.dif");
        CHECK(edit && edit->m_font == 401 && edit->m_outline);
        // An empty font name is refused before the load.
        CHECK(!harness.builder.setEditNodeFont("", true));
        CHECK(harness.system.fonts.size() == 1);

        // The duplicate search has the family's usual child-0-chain hole.
        CHECK(harness.builder.createEditNode("IngameHud", "CreateSquadNameBox", 0, 0, 1, 1));
        harness.root->setChild(0, transform);
        CHECK(!harness.builder.createEditNode("IngameHud", "CreateSquadNameBox", 0, 0, 1, 1));
        harness.root->setChild(0, nullptr);
    }
    {   // Rows 68 and 69 reach two different HUD singleton factories, and
        // they disagree about selector zero: the data row accepts it, the
        // string row refuses it.
        Harness harness;
        CHECK(harness.builder.createEditNode("IngameHud", "Box", 0, 0, 10, 10));
        Bf2EditNode* edit =
            dice::hfe::hud::builderCast<Bf2EditNode>(harness.builder.m_field014);
        FixtureHud* const singleton = new FixtureHud;
        hudManager = singleton;
        BfEditData data("Rows");
        IntData string(0, "Text");
        singleton->editAnswer = &data;
        singleton->stringAnswer = &string;

        CHECK(data.m_activeNode == nullptr);
        CHECK(harness.builder.setEditNodeData(4));
        CHECK(singleton->editQueries == 1 && singleton->editSelector == 4);
        CHECK(edit && edit->m_editData == &data);
        // The row writes the node back into the data's own +14.
        CHECK(data.m_activeNode == edit);
        // Selector 0 passes the `>= 0` test and reaches the factory.
        CHECK(harness.builder.setEditNodeData(0));
        CHECK(singleton->editQueries == 2 && singleton->editSelector == 0);
        // A negative selector is refused before the factory.
        CHECK(!harness.builder.setEditNodeData(-1));
        CHECK(singleton->editQueries == 2);

        CHECK(harness.builder.setEditNodeString(2));
        CHECK(singleton->stringQueries == 1 && singleton->stringSelector == 2);
        CHECK(edit && edit->m_string == &string);
        // Zero is refused here, where the data row accepted it.
        CHECK(!harness.builder.setEditNodeString(0));
        CHECK(singleton->stringQueries == 1);
        CHECK(!harness.builder.setEditNodeString(-1));
        CHECK(singleton->stringQueries == 1);
        // A factory that answers null reports, and leaves the node alone.
        singleton->stringAnswer = nullptr;
        CHECK(!harness.builder.setEditNodeString(2));
        CHECK(singleton->stringQueries == 2);
        CHECK(edit && edit->m_string == &string);

        hudManager = nullptr;
        destroyHudStorage(singleton);
    }
    {   // With nothing selected every setter reports and returns false.
        Harness harness;
        FixtureHud* const singleton = new FixtureHud;
        hudManager = singleton;
        CHECK(!harness.builder.setEditNodeMaxLength(1));
        CHECK(!harness.builder.setEditNodeColor(1, 1, 1, 1));
        CHECK(!harness.builder.setEditNodeFont("a.dif", false));
        CHECK(!harness.builder.setEditNodeData(1));
        CHECK(!harness.builder.setEditNodeString(1));
        // The font row still loaded before it found out - the cast comes after.
        CHECK(harness.system.fonts.size() == 1);
        hudManager = nullptr;
        destroyHudStorage(singleton);
    }
    {   // With no System the five setter rows refuse. createEditNode does NOT
        // test it - findNodePredecessor dereferences it - so that row is covered
        // by the create-death run instead.
        Harness harness;
        harness.builder.setSystem(nullptr);
        CHECK(!harness.builder.setEditNodeMaxLength(1));
        CHECK(!harness.builder.setEditNodeColor(1, 1, 1, 1));
        CHECK(!harness.builder.setEditNodeFont("a.dif", false));
    }
}

static void printDiagnostic(DebugType, const std::string&, const std::string&, int line,
    const std::string&, const std::string& message, const std::string&, void*)
{
    std::fprintf(stderr, "FIXTURE_DEBUG line=%d %s\n", line, message.c_str());
    std::fflush(stderr);
}

static RecordingEvents fixtureEvents;

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    eventManager = &fixtureEvents;
    void* memory = ::operator new(sizeof(HudInformationLayer));
    information = new(memory) HudInformationLayer;
    g_hudInformationLayer = information;
    if (argc > 1 && !std::strcmp(argv[1], "create-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.createEditNode("IngameHud", "Warn", 0, 0, 1, 1);
        Bf2TransformNode* transform = dice::hfe::hud::builderCast<Bf2TransformNode>(
            harness.find("Warn"));
        harness.root->setChild(0, transform);
        harness.builder.createEditNode("IngameHud", "Warn", 0, 0, 1, 1);
        std::fprintf(stderr, "FIXTURE_CREATE_RETURNED\n");
        return 96;
    }
    if (argc > 1 && !std::strcmp(argv[1], "data-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setEditNodeData(1);
        std::fprintf(stderr, "FIXTURE_DATA_RETURNED\n");
        return 97;
    }
    if (argc > 1 && !std::strcmp(argv[1], "string-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setEditNodeString(1);
        std::fprintf(stderr, "FIXTURE_STRING_RETURNED\n");
        return 98;
    }
    if (argc > 1 && !std::strcmp(argv[1], "maxlength-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setEditNodeMaxLength(1);
        std::fprintf(stderr, "FIXTURE_MAXLENGTH_RETURNED\n");
        return 99;
    }
    if (argc > 1 && !std::strcmp(argv[1], "color-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setEditNodeColor(1, 1, 1, 1);
        std::fprintf(stderr, "FIXTURE_COLOR_RETURNED\n");
        return 94;
    }
    if (argc > 1 && !std::strcmp(argv[1], "font-death")) {
        setDebugCallback(DebugTypeError, printDiagnostic, nullptr);
        Harness harness;
        harness.builder.m_field018 = "Absent";
        harness.builder.setEditNodeFont("a.dif", false);
        std::fprintf(stderr, "FIXTURE_FONT_RETURNED\n");
        return 93;
    }
    turnOffAllDebug(DebugTypeError);
    editNodeCases();
    editSlotCases();
    editInputCases();
    commandCases();
    std::printf("HUD edit node %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
