// gameswf_xml.h      -- Rob Savoye <rob@welcomehome.org> 2005

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Implementation of ActionScript String class.


#include "base/tu_config.h"
#include "gameswf_string.h"
#include "gameswf_log.h"
#if defined(__APPLE__)
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered] bf2x87::lowUInt32
#endif
#include "base/smart_ptr.h"


namespace gameswf
{
	void string_method(const fn_call& fn, const tu_stringi& method_name, const tu_string& this_string)
	// Executes the string method named by method_name.
	{
		if (method_name == "charCodeAt")
		{
			int	index = (int) fn.arg(0).to_number();
			if (index >= 0 && index < this_string.utf8_length())
			{
				fn.result->set_double(this_string.utf8_char_at(index));
				return;
			}

			fn.result->set_double(0);	// FIXME: according to docs, we're supposed to return "NaN"
			return;
		}
		else if (method_name == "charAt")
		{
			int	index = (int) fn.arg(0).to_number();
			if (index >= 0 && index < this_string.utf8_length())
			{
				tu_string result;
				result += this_string.utf8_char_at(index);
				fn.result->set_tu_string(result);
			}

			fn.result->set_string("");
			return;
		}
		else if (method_name == "fromCharCode")
		{
			// Takes a variable number of args.  Each arg
			// is a numeric character code.  Construct the
			// string from the character codes.

			tu_string result;

			for (int i = 0; i < fn.nargs; i++)
			{
#if defined(__APPLE__)
				// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
				uint32 c = bf2x87::lowUInt32(fn.arg(i).to_number());
#else
				uint32 c = (uint32) fn.arg(i).to_number();
#endif
				result.append_wide_char(c);
			}

			fn.result->set_tu_string(result);
			return;
		}
		else if (method_name == "toUpperCase")
		{
			fn.result->set_tu_string(this_string.utf8_to_upper());
			return;
		}
		else if (method_name == "toLowerCase")
		{
			fn.result->set_tu_string(this_string.utf8_to_lower());
			return;
		}
		else if (method_name == "indexOf")
		{
			if (fn.nargs < 1)
			{
				fn.result->set_double(-1);
				return;
			}
			else
			{
				int	start_index = 0;
				if (fn.nargs > 1)
				{
					start_index = (int) fn.arg(1).to_number();
				}
				const char*	str = this_string.c_str();
				const char*	p = strstr(
					str + start_index,	// FIXME: not UTF-8 correct!
					fn.arg(0).to_string());
				if (p == NULL)
				{
					fn.result->set_double(-1);
					return;
				}

				fn.result->set_double(tu_string::utf8_char_count(str, p - str));
				return;
			}
		}
		else if (method_name == "substring")
		{
			// Pull a slice out of this_string.
			int	start = 0;
			int	utf8_len = this_string.utf8_length();
			int	end = utf8_len;
			if (fn.nargs >= 1)
			{
				start = (int) fn.arg(0).to_number();
				start = iclamp(start, 0, utf8_len);
			}
			if (fn.nargs >= 2)
			{
				end = (int) fn.arg(1).to_number();
				end = iclamp(end, 0, utf8_len);
			}

			if (end < start) swap(&start, &end);	// dumb, but that's what the docs say
			assert(end >= start);

			fn.result->set_tu_string(this_string.utf8_substring(start, end));
			return;
		}
		// [DICE-MOD SwiffPlayer #31: the rest of the String method set the shipped String object
		// registers (sub_1009BB10: split, charAt, charCodeAt, concat, indexOf, lastIndexOf, slice,
		// toLowerCase, toUpperCase, substr, substring, toString). Stock 2006 stopped at the TODO
		// list that stood here; the login page's column lists split their construct parameters.]
		else if (method_name == "concat")
		{
			tu_string	result = this_string;
			for (int i = 0; i < fn.nargs; i++)
			{
				result += fn.arg(i).to_tu_string();
			}
			fn.result->set_tu_string(result);
			return;
		}
		else if (method_name == "lastIndexOf")
		{
			if (fn.nargs < 1)
			{
				fn.result->set_double(-1);
				return;
			}
			const tu_string	needle = fn.arg(0).to_tu_string();
			const char*	str = this_string.c_str();
			int	best = -1;
			if (needle.length() > 0)
			{
				for (const char* p = strstr(str, needle.c_str()); p != NULL; p = strstr(p + 1, needle.c_str()))
				{
					best = tu_string::utf8_char_count(str, p - str);
				}
			}
			fn.result->set_double(best);
			return;
		}
		else if (method_name == "length")
		{
			fn.result->set_int(this_string.utf8_length());
			return;
		}
		else if (method_name == "slice" || method_name == "substr")
		{
			const int	utf8_len = this_string.utf8_length();
			int	start = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : 0;
			if (start < 0) start += utf8_len;
			start = iclamp(start, 0, utf8_len);
			int	end = utf8_len;
			if (fn.nargs >= 2)
			{
				if (method_name == "slice")
				{
					end = (int) fn.arg(1).to_number();
					if (end < 0) end += utf8_len;
				}
				else
				{
					end = start + (int) fn.arg(1).to_number();
				}
				end = iclamp(end, 0, utf8_len);
			}
			if (end < start) end = start;
			fn.result->set_tu_string(this_string.utf8_substring(start, end));
			return;
		}
		else if (method_name == "split")
		{
			as_object*	arr = create_array_object();
			smart_ptr<as_object>	keep(arr);
			if (fn.nargs < 1 || fn.arg(0).get_type() == as_value::UNDEFINED)
			{
				array_object_push(arr, as_value(this_string.c_str()));
			}
			else
			{
				const tu_string	delim = fn.arg(0).to_tu_string();
				if (delim.length() == 0)
				{
					// An empty delimiter splits into single characters.
					const int	n = this_string.utf8_length();
					for (int i = 0; i < n; i++)
					{
						tu_string	c;
						c += this_string.utf8_char_at(i);
						array_object_push(arr, as_value(c.c_str()));
					}
				}
				else
				{
					const char*	s = this_string.c_str();
					for (;;)
					{
						const char*	p = strstr(s, delim.c_str());
						if (p == NULL)
						{
							array_object_push(arr, as_value(s));
							break;
						}
						tu_string	piece(s, (int) (p - s));
						array_object_push(arr, as_value(piece.c_str()));
						s = p + delim.length();
					}
				}
			}
			fn.result->set_as_object_interface(arr);
			return;
		}
		else if (method_name == "toString")
		{
			fn.result->set_tu_string(this_string);
			return;
		}

		fn.result->set_undefined();
	}



