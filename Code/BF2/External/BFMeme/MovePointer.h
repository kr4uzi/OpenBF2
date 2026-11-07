// Descriptor name82B900 attests dice::meme::MovePointer. Full23-slot table949FF0.
// Native64-byte storage, no STL.
#ifndef BFMEME_MOVEPOINTER_H
#define BFMEME_MOVEPOINTER_H
#include "Pointer.h"
#if !DEDICATED
namespace dice { namespace meme {
class TransformNode;
class MovePointer : public Pointer {
public:
    MovePointer(); // inlined in descriptor factory 82B4E0
    ~MovePointer() override; // 82B8D0 -> Pointer destructor
    TypeDescriptor* getType() override; // 82B900, named descriptor dependency
    unsigned int getObjectSize() override;
    bool paint(System*) override;
    bool setPosition(System*, float x, float y) override;
    bool event1(System*, int first, int index, int second) override;
    bool event2(System*, int first, int index) override;
    static TypeDescriptor* getClassType(); // 82B900, base slot 82B4D0 -> Pointer
protected:
    TransformNode* m_selected; // +14, borrowed
    bool m_dragging; // +18
    float m_relativeX, m_relativeY; // +1C,+20
    float m_startX, m_startY; // +24,+28
    float m_startPositionX, m_startPositionY; // +2C,+30
    float m_startWidth, m_startHeight; // +34,+38
    bool m_resizing; // +3C, first event argument ==4
};
} }
#endif
#endif
