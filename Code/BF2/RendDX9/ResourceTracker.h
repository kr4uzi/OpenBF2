// Bookkeeping for D3D resources grouped as textures, vertex buffers, and index buffers.

#ifndef RENDDX9_RESOURCE_TRACKER_H
#define RENDDX9_RESOURCE_TRACKER_H

#include <string>

namespace dice { namespace hfe { namespace dx9rend {

// The pool pointer is read immediately; the resource and owner pointers are borrowed.
void resourceTrackerRegister(unsigned int kind, void* resource, unsigned int bytes,
                             const std::string& name, const int* pool, void* owner);
void resourceTrackerRemove(unsigned int kind, void* resource);
// Returns the sum of registered byte sizes; kind must be in [0, 3].
unsigned int resourceTrackerBytes(unsigned int kind);
unsigned int resourceTrackerTotalBytes();

}}}

#endif
