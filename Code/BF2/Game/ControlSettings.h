// Control-map settings with backup maps for commit/cancel, key capture, and
// selected-map input tuning. Preserve the virtual interface. [INFERRED] path and names.
#ifndef GAME_CONTROLSETTINGS_H
#define GAME_CONTROLSETTINGS_H
#include "../Core/IBase.h"
#include <string>
#if !DEDICATED
namespace dice { namespace hfe {
namespace io { class IControlMap; }

// Every slot of table 8A3BF8 except the deleting destructor at +94. The manager and map slots
// the bodies dispatch are named after MSVC's reversal of adjacent overloads (deleteControlMap
// by name is PE +28, getControlMap by name +1C, setActive by map +2C, ControlMap::reset() +58).
class IControlSettings : public IBase {
public:
    virtual std::string getFileName() = 0;                    //+10 4B2AE0 "Controls.con"
    virtual std::string getSettingsScript() = 0;              //+14 4B2B00: the seven maps' save() text
    virtual bool restoreControlMaps() = 0;                    //+18 4B2F30: live maps reset, then copied from the backups; true
    virtual void backupControlMaps() = 0;                     //+1C 4B30E0: backups recreated from the live maps around the active one; createDefaultProfile 4A811E
    virtual void backupControlMapsByName() = 0;               //+20 4B2FC0: same, re-selecting the active map by its name
    virtual bool setDefaults() = 0;                           //+24 4B3230: reset, re-run Settings/Controls.con, back up; true
    virtual void resetControlMaps() = 0;                      //+28 4B2BE0: reset() on every live map
    virtual void updateInputDevices() = 0;                    //+2C 4B2C70: device manager update(-1.0f) then update(0.1f)
    virtual int mapNextInput(int input, bool alternate, int unused, bool second, bool gameInput) = 0; //+30 4B2CA0: the manager's full EAX (int status)
    virtual bool selectControlMap(const std::string& name) = 0; //+34 4B38A0: the map the feel accessors forward to; true
    virtual bool getBindingDescription(io::IControlMap* map, int input, bool alternate, bool second,
                                       bool gameInput, std::string& key, std::wstring& text) = 0;
    virtual std::string getActiveControlMapName() = 0;        //+3C 4B2C20: "" without a manager or an active map
    virtual float getMouseSensitivity() = 0;                  //+40 4B2D60: selected map or 1.0f
    virtual void setMouseSensitivity(float) = 0;
    // [INFERRED] Mouse sensitivity multiplier and smoothing slots. Original getters
    // return constants and setters do nothing; these options are inert.
    virtual float getSlot48() = 0;                            //+48 602290: constant 1.0f - mouse sensitivity multiplier
    virtual void setSlot4C(float) = 0;                        //+4C 5AB2A0: no body - mouse sensitivity multiplier
    virtual float getSlot50() = 0;                            //+50 7D3950: constant 0.0f - mouse smoothing
    virtual void setSlot54(float) = 0;                        //+54 5AB2A0: no body - mouse smoothing
    virtual float getYawFactor() = 0;                         //+58 4B2DA0: selected map or 1.0f
    virtual void setYawFactor(float) = 0;                     //+5C 4B2DC0
    virtual float getPitchFactor() = 0;                       //+60 4B2DE0: selected map or 1.0f
    virtual void setPitchFactor(float) = 0;
    virtual bool getInvertMouse() = 0;                        //+68 4B2E20: selected map or false
    virtual void setInvertMouse(bool) = 0;                    //+6C 4B2E40: no null test on the selected map
    virtual float getKeyboardSensitivity() = 0;               //+70 4B2E50: selected map or 1.0f
    virtual void setKeyboardSensitivity(float) = 0;
    virtual bool apply() = 0;                                 //+78 4B2E90: backupControlMaps, then true
    virtual void deleteControlMapBackups() = 0;               //+7C 4B31C0
    virtual void activateMouse() = 0;                         //+80 4B2EA0: device manager's mouse activate()
    virtual void deactivateMouse() = 0;                       //+84 4B2EC0: its deActivate()
    virtual bool isMappingInput() = 0;                        //+88 46CA70: the +08 flag
    virtual void setMappingInput(bool) = 0;                   //+8C 8247D0; BF2Engine's event (10,12) handler clears it
    virtual std::string getMappedInputName() = 0;             //+90 4B2D40: the manager's mapNextInput output
    // The deleting destructor at +94 (4B2F00) is the only withheld slot.
};

struct ControlSettingsFixture;
class ControlSettings final : public IControlSettings {
public:
    ControlSettings();                                        // retail 44 bytes
    // [CHOSEN, not recovered] [RETIRE-WHEN: control-settings-prefix-publication]
    // Direct final-owner deletion; the deleting destructor lives at the withheld +94 (4B2F00).
    ~ControlSettings();                                       // body
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;                                   // shared
    int getRef() override;                                    // shared
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    std::string getFileName() override;
    std::string getSettingsScript() override;
    bool restoreControlMaps() override;
    void backupControlMaps() override;
    void backupControlMapsByName() override;
    bool setDefaults() override;
    void resetControlMaps() override;
    void updateInputDevices() override;
    int mapNextInput(int input, bool alternate, int unused, bool second, bool gameInput) override;
    bool selectControlMap(const std::string& name) override;
    bool getBindingDescription(io::IControlMap* map, int input, bool alternate, bool second,
                               bool gameInput, std::string& key, std::wstring& text) override;
    std::string getActiveControlMapName() override;
    float getMouseSensitivity() override;            void setMouseSensitivity(float value) override;
    float getSlot48() override;                      void setSlot4C(float value) override;
    float getSlot50() override;                      void setSlot54(float value) override;
    float getYawFactor() override;                   void setYawFactor(float value) override;
    float getPitchFactor() override;                 void setPitchFactor(float value) override;
    bool getInvertMouse() override;                  void setInvertMouse(bool value) override;
    float getKeyboardSensitivity() override;         void setKeyboardSensitivity(float value) override;
    bool apply() override;
    void deleteControlMapBackups() override;
    void activateMouse() override;
    void deactivateMouse() override;
    bool isMappingInput() override;
    void setMappingInput(bool value) override;
    std::string getMappedInputName() override;
private:
    friend struct ControlSettingsFixture;
    int m_refCount;                          // retail +04
    bool m_mappingInput;                     //+08: set by mapNextInput, cleared by the engine's (10,12) event
    io::IControlMap* m_map;                  //+0C: selectControlMap's choice; the feel accessors forward to it
    std::string m_mappedInputName;           //+10: the manager's mapNextInput output
};

} }
#endif
#endif
