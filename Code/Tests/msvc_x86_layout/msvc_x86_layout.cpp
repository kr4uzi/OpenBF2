// [TEMP] Compiles voice-client and online-service retail layouts for the MSVC x86 ABI on any host:
// clang -target i686-pc-windows-msvc -fsyntax-only with the stub headers next to this file. The
// Voice asserts live in the headers under _M_IX86 (VoipClient 0xA0 with +0x44 / +0x88,
// BF2VoipClient 0xC8, VoipBandPassFilter 0x70); online_service_layout.h checks each 8-byte owner.
// Compile with DEDICATED=0 and force-include Core/BuildConfig.h. This is no MSVC image build.
#include "../../BF2/Game/VoipClient/BF2VoipClient.h"
#include "../online_service_layout.h"
static_assert(sizeof(void*) == 4, "the MSVC x86 target");
