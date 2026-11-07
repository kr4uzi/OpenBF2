// [INFERRED] path/names. BfEditNode storage, properties, update, painting and
// input; no complete Node descriptor or edit-node factory is published.
#include "BfEditNode.h"
#if !DEDICATED
#include "BfEditData.h"
#include "DifFontMetadata.h"
#include "Font.h"
#include "../External/BFMeme/Action.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/Pointer.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cmath>
#include <cstring>
#include <new>
#include <string>
namespace dice { namespace meme {
BfEditNode::BfEditNode()
    : m_editData(nullptr), m_string(nullptr), m_selectAction(nullptr), m_font(0),
      m_maxCharacters(-1), m_firstVisible(0), m_cursor(0), m_blinkTime(-0.4f),
      m_focused(false), m_pendingInput(0), m_ignoreClick(false), m_flag39(false),
      m_slowBlink(false), m_skipCharacter(false), m_limitWidth(false), m_outline(false),
      m_color{0.0f, 0.0f, 1.0f, 1.0f} {}
unsigned int BfEditNode::getObjectSize() { return sizeof(BfEditNode); }
bool BfEditNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamFont(system, &m_font, "Font")) return false;
    if (!stream->streamData(system, m_string, "String", nullptr)) return false;
    if (!stream->streamInt(&m_maxCharacters, "Max characters(-1 = no limit)")) return false;
    if (!stream->streamAction(system, m_selectAction, "Select action", nullptr)) return false;
    // Stream the pointer itself, retaining modifications made before a failure.
    if (!stream->streamData(system, reinterpret_cast<Data*&>(m_editData),
                            "Editbox data", BfEditData::getClassType())) return false;
    if (m_editData) {
        m_editData->m_activeNode = this;
        m_editData->registerNode(this); // reload the live member; duplicates retained
    }
    return stream->streamBool(&m_focused, "Focus") != false;
}
bool BfEditNode::updateSelf(System* system, const NodeState* state, const Rect* rect,
                          float delta)
{
    if (!Node::updateSelf(system, state, rect, delta)) return false;
    // Original stores the float but compares the still-live x87 sum. Double
    // holds the exact sum of these two float operands for this comparison.
    const double time = static_cast<double>(delta) + m_blinkTime;
    m_blinkTime = static_cast<float>(time);
    if (!m_slowBlink) {
        if (time >= static_cast<double>(0.4f)) m_blinkTime = -0.4f;
    } else if (time >= static_cast<double>(0.8f)) m_blinkTime = -0.8f;
    if (m_pendingInput == 1) {
        onEvent1(system, rect, state, m_pendingKey, 1, 1);
        m_pendingInput = 0; // callback may mutate it; the shipped clear follows
        return true;
    }
    if (m_pendingInput == 2) {
        onEvent4(system, rect, state, m_pendingCharacter, 1, 1);
        m_pendingInput = 0;
    }
    return true;
}
void BfEditNode::updateVisibleStart(System* system, const Rect* rect)
{
    if (!m_string) return;
    if (m_cursor <= m_firstVisible) {
        m_firstVisible = m_cursor;
        return;
    }
    float remaining = rect->width;
    for (int index = m_cursor - 1; index >= m_firstVisible; --index) {
        GlyphMetrics metrics;
        const FontHandle font = m_font;
        const int character = m_string->getCharacter(system, index);
        system->getGlyphMetrics(character, font, &metrics); // result deliberately discarded
        // Right + width + left, preserving the actual evaluation order.
        const double width = (static_cast<double>(metrics.rightBearing) + metrics.width) + metrics.leftBearing;
        const double next = static_cast<double>(remaining) - width;
        remaining = static_cast<float>(next);
        if (next <= 0.0) {
            m_firstVisible = index + 1;
            return;
        }
    }
}
bool BfEditNode::onEvent4(System* system, const Rect* rect, const NodeState*,
                         int character, int, int) // 7AB9B0; final two integers unused
{
    if (!m_focused || !m_string || character == 13 || character == 27 ||
        character > 256 || character == 22) return false;
    if (m_slowBlink && m_skipCharacter) {
        m_skipCharacter = false;
        return false;
    }
    m_skipCharacter = !m_skipCharacter;
    const int length = m_string->getStringLength(system);
    if (m_maxCharacters > 0 && length + 1 > m_maxCharacters) return false;
    const int capacity = length + 2;
    char* buffer = static_cast<char*>(::operator new(capacity)); // 447EF0; no null guard
    m_string->getString(system, buffer, capacity);
    std::memmove(buffer + m_cursor + 1, buffer + m_cursor, length - m_cursor + 1);
    buffer[m_cursor] = static_cast<char>(character); // negative and 256 accepted; low byte retained
    if (m_limitWidth) {
        float remaining = rect->width;
        for (int index = 0; index < length + 1; ++index) {
            GlyphMetrics metrics;
            system->getGlyphMetrics(static_cast<signed char>(buffer[index]), m_font, &metrics);
            const double width = (static_cast<double>(metrics.rightBearing) + metrics.width) + metrics.leftBearing;
            const double next = static_cast<double>(remaining) - width;
            remaining = static_cast<float>(next);
            if (next <= 0.0) return false; // Shipped buffer leak at7ABAB1 ->7ABAFE; preserve it.
        }
    }
    m_string->setString(system, buffer); // callback result is ignored; member reloaded below
    ::operator delete(buffer);
    buffer = nullptr;
    if (length != m_string->getStringLength(system)) {
        ++m_cursor;
        updateVisibleStart(system, rect);
    }
    return true;
}
TypeDescriptor* BfEditNode::getClassType()                 // 7AC340, singletonA20EFC
{
    static ClassTypeDescriptor<BfEditNode, Node> type("dice::meme::BfEditNode");
    return &type;
}
TypeDescriptor* BfEditNode::getType() { return getClassType(); }

