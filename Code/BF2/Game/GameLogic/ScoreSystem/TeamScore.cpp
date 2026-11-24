// Team score state and wire serialization. [INFERRED] source path.

#include "TeamScore.h"
#if !DEDICATED
#include "../../../Core/EventManager.h"
#endif

namespace dice {
namespace hfe {
namespace world {

// Note what it does NOT touch: +0x00, and the whole +0x44..+0x80 region. The serialiser and
// deserialiser skip that region too, so three independent silences agree it is runtime-only scratch
// rather than a missing recovery. Leaving it uninitialised here is faithful, not an oversight.
TeamScore::TeamScore()
{
    m_tickets     = 0;      // +0x84
    m_roundsWon   = 0;      // +0x88
    m_ticketState = 0;      // +0x8C
    m_ticketBleed = false;  // +0x90 — a byte store

    // +0x04..+0x40: sixteen consecutive dwords, all zeroed. Every one of them IS serialised, so
    // this block is authoritative game state despite its members being unnamed.
    for (int i = 0; i < 16; ++i)
        m_block[i] = 0;
}

// Clamps tickets to 0..9999, applying the lower and upper clamps in sequence.
void TeamScore::setTickets(int n)
{
    m_tickets = n;

    if (m_tickets < 0)
        m_tickets = 0;

    if (m_tickets > 9999)
        m_tickets = 9999;
}

// . floor only — deliberately NO upper clamp, because this path only ever
// decreases. Do not "make it symmetric" with setTickets.
void TeamScore::subTicket(int n)
{
    m_tickets -= n;

    if (m_tickets < 0)
        m_tickets = 0;
}

// BF2.exe 0x005C7620 publishes the changed state after storing it. Dedicated ELF
// 0x08107510 only stores; the HUD notification carries this team's borrowed score object.
void TeamScore::setTicketState(int state)
{
#if !DEDICATED
    const int previous = m_ticketState;
#endif
    m_ticketState = state;      // +0x8C
#if !DEDICATED
    if (previous != state)
        eventManager->postEvent(ECHud, 71, this, 0.0f);
#endif
}

// BF2.exe 0x005C75F0; dedicated ELF 0x081074F0 retains its plain byte store.
void TeamScore::setTicketBleed(bool bleeding)
{
#if !DEDICATED
    const bool previous = m_ticketBleed;
#endif
    m_ticketBleed = bleeding;   // +0x90
#if !DEDICATED
    if (previous != bleeding)
        eventManager->postEvent(ECHud, 70, this, 0.0f);
#endif
}

} // namespace world
} // namespace hfe
} // namespace dice
