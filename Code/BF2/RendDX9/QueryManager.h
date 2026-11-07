#ifndef BF2_RENDDX9_QUERYMANAGER_H
#define BF2_RENDDX9_QUERYMANAGER_H

namespace dice { namespace hfe { namespace dx9rend {

extern void* g_queryManager;

// Access the manager's enable flag; unsupported hardware ignores writes.
// Changing the flag releases or recreates its tracked D3D queries.
bool queryManagerGetHWOcclusion(void* manager);
void queryManagerSetHWOcclusion(void* manager, bool enabled);

// Maps the recovered Query fields to their native addresses without changing ownership.
// An offset outside the recovered field map reports a frontier and aborts.
unsigned char* queryFieldAt(void* query, unsigned int retailOffset);

} } }

#endif
