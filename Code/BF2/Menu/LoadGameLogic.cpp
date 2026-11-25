// [INFERRED] Source placement. Recovered from BF2.exe 1.50 x86.
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "LoadGameLogic.h"
#include "MenuLogicKeyboard.h"
#include "IMenuTeamManager.h"
#include "ISwiffHost.h"
#include "ISwiffRender.h"
#include "../Core/ClassManager.h"
#include "../Core/SmartItf.h"
#include "../Core/Ids.h"
#include "../Core/EventManager.h"
#include "../Game/ControlSettings.h"
#include "../Game/IGame.h"
#include "../Game/Main/BF2Engine.h"
#include "../Game/Main/BF2EngineSetup.h"
#include "../Game/Main/BF2Console.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Main/BF2Window.h"
#include "../IO/Console/Console.h"
#include "../IO/Input/GameInput.h"
#include "../Platform/X87Compat.h"

namespace dice { namespace hfe {
extern BF2Engine* bf2Engine;
namespace io { extern IBase* inputDeviceManager; extern IBase* controlMapManager; }
// 0x4A4600; table 0x8A2C08, allocation 0x38.
LoadGameLogic::LoadGameLogic()
    : m_refCount(1), m_swiffActive(false), m_frameActive(false), m_player(0), m_host(0),
      m_render(0), m_console(0) {}
// 0x4A4590: inlined deactivate, then string destruction.
LoadGameLogic::~LoadGameLogic() { deactivate(); }
IBase* LoadGameLogic::create(unsigned int iid, IBase* outer) // 0x410343
{
    if (outer) return 0;
    LoadGameLogic* logic = new LoadGameLogic;
    IBase* result = static_cast<IBase*>(logic->queryInterface(iid));
    if (!result) logic->release();
    return result;
}
void LoadGameLogic::addRef() { ++m_refCount; } // 0x6D0970
int LoadGameLogic::getRef() { return m_refCount; } // 0x68AED0
int LoadGameLogic::release() // 0x68AEE0
{
    const int left = --m_refCount;
    if (!left) delete this;
    return left;
}
void* LoadGameLogic::queryInterface(unsigned int iid) const // 0x4A4550
{
    return iid == 1 || iid == IID_IMenuLogic ? const_cast<LoadGameLogic*>(this) : 0;
}
unsigned int LoadGameLogic::getClassId() const { return CID_LoadGameLogic; } // 0x4A4580
// 0x4A4290. Early failures preserve the enable flag for teardown.
bool LoadGameLogic::init()
{
    bf2Engine->bf2EngineSetup->initSwiff();
    eventManager->postEvent(static_cast<EventCategory>(10), 1, 0, 0.0f);
    m_swiffActive = true;
    m_player = static_cast<ui::ISwiffPlayer*>(classManager->getSingleton(std::string("SwiffPlayer")));
    if (!m_player) return false;
    m_host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    if (!m_host) return false;
    m_player->setSwiffHost(m_host);
    m_host->setFlag28(false);
    m_render = static_cast<ui::ISwiffRender*>(classManager->getSingleton(std::string("SwiffRender")));
    if (!m_render) return false;
    static_cast<IMenuTeamManager*>(classManager->getSingleton(std::string("MenuTeamManager")))->init();
    auto maps = static_cast<io::IControlMapManager*>(io::controlMapManager);
    if (maps->getActive()) m_activeControlMapName = maps->getActive()->getName();
    static_cast<IControlSettings*>(g_controlSettings)->apply();
    m_render->init();
    m_player->setRender(m_render);
    m_player->loadMovie("loadGame.swf");
    float width = 800.0f, height = 600.0f;
    if (bf2Engine && bf2Engine->window)
    {
        width = static_cast<float>(bf2Engine->window->getWidth());
        height = static_cast<float>(bf2Engine->window->getHeight());
    }
    const float movieWidth = static_cast<float>(static_cast<unsigned int>(m_player->getMovieWidth()));
    const float movieHeight = static_cast<float>(static_cast<unsigned int>(m_player->getMovieHeight()));
    m_player->setScale(width / movieWidth, height / movieHeight);
    static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->activateDevices(io::IDFAll);
    return true;
}
void LoadGameLogic::deactivate() // 0x4A44A0
{
    static_cast<IControlSettings*>(g_controlSettings)->deleteControlMapBackups();
    if (m_swiffActive)
    {
        eventManager->postEvent(static_cast<EventCategory>(10), 2, 0, 0.0f);
        bf2Engine->bf2EngineSetup->shutdownSwiff();
    }
    m_swiffActive = false;
    m_frameActive = false;
}
ui::ISwiffPlayer* LoadGameLogic::getSwiffPlayer() const { return m_player; } // 0x68AD50
ui::ISwiffHost* LoadGameLogic::getSwiffHost() const { return m_host; } // 0x68ABC0
ui::ISwiffRender* LoadGameLogic::getSwiffRender() const { return m_render; } // 0x67D080
void LoadGameLogic::update(double dt) // 0x4A44E0
{
    const float delta = static_cast<float>(dt);
    m_player->update(delta);
    static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->update(delta);
}
bool LoadGameLogic::beginFrame() // 0x4A4510
{
    if (!m_render->beginFrame()) return false;
    m_frameActive = true;
    return true;
}
void LoadGameLogic::display() { m_player->display(); } // 0x4A2C50
void LoadGameLogic::endFrame() { m_render->endFrame(); m_frameActive = false; } // 0x4A4530
void LoadGameLogic::handleInput(io::GameInput& input) // 0x4A4640
{
    if (m_console)
    {
        io::mainConsole->updateKeyMacros(input);
        if ((input.mappingFlags & 2) && input.inputs[1] > 0.5f)
        {
            m_console->setConsoleActive(!m_console->getConsoleActive());
            consoleCharCount = 0;
            consoleCharRead = consoleCharWrite;
        }
        if (m_console->getConsoleActive())
        {
            io::mainConsole->updateGameInput(input);
            return;
        }
    }
    auto devices = static_cast<io::IInputDeviceManager*>(io::inputDeviceManager);
    auto mouse = static_cast<io::IAdvancedInputDevice*>(devices->getInputDevice(io::IDFMouse));
    if (mouse)
    {
        const int x = bf2x87::toIntTruncate(bf2x87::wide(mouse->readAxis(io::IDAxis_0, false)) * bf2x87::wide(30.0f));
        const int y = bf2x87::toIntTruncate(bf2x87::wide(mouse->readAxis(io::IDAxis_1, false)) * bf2x87::wide(30.0f));
        m_player->queueMouseState(x, y, mouse->buttonDown(static_cast<io::InputDeviceButtons>(1)));
    }
    auto keyboard = static_cast<io::IKeyboardDevice*>(devices->getInputDevice(io::IDFKeyboard));
    if (!keyboard) return;
    if (keyboard->keyPressed(static_cast<io::InputDeviceKeys>(1)) && game)
    {
        MenuEvent event;
        event.type = 9;
        m_host->pushEvent(event);
        return;
    }
    menu_logic_detail::keyboardInput(m_player, keyboard);
}
void LoadGameLogic::setConsole(BF2Console* console) { m_console = console; } // 0x61B420
BF2Console* LoadGameLogic::getConsole() const { return 0; } // 0x80D430
void LoadGameLogic::onResize(unsigned int, unsigned int) {} // 0x5A59F0
void LoadGameLogic::onEscape() {} // 0x83D270
void LoadGameLogic::onMouseMove(int, int) {} // 0x5A59F0
void LoadGameLogic::onMouseButtonDown() {} // 0x83D270
void LoadGameLogic::onMouseButtonUp() {} // 0x83D270
void LoadGameLogic::onMouseDoubleClick() {} // 0x83D270
} }
#endif
