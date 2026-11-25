// Client menu-logic interface. Preserve virtual declaration order and keep the destructor last.
// The additional MainMenuLogic slot is not part of this interface. Keep [UNRESOLVED] slots in place.

#ifndef MENU_IMENULOGIC_H           // [INFERRED] guard style
#define MENU_IMENULOGIC_H

#include "../Core/IBase.h"

namespace dice {
namespace hfe {

class BF2Console;
namespace io { class GameInput; }
namespace ui { class ISwiffPlayer; struct ISwiffHost; struct ISwiffRender; }   // Menu/ISwiffPlayer.h, ISwiffHost.h, ISwiffRender.h

class IMenuLogic : public IBase         // +0x00..+0x0C: the IBase quartet (addRef is the shared
{                                       // folded body at 0x6D0970, getRef 4 B, release 23 B,
public:                                 // queryInterface 36 B)
    virtual unsigned int getClassId() const = 0;            // +0x10  MainMenuLogic: `return 121013` (0x4A2FA0, 6 B)
    virtual bool         init() = 0;                        // +0x14  0x4A2FB0, 1,421 B - loads SwiffPlayer.dll (BF2EngineSetup::initSwiff),
                                                            //        pulls SwiffPlayer/SwiffHost/SwiffRender/MenuTeamManager, loads mainMenu.swf
    virtual void         deactivate() = 0;                  // +0x18  0x4A2B90, 98 B - the mouse device back, the control-map backups dropped, (10, 2) and the SwiffPlayer.dll unload when enabled, the cursor hidden [INFERRED] name
    virtual ui::ISwiffPlayer* getSwiffPlayer() const = 0;   // +0x1C  returns +0x0C - the "SwiffPlayer" singleton the mouse slots forward to
    virtual ui::ISwiffHost*   getSwiffHost() const = 0;     // +0x20  returns +0x10 - the "SwiffHost" singleton whose busy state gates clicks
    virtual ui::ISwiffRender* getSwiffRender() const = 0;   // +0x24  returns +0x14 - the "SwiffRender" singleton init fetched
    virtual void         update(double wallDelta) = 0;      // +0x28  0x4A4050 - BF2Engine::renderMenu pushes the frame's unclamped wall-clock delta (8 bytes)
    virtual bool         beginFrame() = 0;                  // +0x2C  0x4A2C00 - renderMenu tests its result before display()
    virtual void         display() = 0;                     // +0x30
    virtual void         endFrame() = 0;                    // +0x34
    // BF2.exe 0x4A3540 / 0x4A3BB0 / 0x4A4140 / 0x4A4640 return no value;
    // filterGameInput 0x404650 discards the slot's result.
    virtual void         handleInput(io::GameInput& input) = 0;   // +0x38; [INFERRED] name
    virtual void         setConsole(BF2Console* console) = 0;     // +0x3C  0x4A2C60 - stores +0x18, sub_446B90(0) when non-null
    virtual BF2Console*  getConsole() const = 0;            // +0x40  returns +0x18
    virtual void         onResize(unsigned int width, unsigned int height) = 0;   // +0x44  0x4A2C80, 176 B - BF2Engine::handleEvent(11, 2)'s packed (width, height): the player rescaled, the scales cached, the host's +0x2C8 flag raised [INFERRED] name
    virtual void         onEscape() = 0;                    // +0x48  0x4A3720, 180 B  [INFERRED] - WM_CHAR 27 without Ctrl
    virtual void         onMouseMove(int x, int y) = 0;     // +0x4C
    virtual void         onMouseButtonDown() = 0;           // +0x50  0x4A2D60 - button code 1
    virtual void         onMouseButtonUp() = 0;             // +0x54  0x4A2DA0 - button code 0
    virtual void         onMouseDoubleClick() = 0;          // +0x58  0x4A2DD0 - button code 2
    virtual ~IMenuLogic() {}                                // +0x5C  0x4A36B0, 48 B - the deleting destructor: release() (0x68AEE0) calls this slot with 1 at zero
};                                                          // ends here - the +0x60 slot belongs
                                                            //   to MainMenuLogic alone; see above.

} // namespace hfe
} // namespace dice

#endif // MENU_IMENULOGIC_H