	struct tu_string_as_object : public gameswf::as_object
	{
		tu_string m_string;
		// [DICE-MOD SwiffPlayer #38] the shipped object's std::wstring at +60: set only when the object
		// is built from wide text (Locale.getString), a terminating 0 included; empty otherwise.
		array<Uint16> m_wide;

		// [DICE-MOD SwiffPlayer #31] `length` on a String object, as on a string value.
		// [#38] the shipped getter (sub_1009B090) answers the WIDE size first (+80) and falls back to
		// the narrow length only when no wide text is set.
		virtual bool	get_member(const tu_stringi& name, as_value* val)
		{
			if (name == "length")
			{
				val->set_int(m_wide.size() > 1 ? m_wide.size() - 1 : m_string.utf8_length());
				return true;
			}
			return as_object::get_member(name, val);
		}

		// [#38] the shipped get_text_value (sub_1009BFA0) returns the narrow +44 string, and the
		// shipped to_tu_string (0x1008B430, object case) reads it through that slot. A String object
		// built from wide text keeps its narrow copy EMPTY (the shipped Locale.getString never fills
		// it), so string contexts see "" for a translation - a shipped quirk, preserved.
		virtual const char*	get_text_value() const { return m_string.c_str(); }
		// [#38] Never null for a String object: the shipped text setter (sub_10089440, 0x10089553)
		// takes the object's std::wstring c_str(), which MSVC never returns null, so a String object
		// always goes through the wide setter (vslot +0x110), an empty one included.
		virtual const Uint16*	get_wide_text_value() const
		{
			static const Uint16	s_empty = 0;
			return m_wide.size() > 1 ? &m_wide[0] : &s_empty;
		}
	};

	// [DICE-MOD SwiffPlayer #31] the forwarders for the methods the shipped String object registers.
#define SWF_STRING_FORWARDER(fn_name, method)                                          \
	void fn_name(const fn_call& fn)                                                    \
	{                                                                                  \
		tu_string_as_object* this_string_ptr = (tu_string_as_object*) fn.this_ptr;    \
		assert(this_string_ptr);                                                       \
		string_method(fn, tu_stringi(method), this_string_ptr->m_string);              \
	}
	SWF_STRING_FORWARDER(string_split, "split")
	SWF_STRING_FORWARDER(string_char_at, "charAt")
	SWF_STRING_FORWARDER(string_concat, "concat")
	SWF_STRING_FORWARDER(string_index_of, "indexOf")
	SWF_STRING_FORWARDER(string_slice, "slice")
	SWF_STRING_FORWARDER(string_to_lower_case, "toLowerCase")
	SWF_STRING_FORWARDER(string_to_upper_case, "toUpperCase")
	SWF_STRING_FORWARDER(string_substr, "substr")
	SWF_STRING_FORWARDER(string_substring, "substring")
#undef SWF_STRING_FORWARDER
  
  
	void string_last_index_of(const fn_call& fn)
	{
		tu_string_as_object* this_string_ptr = (tu_string_as_object*) fn.this_ptr;
		assert(this_string_ptr);

		// tulrich: Ugh!  The caller has done hash.get() on
		// the method name to find the method, and now we're
		// going to construct a new method name and run it
		// through a big if-else.
		//
		// TODO do this more efficiently.
		string_method(fn, tu_stringi("lastIndexOf"), this_string_ptr->m_string);
	}
  
