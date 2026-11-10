// gameswf_xml.h      -- Rob Savoye <rob@welcomehome.org> 2005

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

#include "../../Platform/X87Compat.h"
#include "gameswf_log.h"
#include "gameswf_action.h"
#include "gameswf_impl.h"
#include "base/smart_ptr.h"

#include "gameswf_timers.h"


namespace gameswf 
{
  // SwiffPlayer.dll 0x100774C0 copies borrowed pointers, then the method string,
  // argument array and float timing fields. 0x10075E30 assigns in the same order.
  // [INFERRED] Source-level type and field names; native pointer/container widths.
  struct interval_record
  {
    // Implicit construction/destruction: 0x10077B80 / 0x10077BA0 initializes
    // only method/arguments, then destroys arguments before method.
    as_environment* environment;
    as_object_interface* receiver;
    as_object_interface* target;
    tu_string method;
    ::array<as_value> arguments;
    float milliseconds;
    float last_fire;
  };

  // SwiffPlayer.dll 0x100F8CC0, destructor 0x1007DA60 / veneer 0x1007EB10.
  // Entry copy/construct/clear: 0x10077CA0 / 0x10077CD0 / 0x1007CD50.
  // The records own only
  // their method names and arguments; object/environment pointers are borrowed.
  static ::hash<Uint32, interval_record> s_intervals;
  static Uint32 s_next_interval = 0;

  void clear_interval_registry() { s_intervals.clear(); }

  // [CHOSEN, not recovered] S3 bridge for the native combined movie interface;
  // retail calls the receiver's environment slot directly.
  static as_environment* interval_environment(as_object_interface* receiver)
  {
    movie* target = receiver->to_movie();
    return target ? target->get_action_environment() : NULL;
  }

  // SwiffPlayer.dll 0x10077910. Keep the live record and iterator across calls:
  // retail neither snapshots nor protects them against callback invalidation.
  void update_intervals(float movie_time)
  {
    for (::hash<Uint32, interval_record>::iterator it = s_intervals.begin();
         it != s_intervals.end(); ++it)
    {
      interval_record& record = it->second;
      if ((long double)record.milliseconds * 0.001f < (long double)movie_time - record.last_fire
          || record.last_fire == 0.0f)
      {
        record.last_fire = movie_time;
        if (record.receiver == NULL) continue;
        set_call_super(NULL);
        as_value method;
        if (record.receiver->get_member(record.method.to_tu_stringi(), &method)
            && (method.get_type() == as_value::C_FUNCTION || method.get_type() == as_value::AS_FUNCTION))
        {
          for (int i = 0; i < record.arguments.size(); ++i)
            record.environment->push_val(record.arguments[i]);
          as_value result;
          if (as_as_function* function = method.to_as_function())
          {
            as_environment* env = interval_environment(record.receiver);
            if (env) env = interval_environment(record.receiver);
            else env = record.environment;
            set_environment_super(env);
            (*function)(fn_call(&result, record.receiver, env,
                record.arguments.size(), record.environment->get_top_index()));
          }
          for (int i = 0; i < record.arguments.size(); ++i)
            record.environment->pop();
        }
      }
    }
  }

  void
  timer_setinterval(const fn_call& fn)
  {
    // SwiffPlayer.dll 0x1007E030: argument refusal precedes timer allocation.
    if (fn.nargs < 2)
    {
      log_error("error: set_interval needs atleast two args\n");
      return;
    }
    interval_record record;
    record.last_fire = 0.0f;
    record.environment = fn.env;
    record.target = fn.env->get_target();
    const as_value& first = fn.arg(0);
    record.receiver = first.get_type() == as_value::OBJECT ? first.m_object_value : NULL;
    int extra;
    if (record.receiver == NULL)
    {
      record.method = first.to_string();
      record.milliseconds = (float)fn.arg(1).to_number();
      extra = 2;
    }
    else
    {
      record.method = fn.arg(1).to_string();
      if (fn.nargs < 3)
      {
        log_error("error: set_interval needs atleast three args\n");
        return;
      }
      record.milliseconds = (float)fn.arg(2).to_number();
      extra = 3;
    }
    for (int i = extra; i < fn.nargs; ++i)
      record.arguments.push_back(fn.arg(i));
    const Uint32 id = s_next_interval;
    s_intervals.set(id, record);
    ++s_next_interval;
    fn.result->set_int(bf2x87::lowWord(id));
  }
  
  void
  timer_clearinterval(const fn_call& fn)
  {
    // SwiffPlayer.dll 0x1007E210: the shipped typo names set_interval here too.
    if (fn.nargs == 0)
    {
      log_error("error: set_interval needs one arg\n");
      return;
    }
    const Uint32 id = (Uint32)bf2x87::toIntTruncate(fn.arg(0).to_number());
    interval_record copy;
    if (s_intervals.get(id, &copy)) s_intervals.invalidate(id);
  }
}
