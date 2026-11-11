// Typed bounds dispatch while the native and retail definition hierarchies differ.
#ifndef GAMESWF_BOUNDS_H
#define GAMESWF_BOUNDS_H

#include "gameswf.h"

namespace gameswf
{
	// [CHOSEN, not recovered] S3 bridge: the retail definition's rectangle slot
	// is supplied by these native providers without asserting a shared x86 layout.
	struct definition_bounds_provider
	{
		virtual rect get_bound_copy() const = 0;
	protected:
		~definition_bounds_provider() = default;
	};

	// Returns the definition's rectangle, or diagnoses an unmapped native type.
	// No substitute dimensions are published for legacy definitions without a provider.
	rect get_definition_bounds(const character_def* definition);
}

#endif