	void string_from_char_code(const fn_call& fn)
	{
		tu_string_as_object* this_string_ptr = (tu_string_as_object*) fn.this_ptr;
		assert(this_string_ptr);

		// tulrich: Ugh!  The caller has done hash.get() on
		// the method name to find the method, and now we're
		// going to construct a new method name and run it
		// through a big if-else.
		//
		// TODO do this more efficiently.
		string_method(fn, tu_stringi("fromCharCode"), this_string_ptr->m_string);
	}


	void string_char_code_at(const fn_call& fn)
	{
		tu_string_as_object* this_string_ptr = (tu_string_as_object*) fn.this_ptr;
		assert(this_string_ptr);

		// tulrich: Ugh!  The caller has done hash.get() on
		// the method name to find the method, and now we're
		// going to construct a new method name and run it
		// through a big if-else.
		//
		// TODO do this more efficiently.
		string_method(fn, tu_stringi("charCodeAt"), this_string_ptr->m_string);
	}


	void string_to_string(const fn_call& fn)
	{
		tu_string_as_object* this_string_ptr = (tu_string_as_object*) fn.this_ptr;
		assert(this_string_ptr);

		fn.result->set_tu_string(this_string_ptr->m_string);
	}


	// [DICE-MOD SwiffPlayer #31/#38] the twelve methods the shipped String object registers on
	// itself (sub_1009BB10), plus stock's fromCharCode - shared by the AS `String` constructor and
	// the wide factory below.
	static void	register_string_methods(tu_string_as_object* str)
	{
		str->set_member("toString", &string_to_string);
		str->set_member("fromCharCode", &string_from_char_code);
		str->set_member("charCodeAt", &string_char_code_at);
		str->set_member("lastIndexOf", &string_last_index_of);
		str->set_member("split", &string_split);
		str->set_member("charAt", &string_char_at);
		str->set_member("concat", &string_concat);
		str->set_member("indexOf", &string_index_of);
		str->set_member("slice", &string_slice);
		str->set_member("toLowerCase", &string_to_lower_case);
		str->set_member("toUpperCase", &string_to_upper_case);
		str->set_member("substr", &string_substr);
		str->set_member("substring", &string_substring);
	}

	// [DICE-MOD SwiffPlayer #38] DICE's String object built from wide text - what the shipped
	// Locale.getString (sub_1004C650) returns: the wide payload set, the methods registered, the
	// narrow copy left empty (see get_text_value above).
	as_object_interface*	create_dice_string_object(const Uint16* wide, int count)
	{
		tu_string_as_object* str = new tu_string_as_object;
		for (int i = 0; i < count && wide[i]; i++)
		{
			str->m_wide.push_back(wide[i]);
		}
		str->m_wide.push_back(0);
		register_string_methods(str);
		return str;
	}

	// [DICE-MOD SwiffPlayer #38] Locale.getString1004C650 constructs its String
	// before the host lookup, then assigns out.c_str() at1004C6DE and clears the
	// narrow side at1004C6EC. This helper preserves that order without new slots.
	void assign_dice_string_object_wide(as_object_interface* object, const Uint16* wide)
	{
		tu_string_as_object* str = static_cast<tu_string_as_object*>(object);
		str->m_wide.resize(0);
		for (int i = 0; wide[i]; ++i)
		{
			str->m_wide.push_back(wide[i]);
		}
		str->m_wide.push_back(0);
		str->m_string = "";
	}

	// [DICE-MOD SwiffPlayer #38] The narrow twin. Mod.getCurrentModName (sub_1004F950) builds the same
	// object the other way round: it assigns the +44 tu_string from the host's plain mod name and
	// CLEARS the +60 wide side, so get_text_value answers the narrow copy and the length getter
	// falls through to it. Same object, opposite field.
	void	assign_dice_string_object_narrow(as_object_interface* object, const char* text)
	{
		tu_string_as_object* str = static_cast<tu_string_as_object*>(object);
		str->m_string = text ? text : "";
		str->m_wide.resize(0);
	}

	void string_ctor(const fn_call& fn)
	{
		smart_ptr<tu_string_as_object> str = new tu_string_as_object;

		if (fn.nargs > 0)
		{
			str->m_string = fn.arg(0).to_tu_string();
		}

		register_string_methods(str.get_ptr());

		fn.result->set_as_object_interface(str.get_ptr());
	}
  
} // namespace gameswf
