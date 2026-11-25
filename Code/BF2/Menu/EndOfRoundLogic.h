// End-of-round movie lifecycle and input. [INFERRED] Source placement and member names.
#ifndef MENU_ENDOFROUNDLOGIC_H
#define MENU_ENDOFROUNDLOGIC_H
#include "IMenuLogic.h"
#include <string>
#if !DEDICATED
namespace dice { namespace hfe {

// BF2.exe primary table 0x8A2AF8: IMenuLogic only, destructor last at +0x5C.
class EndOfRoundLogic : public IMenuLogic
{
public:
    EndOfRoundLogic();                                         // 0x4A4100
    static IBase* create(unsigned int iid, IBase* outer);       // 0x410304
    void addRef() override;                                    // 0x6D0970
    int getRef() override;                                     // 0x68AED0
    int release() override;                                    // 0x68AEE0
    void* queryInterface(unsigned int iid) const override;      // 0x4A4550
    unsigned int getClassId() const override;                   // 0x4A40C0
    bool init() override;                                      // 0x4A3E20
    void deactivate() override;                                // 0x4A3FF0
    ui::ISwiffPlayer* getSwiffPlayer() const override;           // 0x68AD50
    ui::ISwiffHost* getSwiffHost() const override;               // 0x68ABC0
    ui::ISwiffRender* getSwiffRender() const override;           // 0x67D080
    void update(double wallDelta) override;                    // 0x4A4050
    bool beginFrame() override;                                // 0x4A4510
    void display() override;                                   // 0x4A2C50
    void endFrame() override;                                  // 0x4A4530
    void handleInput(io::GameInput& input) override;            // 0x4A4140
    void setConsole(BF2Console* console) override;              // 0x61B420
    BF2Console* getConsole() const override;                    // 0x80D430: null
    void onResize(unsigned int width, unsigned int height) override; // 0x5A59F0: ret 8
    void onEscape() override;                                  // 0x83D270: ret
    void onMouseMove(int x, int y) override;                    // 0x4A4070
    void onMouseButtonDown() override;                         // 0x4A4090
    void onMouseButtonUp() override;                           // 0x4A40A0
    void onMouseDoubleClick() override;                        // 0x4A40B0
    ~EndOfRoundLogic() override;                               // 0x4A40D0

private:
    friend struct MenuLogicFixture; // [TEMP] headless layout and lifecycle checks
    // Retail x86 offsets; native pointers and std::string use the rebuilt host ABI.
    int m_refCount;                            // +0x04, initially 1
    bool m_swiffActive;                        // +0x08
    bool m_frameActive;                        // +0x09; +0x0A..0x0B padding
    ui::ISwiffPlayer* m_player;                // +0x0C
    ui::ISwiffHost* m_host;                    // +0x10
    ui::ISwiffRender* m_render;                // +0x14
    BF2Console* m_console;                     // +0x18, stored but not read
    std::string m_activeControlMapName;        // +0x1C..0x37 (retail STL)
    bool m_mouseDown;                          // +0x38; +0x39..0x3B padding
    int m_mouseX;                              // +0x3C
    int m_mouseY;                              // +0x40; allocation ends at +0x44
};
} }
#endif
#endif