// Draws the visible characters from m_firstVisible onward, each glyph clipped
// to the node's own bounds, with an optional eight-neighbour outline, and the
// blinking two-unit cursor while focused. A carriage return ends the line.
void BfEditNode::paintSelf(System* system, const NodeState*, const Rect* bounds,
                           const Rect*, RenderContext* context) // 7AAEF0; clip unused
{
    if (!m_string) return;
    float penX = bounds->x;
    const float penY = bounds->y;
    const float right = static_cast<float>(static_cast<double>(penX) + bounds->width);
    const float lineHeight = system->getFontHeight(m_font);
    Rect glyph = *bounds;
    int index = m_firstVisible;
    int elementIndex = 0;
    bool cursorDrawn = false;
    float scale = 1.0f;
    hfe::menu::Font* const font = reinterpret_cast<hfe::menu::Font*>(m_font);
    const std::string fontName(font->getName()); // retail has no null-font guard
    if (fontName.find(".dif", 0, 4) != std::string::npos)
        scale = static_cast<hfe::menu::DifFont*>(font)->getMetadata()->getScale();
    const float reciprocalScale = static_cast<float>(1.0 / scale);
    int character = m_string->getCharacter(system, index);
    while (character) {
        if (character == 13) {
            // 7AB56C: a line break leaves the loop; an undrawn cursor is then
            // placed at the pen as though the cursor index had been reached.
            if (!cursorDrawn) index = m_cursor;
            break;
        }
        GlyphMetrics metrics; // failure leaves the shipped output uninitialized
        system->getGlyphMetrics(character, m_font, &metrics);
        glyph.width = metrics.width;
        glyph.height = metrics.height;
        if (scale == 1.0f) {
            glyph.x = penX;
            glyph.y = static_cast<float>(static_cast<double>(metrics.verticalOffset) + penY);
        } else {
            // Unlike Style::positionGlyph the pen itself keeps the snapped x, and
            // the offset is added before the reciprocal scale.
            penX = static_cast<float>(static_cast<double>(scale) * penX);
            const float scaledY = static_cast<float>(static_cast<double>(scale) * penY);
            // x87 tests C0 alone: unordered follows the same arm as remainder<0.5.
            const double roundedX = std::floor(!(std::fmod(penX, 1.0) >= 0.5) ?
                                               penX : static_cast<double>(penX) + 1.0);
            penX = static_cast<float>(roundedX * reciprocalScale);
            glyph.x = penX;
            const double roundedY = std::floor(!(std::fmod(scaledY, 1.0) >= 0.5) ?
                                               scaledY : static_cast<double>(scaledY) + 1.0);
            glyph.y = static_cast<float>((roundedY + metrics.verticalOffset) * reciprocalScale);
        }
        if (!(penX < right)) break; // 7AB0E8; NaN keeps drawing
        if (!m_editData || !m_editData->m_hideText) {
            if (m_outline) {
                // Retail inlines setQuadBounds (7AA7F0) for the last seven
                // passes and keeps each x + width and y + height on the x87
                // stack; the out-of-line helper rounds those sums to float.
                const Color outlineColor = {0.15000001f, 0.15000001f, 0.15000001f, 1.0f};
                Quad outline(&glyph, bounds, &outlineColor);
                translateQuad(&outline, 1.0f, 1.0f, -0.001f);       // 7AA750
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX + 1.0f, glyph.y, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX + 1.0f, glyph.y - 1.0f, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX, glyph.y - 1.0f, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX - 1.0f, glyph.y - 1.0f, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX - 1.0f, glyph.y, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX - 1.0f, glyph.y + 1.0f, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                setQuadBounds(&outline, penX, glyph.y + 1.0f, glyph.width, glyph.height);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
            }
            Quad text(&glyph, bounds, &m_color);
            // Only the main glyph's result is tested; a refusal also skips the cursor.
            if (!drawContextCharacter(context, system, character, m_font, elementIndex, &text))
                return;
        }
        if (index == m_cursor && m_blinkTime >= 0.0f && m_focused) {
            // Reuses the glyph rectangle: x stays at this glyph; the rest becomes
            // the unsnapped line top, the font height and a two-unit width.
            glyph.height = lineHeight;
            glyph.y = penY;
            glyph.width = 2.0f;
            Quad cursor(&glyph, nullptr);
            drawContextTexture(context, system, 0, &cursor);
            cursorDrawn = true;
        }
        // Right + width + left, then the pen, preserving the evaluation order.
        penX = static_cast<float>(((static_cast<double>(metrics.rightBearing) + metrics.width)
                                   + metrics.leftBearing) + penX);
        ++elementIndex;
        character = m_string->getCharacter(system, ++index);
    }
    // 7AB578: the cursor after the last drawn character, or after a line break.
    if (index == m_cursor && m_blinkTime >= 0.0f && m_focused) {
        glyph.x = penX;
        glyph.height = lineHeight;
        glyph.y = penY;
        glyph.width = 2.0f;
        Quad cursor(&glyph, nullptr);
        drawContextTexture(context, system, 0, &cursor);
    }
}
// Key and pointer input for index 1. Key 18 runs the select action and drops
// focus; key 1 is a click that takes focus inside the bounds, or drops it and
// forwards the click to the System pointer outside them, then places the
// cursor. Editing keys apply only while focused. Returns false when the event
// was refused or focus was lost.
bool BfEditNode::onEvent1(System* system, const Rect* rect, const NodeState* state,
                          int key, int index, int second) // 7AB5F0
{
    if (index != 1) return false;
    if (key == 18) {
        if (m_selectAction) {
            const NodeInputEvent event = {1, 0, 0.0f, 0.0f, 0};
            m_selectAction->execute(system, rect, state, &event); // result ignored
            if (!m_focused) return false;
            m_focused = false;
            system->unlockPointer();                               // 82A3C0
        }
    } else if (key == 1 && !m_ignoreClick) {
        const float* const pointer = system->getPointerPosition(); // 682BA0
        const float x = pointer[0];
        const float y = pointer[1];
        const float rightEdge = static_cast<float>(static_cast<double>(rect->width) + rect->x);
        const float bottomEdge = static_cast<float>(static_cast<double>(rect->height) + rect->y);
        // Retail tests m_focused first and repeats these comparisons in both
        // arms. Edges and NaN count as outside.
        if (x > rect->x && x < rightEdge && y > rect->y && y < bottomEdge) {
            if (!m_focused) {
                m_focused = true;
                system->lockPointer();                             // 82A3B0
            }
        } else {
            m_focused = false;
            system->unlockPointer();
            // System 82A2D0 reloads its +20 pointer and tail-calls the guarded
            // 828DF0 wrapper; the result is ignored.
            pointerEvent1(system, static_cast<Pointer*>(system->getPointerObject()),
                          1, 1, second);
            return false;
        }
    }
    if (!m_focused || !m_string) return false;
    const int length = m_string->getStringLength(system);
    switch (key) {
    case 1:
        if (m_ignoreClick) {
            m_ignoreClick = false;
            break;
        }
        {
            const float* const pointer = system->getPointerPosition();
            float offset = static_cast<float>(static_cast<double>(rect->x) - pointer[0]);
            if (offset < 0.0f) offset = offset * -1.0f;            // ds:8AA8C0
            float remaining = rect->width;
            int position = 0;
            for (; position < length; ++position) {
                GlyphMetrics metrics;
                const FontHandle font = m_font;
                const int character = m_string->getCharacter(system, position);
                system->getGlyphMetrics(character, font, &metrics); // result discarded
                const double width = (static_cast<double>(metrics.rightBearing) + metrics.width)
                                     + metrics.leftBearing;
                const double next = static_cast<double>(remaining) - width;
                remaining = static_cast<float>(next);
                // The unrounded difference meets the unrounded width - offset.
                if (next <= static_cast<double>(rect->width) - offset) {
                    m_cursor = position;
                    updateVisibleStart(system, rect);
                    break;
                }
            }
            if (position == length) {
                m_cursor = position;
                updateVisibleStart(system, rect);
            }
        }
        break;
    case 5:
        if (m_cursor > 0) --m_cursor;
        break;
    case 7:
        if (m_cursor < length) ++m_cursor;
        break;
    case 9:
        m_cursor = 0;
        updateVisibleStart(system, rect);
        break;
    case 10:
        m_cursor = length - 1; // shipped: one before the end, -1 for empty text
        updateVisibleStart(system, rect);
        break;
    case 14:
        if (m_cursor < length) {
            char* buffer = static_cast<char*>(::operator new(length + 1)); // 447EF0
            m_string->getString(system, buffer, length + 1);
            std::memmove(buffer + m_cursor, buffer + m_cursor + 1, length - m_cursor); // 73C5D0
            m_string->setString(system, buffer); // result ignored
            ::operator delete(buffer);           // 737EE0 frees and nulls
            buffer = nullptr;
        }
        break;
    case 15:
        if (m_cursor > 0) {
            char* buffer = static_cast<char*>(::operator new(length + 1));
            m_string->getString(system, buffer, length + 1);
            std::memmove(buffer + m_cursor - 1, buffer + m_cursor, length - m_cursor + 1);
            m_string->setString(system, buffer);
            ::operator delete(buffer);
            buffer = nullptr;
            --m_cursor; // member reloaded after the callback
        }
        break;
    case 16:
        return true; // the only arm without the trailing visible-start update
    case 17:
        if (m_ignoreClick) m_ignoreClick = false;
        break;
    default:
        break;
    }
    updateVisibleStart(system, rect);
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(BfEditNode) == 80, "native BfEditNode storage");
#endif
} }
#endif
