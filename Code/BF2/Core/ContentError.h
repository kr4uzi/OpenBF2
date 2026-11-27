// [CHOSEN, not recovered] Retail fix "Content errors are logged without ending the game"
// (docs/retail-fixes.md). Only the explicitly selected content checks use this path.
#ifndef CORE_CONTENTERROR_H
#define CORE_CONTENTERROR_H

#include "../Game/Main/BF2Log.h"
#include "../IO/Console/Console.h"

#include <cstdio>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>

namespace dice {
namespace hfe {

// Writes an Error without Debug's fatal callback/destructor. Each distinct file,
// text and content key is reported once per process, across call sites and instances.
// content distinguishes inputs that share the same diagnostic text.
inline void reportContentError(const char* file, int line, const std::string& text,
                               const std::string& content = "")
{
    typedef std::tuple<std::string, std::string, std::string> Key;
    static std::mutex mutex;
    static std::set<Key> reported;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!reported.insert(Key(file, text, content)).second)
            return;
    }

    const std::string path(file);
    const std::string::size_type name = path.rfind('/');
    const std::string::size_type dir = path.rfind('/', name - 1);
    const std::string module = path.substr(dir + 1, name - dir - 1);
    std::ostringstream out;
    out << path << '(' << line << "): Error: " << module << ": " << text << '\n';
    const std::string entry = out.str();

    // BF2Log's Error callback also terminates. Use its ordinary typed log sink
    // directly, and only interpret callback data owned by that adapter.
    if (bfGlobal_callback[DebugTypeError] == &BF2Log::debugCallback &&
        bfGlobal_callbackData[DebugTypeError])
    {
        BF2Log* log = static_cast<BF2Log*>(bfGlobal_callbackData[DebugTypeError]);
        log->addLogEntry(DebugTypeError, entry);
        log->flushWrite();
    }
    if (io::mainConsole)
        io::mainConsole->output(entry);
    else
    {
        std::fputs(entry.c_str(), stderr);
        std::fflush(stderr);
    }
}

} // namespace hfe
} // namespace dice

#endif // CORE_CONTENTERROR_H
