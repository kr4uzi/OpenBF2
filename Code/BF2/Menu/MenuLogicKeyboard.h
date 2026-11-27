// [INFERRED] Shared source expression of BF2.exe 0x4A41CA..0x4A4283 / 0x4A479A..0x4A4853.
#ifndef MENU_MENULOGICKEYBOARD_H
#define MENU_MENULOGICKEYBOARD_H
#include "ISwiffPlayer.h"
#include "../IO/Input/Input.h"
#if !DEDICATED
namespace dice { namespace hfe {
int menuScanCodeToChar(unsigned int scanCode, unsigned short* character);
namespace menu_logic_detail {
inline void keyboardInput(ui::ISwiffPlayer* player, io::IKeyboardDevice* keyboard)
{
    for (unsigned int key = 1; key < 0xFF; ++key)
    {
        const auto code = static_cast<io::InputDeviceKeys>(key);
        if (keyboard->keyPressed(code))
        {
            unsigned short character[2] = {};
            if (key == 15 || key == 28 || key == 14 || menuScanCodeToChar(key, character) != 1)
                player->notifyKeyState(static_cast<int>(key), 1);
            else if ((character[0] >= 0x20 && character[0] <= 0x7E)
                     || (character[0] >= 0xA0 && character[0] <= 0xFF))
                player->notifyChar(character[0]);
        }
        else if (keyboard->keyReleased(code))
            player->notifyKeyState(static_cast<int>(key), 0);
    }
}
} } }
#endif
#endif
