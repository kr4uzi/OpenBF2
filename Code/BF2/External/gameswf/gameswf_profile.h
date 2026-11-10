// Shared counters for the movie, ActionScript and renderer profiling sites.
#ifndef GAMESWF_PROFILE_H
#define GAMESWF_PROFILE_H

#include "base/container.h"
#include "base/tu_timer.h"

namespace gameswf
{
	// [INFERRED] Source-level names; SwiffPlayer.dll 0x1006F140 controls this flag.
	extern bool s_profiling;

	// The registry borrows static records. Destruction releases only the name.
	struct profile_record
	{
		Uint32 m_count;
		uint64 m_ticks;
		uint64 m_start;
		tu_string m_name;

		// SwiffPlayer.dll 0x10075230; each caller initializes its own start time.
		explicit profile_record(const char* name);
		// SwiffPlayer.dll 0x10084CF0: only the name has destruction work.
		~profile_record() = default;

		// SwiffPlayer.dll 0x10084D10, also inlined at the other profiling sites.
		void begin() { m_start = tu_timer::get_profile_ticks(); }
		void end()
		{
			m_ticks += tu_timer::get_profile_ticks() - m_start;
			++m_count;
		}
	};

	// [CHOSEN, not recovered] S3 bridge to the native registry representation.
	// Used by the report reader without transferring ownership of the records.
	const array<profile_record*>& get_profile_records();
}

#endif
