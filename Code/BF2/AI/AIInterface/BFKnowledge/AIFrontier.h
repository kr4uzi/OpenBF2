// [CHOSEN, not recovered] Reports an unimplemented method to stderr, flushes, and aborts.
// The argument must be its fully qualified name as a string literal. This macro never returns.
// The stop mechanism itself, not a stop: since dbg/ai-frontiers no Code/BF2/AI code calls it. Still
// included by Game/GameLogic/SquadManager/{Squad,SquadManager}.cpp (no call there either) and
// Code/Tests/aidll. [RETIRE-WHEN: aidll-01-adapters] remove with those includes.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_AIFRONTIER_H
#define AI_AIINTERFACE_BFKNOWLEDGE_AIFRONTIER_H

#include <cstdio>
#include <cstdlib>

#define BF2_AI_FRONTIER(qualifiedName)                                                             \
    do                                                                                             \
    {                                                                                              \
        std::fprintf(stderr, "[FRONTIER] " qualifiedName " reached\n");                            \
        std::fflush(stderr);                                                                       \
        std::abort();                                                                              \
    }                                                                                              \
    while (false)

#endif // AI_AIINTERFACE_BFKNOWLEDGE_AIFRONTIER_H
