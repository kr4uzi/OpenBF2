// Script-defined engine sound properties.

#ifndef SOUND_ENGINESOUNDTEMPLATE_H     // [INFERRED] guard style
#define SOUND_ENGINESOUNDTEMPLATE_H

namespace dice {
namespace hfe {

class EngineSoundTemplate
{
public:
    EngineSoundTemplate();                          // clears the two bools,
                                                    // then calls reset()
    void reset();                                   // the seven Vars lookups

    unsigned char m_byte00;                         // +0x00 audio.newStyleAudio
    unsigned char m_byte01;                         // +0x01 audio.transformationRelativeRoot
    float m_swayFrequencyFirst;                     // +0x04
    float m_swayFrequencyLast;                      // +0x08
    float m_swayAmplitudeFirst;                     // +0x0C
    float m_swayAmplitudeLast;                      // +0x10
    float m_swayTimeFirst;                          // +0x14
    float m_swayTimeLast;                           // +0x18
    float m_materialTweak;                          // +0x1C
};

} // namespace hfe
} // namespace dice

#endif
