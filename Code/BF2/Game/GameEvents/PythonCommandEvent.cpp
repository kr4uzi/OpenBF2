#include "PythonCommandEvent.h"

#include <vector>

#include "../Objects/IPlayer.h"                  // getIsAIPlayer (+0x64)
#include "../Python/PyRef.h"
#include "../Python/PythonHost.h"                // pythonHost, buildArgs, handleEvent
#include "../Main/Game.h"
#include "../../Core/EventManager.h"             // eventManager->postEvent
#include "../../IO/BitStream.h"                  // readBits / writeBits / skipBits
#include "../../Scene/Object/PlayerManager.h"    // getPlayerFromId / getLocalHumanPlayer

namespace dice {
namespace hfe {

namespace {
// .rodata dword GEPythonCommand (0x888A15C) = 49.
const int GEPythonCommand = 49;

// The Python callback slot executeServer fires. [UNRESOLVED] name - PythonHost.h records the
// ScriptEvent enumerators as unrecovered, so the value travels bare.
const int SCRIPT_EVENT_PYTHON_COMMAND = 34;      // 0x22

// The one command id executeClient reacts to, and the two ECHud events it raises.
const int HUD_COMMAND_ID   = 100;
const unsigned int NOTIFY_HUD_62 = 62u;          // 0x3E - m_args[0] of 0 or 1
const unsigned int NOTIFY_HUD_61 = 61u;          // 0x3D - m_args[0] of 2
} // namespace

// The maker's form - createEvent's inlined copy zeroes the four argument slots and nothing else.
// m_playerId, m_commandId, m_argCount and the unused +0x0C start as heap garbage.
PythonCommandEvent::PythonCommandEvent()
    : GameEvent()
{
    for (int i = 0; i < MaxArgs; ++i)
        m_args[i] = 0;
}

// Stores arguments with trailing zeros trimmed and optionally executes immediately
// as client or server. The server connection argument is unused and supplied as zero.
PythonCommandEvent::PythonCommandEvent(int commandId, int playerId, bool executeLocally,
                                       bool asClient, int arg0, int arg1, int arg2, int arg3)
    : GameEvent()
    , m_playerId(playerId)                                   // +0x10
    , m_commandId(commandId)                                 // +0x14
{
    m_args[0] = arg0;                                        // +0x18
    m_args[1] = arg1;
    m_args[2] = arg2;
    m_args[3] = arg3;

    int last = MaxArgs - 1;
    while (last >= 0 && m_args[last] == 0)
        --last;
    m_argCount = last + 1;                                   // +0x28

    if (executeLocally)
    {
        if (asClient)
            executeClient();
        else
            executeServer(0);                                // see the note above
    }
}

int PythonCommandEvent::getType() const
{
    return GEPythonCommand;
}

// Posts command-100 HUD notifications only for a local human player.
// The unused argument vector still performs its original allocations.
void PythonCommandEvent::executeClient()
{
    std::vector<int> unusedArgs;                 // built and thrown away - see above
    for (int i = 0; i < MaxArgs; ++i)
        unusedArgs.push_back(m_args[i]);

    if (m_commandId != HUD_COMMAND_ID)
        return;

    world::IPlayer* a = world::playerManager->getPlayerFromId(m_args[1]);         // +0x20
    world::IPlayer* b = world::playerManager->getPlayerFromId(m_args[2]);

    if (m_args[0] == 0 || m_args[0] == 1)
    {
        if (b && a && b != a && b == world::playerManager->getLocalHumanPlayer())  // +0x30
            eventManager->postEvent(ECHud, NOTIFY_HUD_62, &m_args[0], 0.0f);
    }
    else if (m_args[0] == 2)
    {
        if (a && a == world::playerManager->getLocalHumanPlayer() &&
            b && !b->getIsAIPlayer())                                             // +0x64
            eventManager->postEvent(ECHud, NOTIFY_HUD_61, &m_args[0], 0.0f);
    }
}

// Forwards recognized players' commands to Python without validating the command ID.
// Always passes four arguments, including zero-filled trimmed values, then notifies the local host.
bool PythonCommandEvent::executeServer(int /*connectionId*/)
{
    world::IPlayer* player = world::playerManager->getPlayerFromId(m_playerId);   // +0x20
    if (!player)
        return true;

    std::vector<int> args;
    for (int i = 0; i < MaxArgs; ++i)
        args.push_back(m_args[i]);

    {
        python::PyRef pyArgs = python::buildArgs(m_commandId, player, args);
        python::pythonHost()->handleEvent(
            static_cast<ScriptEvent>(SCRIPT_EVENT_PYTHON_COMMAND), pyArgs.get());
    }

#if !DEDICATED
    // BF2.exe 0x542F5D..0x54303E: look up both players before testing dedicated mode or command.
    world::IPlayer* a = world::playerManager->getPlayerFromId(m_args[1]);
    world::IPlayer* b = world::playerManager->getPlayerFromId(m_args[2]);
    if (!isDedicated() && m_commandId == HUD_COMMAND_ID)
    {
        if (m_args[0] == 0 || m_args[0] == 1)
        {
            bool punished = m_args[0] == 1;
            if (b && a && b != a && b == world::playerManager->getLocalHumanPlayer())
                eventManager->postEvent(ECHud, NOTIFY_HUD_62, &punished, 0.0f);
        }
        else if (m_args[0] == 2)
        {
            if (a == world::playerManager->getLocalHumanPlayer() && b && !b->getIsAIPlayer())
                eventManager->postEvent(ECHud, NOTIFY_HUD_61, &m_args[0], 0.0f);
        }
    }
#else
    // ELF 0x8078690..0x80786B7 discards both results.
    world::playerManager->getPlayerFromId(m_args[1]);
    world::playerManager->getPlayerFromId(m_args[2]);
#endif

    return true;
}

// [CHOSEN, not recovered] Retail fix "Game event readers keep wire lengths and counts inside their
// buffers" (docs/retail-fixes.md). Stores at most MaxArgs values and clamps the stored count. Skips
// excess serialized arguments so following events retain their wire alignment.
bool PythonCommandEvent::deSerialize(io::BitStream* stream)
{
    int commandId = 0;
    stream->readBits(&commandId, 32);
    m_commandId = commandId;

    int playerId = 0;
    stream->readBits(&playerId, 8);
    m_playerId = playerId;

    int argCount = 0;
    stream->readBits(&argCount, 4);              // 0..15 against an array of 4

    for (int i = 0; i < argCount; ++i)
    {
        if (i < MaxArgs)
        {
            int value = 0;
            stream->readBits(&value, 32);
            m_args[i] = value;
        }
        else
        {
            stream->skipBits(32);                // bounded - cursor unchanged, write dropped
        }
    }

    m_argCount = (argCount > MaxArgs) ? MaxArgs : argCount;

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

// serialize - 0x8078160, 272 B. The exact mirror, and the count is not range-checked on the way out
// either - it goes through a plain four-bit writeBits, so a count above 15 truncates silently.
// After deSerialize's clamp above, that cannot happen from a received event.
bool PythonCommandEvent::serialize(io::BitStream* stream)
{
    int type = getType();
    stream->writeBits(&type, gameEventTypeIdBitCount());

    int commandId = m_commandId;
    stream->writeBits(&commandId, 32);

    int playerId = m_playerId;
    stream->writeBits(&playerId, 8);

    int argCount = m_argCount;
    stream->writeBits(&argCount, 4);

    for (int i = 0; i < m_argCount && i < MaxArgs; ++i)
    {
        int value = m_args[i];
        stream->writeBits(&value, 32);
    }

    if (!stream->hasError())
        return true;
    stream->clearError();
    return false;
}

} // namespace hfe
} // namespace dice
