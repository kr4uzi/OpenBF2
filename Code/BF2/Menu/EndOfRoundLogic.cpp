// [INFERRED] Source placement. Recovered from BF2.exe 1.50 x86.
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "EndOfRoundLogic.h"
#include "MenuLogicKeyboard.h"
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
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Main/BF2Window.h"

namespace dice { namespace hfe {
extern BF2Engine* bf2Engine;
namespace io { extern IBase* inputDeviceManager; extern IBase* controlMapManager; }

// 0x4A4100; table 0x8A2AF8, allocation 0x44.
EndOfRoundLogic::EndOfRoundLogic()
    : m_refCount(1), m_swiffActive(false), m_frameActive(false), m_player(0), m_host(0),
      m_render(0), m_console(0), m_mouseDown(false), m_mouseX(0), m_mouseY(0) {}
// 0x4A40D0: deactivate before destroying the string.
EndOfRoundLogic::~EndOfRoundLogic() { deactivate(); }
// 0x410304.
IBase* EndOfRoundLogic::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    EndOfRoundLogic* logic = new EndOfRoundLogic;
    IBase* result = static_cast<IBase*>(logic->queryInterface(iid));
    if (!result) logic->release();
    return result;
}
void EndOfRoundLogic::addRef() { ++m_refCount; } // 0x6D0970
int EndOfRoundLogic::getRef() { return m_refCount; } // 0x68AED0
int EndOfRoundLogic::release() // 0x68AEE0
{
    const int left = --m_refCount;
    if (!left) delete this;
    return left;
}
void* EndOfRoundLogic::queryInterface(unsigned int iid) const // 0x4A4550
{
    return iid == 1 || iid == IID_IMenuLogic ? const_cast<EndOfRoundLogic*>(this) : 0;
}
unsigned int EndOfRoundLogic::getClassId() const { return CID_EndOfRoundLogic; } // 0x4A40C0

// 0x4A3E20. Failed singleton lookups leave partial state for deactivate().
bool EndOfRoundLogic::init()
{
    bf2Engine->bf2EngineSetup->initSwiff();
    eventManager->postEvent(static_cast<EventCategory>(10), 1, 0, 0.0f);
    m_swiffActive = true;
    m_player = static_cast<ui::ISwiffPlayer*>(classManager->getSingleton(std::string("SwiffPlayer")));
    if (!m_player) return false;
    m_host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    if (!m_host) return false;
    m_player->setSwiffHost(m_host);
    m_render = static_cast<ui::ISwiffRender*>(classManager->getSingleton(std::string("SwiffRender")));
    if (!m_render) return false;
    auto maps = static_cast<io::IControlMapManager*>(io::controlMapManager);
    if (maps->getActive()) m_activeControlMapName = maps->getActive()->getName();
    static_cast<IControlSettings*>(g_controlSettings)->apply();
    m_render->init();
    m_player->setRender(m_render);
    m_player->loadMovie("EndOfRound.swf");
    float width = 800.0f, height = 600.0f;
    if (bf2Engine && bf2Engine->window)
    {
        width = static_cast<float>(bf2Engine->window->getWidth());
        height = static_cast<float>(bf2Engine->window->getHeight());
    }
    const float movieWidth = static_cast<float>(static_cast<unsigned int>(m_player->getMovieWidth()));
    const float movieHeight = static_cast<float>(static_cast<unsigned int>(m_player->getMovieHeight()));
    m_player->setScale(width / movieWidth, height / movieHeight);
    static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFMouse)->deActivate();
    return true;
}
// 0x4A3FF0: mouse activation and backup deletion occur even after a failed init.
void EndOfRoundLogic::deactivate()
{
    static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFMouse)->activate();
    static_cast<IControlSettings*>(g_controlSettings)->deleteControlMapBackups();
    if (m_swiffActive)
    {
        eventManager->postEvent(static_cast<EventCategory>(10), 2, 0, 0.0f);
        bf2Engine->bf2EngineSetup->shutdownSwiff();
    }
    m_swiffActive = false;
    m_frameActive = false;
}
ui::ISwiffPlayer* EndOfRoundLogic::getSwiffPlayer() const { return m_player; } // 0x68AD50
ui::ISwiffHost* EndOfRoundLogic::getSwiffHost() const { return m_host; } // 0x68ABC0
ui::ISwiffRender* EndOfRoundLogic::getSwiffRender() const { return m_render; } // 0x67D080
void EndOfRoundLogic::update(double dt) { m_player->update(static_cast<float>(dt)); } // 0x4A4050
bool EndOfRoundLogic::beginFrame() // 0x4A4510
{
    if (!m_render->beginFrame()) return false;
    m_frameActive = true;
    return true;
}
void EndOfRoundLogic::display() { m_player->display(); } // 0x4A2C50
void EndOfRoundLogic::endFrame() { m_render->endFrame(); m_frameActive = false; } // 0x4A4530
// 0x4A4140: Escape queues cancellation only with a live game and no pending host request.
void EndOfRoundLogic::handleInput(io::GameInput&)
{
    m_player->queueMouseState(m_mouseX, m_mouseY, m_mouseDown);
    auto keyboard = static_cast<io::IKeyboardDevice*>(
        static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFKeyboard));
    if (!keyboard) return;
    if (keyboard->keyPressed(static_cast<io::InputDeviceKeys>(1)) && game && !m_host->getFlag7B8())
    {
        MenuEvent event;
        event.type = 15;
        m_host->pushEvent(event);
        return;
    }
    menu_logic_detail::keyboardInput(m_player, keyboard);
}
void EndOfRoundLogic::setConsole(BF2Console* console) { m_console = console; } // 0x61B420
BF2Console* EndOfRoundLogic::getConsole() const { return 0; } // 0x80D430
void EndOfRoundLogic::onResize(unsigned int, unsigned int) {} // 0x5A59F0
void EndOfRoundLogic::onEscape() {} // 0x83D270
void EndOfRoundLogic::onMouseMove(int x, int y) { m_mouseX = x; m_mouseY = y; } // 0x4A4070
void EndOfRoundLogic::onMouseButtonDown() { m_mouseDown = true; } // 0x4A4090
void EndOfRoundLogic::onMouseButtonUp() { m_mouseDown = false; } // 0x4A40A0
void EndOfRoundLogic::onMouseDoubleClick() { m_player->legacyModeOn(); } // 0x4A40B0
} }
#endif
