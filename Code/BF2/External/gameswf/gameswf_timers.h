// gameswf_xml.h      -- Rob Savoye <rob@welcomehome.org> 2005

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

#ifndef __TIMERS_H__
#define __TIMERS_H__


#include "gameswf_action.h"

namespace gameswf 
{
  
  void timer_setinterval(const fn_call& fn);
  void timer_clearinterval(const fn_call& fn);

  // Dispatches the global interval table against the root's pre-advance clock.
  // Receivers/environments are borrowed; callbacks may mutate the same table.
  // [INFERRED] Native entry name for SwiffPlayer.dll 0x10077910.
  void update_intervals(float movie_time);
  // [CHOSEN, not recovered] S3 bridge to the last native player's teardown.
  void clear_interval_registry();
  
} // end of namespace gameswf

  // __TIMERS_H_
#endif
