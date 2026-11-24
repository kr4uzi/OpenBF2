// Header placement [INFERRED] beside the PE-attested GameControllerDevice.cpp.
// Client primary table 0x914E10 has 31 slots and no virtual destructor.
#ifndef IO_INPUT_GAMECONTROLLERDEVICE_H
#define IO_INPUT_GAMECONTROLLERDEVICE_H
#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include "Input.h"
#include <cstdint>

struct IDirectInput8A;
struct IDirectInputDevice8A;
struct DIDEVCAPS;

namespace dice { namespace hfe { namespace io {

class GameControllerDevice : public IPOVInputDevice
{
public:
    GameControllerDevice();
    ~GameControllerDevice();                       // nonvirtual, 0x6D69D0
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    InputDeviceTypes getType() const override;
    bool setCooperativeMode(InputDeviceMode mode) override;
    InputDeviceMode getCooperativeMode() const override;
    bool activate() override;
    bool deActivate() override;
    void update(float deltaTime) override;
    bool isActive() const override;
    bool getNextInput(InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&, bool&) override;
    int getNumAxes() const override;
    float readAxis(InputDeviceAxes, bool) const override;
    void peakXYAxis(float&, float&) override;
    void setInvertAxis(InputDeviceAxes, bool) override;
    bool getInvertAxis(InputDeviceAxes) const override;
    void setAxisScale(InputDeviceAxes, float) override;
    float getAxisScale(InputDeviceAxes) const override;
    void setAxisAdd(InputDeviceAxes, float) override;
    float getAxisAdd(InputDeviceAxes) const override;
    int getNumButtons() const override;
    bool buttonDown(InputDeviceButtons) const override;
    bool buttonPressed(InputDeviceButtons) const override;
    bool buttonReleased(InputDeviceButtons) const override;
    float buttonPressedSampleLatency(InputDeviceButtons) const override;
    int getNumPOVs() const override;                 // +68, 0x6BBD40
    int readPOV(InputDevicePOVs) const override;     // +6C, 0x6D74A0
    virtual bool slot_0x70() const;                // real false; identity [UNRESOLVED], const [INFERRED]
    virtual bool init(int index);                  // +74, 0x6D7610
    virtual int getDeviceIndex() const;            // +78, name/constness [INFERRED]

private:
    bool initDirectInput();                        // 0x6D7160, name [INFERRED]
    int m_refCount;                                // +04
    InputDeviceMode m_mode;                       // +08, untouched by ctor
    unsigned int m_numButtons;                    // +0C
    std::uint32_t m_buttons, m_previousButtons;    // +10/+14
    float m_axes[8];                              // +18
    int m_numAxes;                                // +38, untouched by ctor
    IDirectInputDevice8A* m_device;                // +3C
    DIDEVCAPS* m_capabilities;                     // +40
    float m_axisScale[8];                         // +44
    float m_axisAdd[8];                           // +64
    bool m_invertAxis[8];                         // +84
    bool m_axisReady[8];                          // +8C
    int m_povs[4];                                // +94, untouched by ctor
    int m_deviceIndex;                            // +A4, ctor -1
    unsigned int m_axisMask;                      // +A8
    int m_numSliders;                             // +AC
    int m_numPOVs;                                // +B0
    IDirectInput8A* m_directInput;                 // +B4, untouched by ctor
    bool m_active;                                // +B8
    bool m_povsAsButtons;                         // +B9
};

} } }
#endif
#endif
