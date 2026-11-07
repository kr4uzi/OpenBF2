// Round-driven demo recording and distribution. Finished recordings are handed to an
// optional background hook; update announces successful completion to clients.
// Strings, mutexes and thread members make the object layout platform-dependent.

#ifndef GAME_GAMESERVER_AUTORECORDER_H     // [INFERRED] guard style
#define GAME_GAMESERVER_AUTORECORDER_H

#include <string>
#if !defined(_WIN32)
#include <atomic>
#endif

#include "../IEventListener.h"
#include "../../Core/Mutex.h"
#include "../../Core/Thread.h"

namespace dice {
namespace hfe {

class AutoRecorder;
class Demo;
class GameServer;

// AutoRecorderHookThread - ELF ctor 0x80A0B10 (91 B), dtor 0x80A0C50 (115 B), init 0x80A0D50 (36 B),
// processFile 0x80A0D80 (18 B), run 0x80A0DA0 (333 B).
// A Thread that owns one pending filename. m_state is what roundFinished consults before handing
// over another file: 0 = no hook configured, 1 = idle and ready, 2 = busy running the hook.
class AutoRecorderHookThread : public Thread
{
public:
    AutoRecorderHookThread();                                 //  (C1)
    virtual ~AutoRecorderHookThread();

    void init(AutoRecorder* owner, Mutex* mutex, const std::string& hookCommand);
    void processFile(const std::string& file);                // one string assign

    virtual void run();                                       // Thread's +0x08 slot

private:
    friend class AutoRecorder;                                // the dtor sets m_quit under the mutex

    /* +0x08 */ int          m_state;          // 0 unconfigured, 1 idle, 2 busy
    /* +0x0C */ std::string  m_pendingFile;    // processFile writes it; run() drains it
    /* +0x10 */ std::string  m_hookCommand;    // sv.autoDemoHook, copied by init
    /* +0x14 */ AutoRecorder* m_owner;         // hookCompleted's receiver
    /* +0x18 */ Mutex*       m_mutex;          // the owner'S mutex, not one of its own
#if defined(_WIN32)
    /* +0x1C */ bool         m_quit;           // set by ~AutoRecorder before Thread::stop
#else
    // [CHOSEN, not recovered] POSIX: run also reads this flag while the owner mutex is unlocked.
    std::atomic<bool> m_quit;
#endif
};

// AutoRecorder - ELF ctor 0x809FD20 (C1, 298 B), dtor 0x809FF90 (D1) / 0x80A00D0 (D0),
// handleEvent 0x80A0220 (32 B), hookCompleted 0x80A0240 (99 B), update 0x80A02B0 (497 B),
// roundStarted 0x80A04B0 (272 B), roundFinished 0x80A05C0 (680 B), makeFilename 0x80A0870 (660 B).
class AutoRecorder : public IEventListener
{
public:
    AutoRecorder(GameServer* gameServer, Demo* demo);
    virtual ~AutoRecorder();

    // IEventListener. ECGameLogic id 1 with data[0] == 2 means the round ended.
    virtual void handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data);

    void update();
    void roundStarted();
    void roundFinished();

    // Called from the hook thread. A null argument means success; anything else is the error text.
    void hookCompleted(const char* error);

private:
    std::string makeFilename();

    /* +0x04 */ GameServer*  m_gameServer;
    /* +0x08 */ Demo*        m_demo;
    /* +0x0C */ bool         m_recording;
    /* +0x10 */ std::string  m_demoFile;       // the recording in progress
    /* +0x14 */ std::string  m_lastFile;       // the file handed to the hook
    /* +0x18 */ std::string  m_mapName;        // stamped when recording starts
    /* +0x1C */ Mutex        m_mutex;
    /* +0x20 */ AutoRecorderHookThread m_hookThread;
    /* +0x40 */ bool         m_hookDone;       // set by hookCompleted, consumed by update
    /* +0x44 */ std::string  m_hookError;      // empty on success
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMESERVER_AUTORECORDER_H
