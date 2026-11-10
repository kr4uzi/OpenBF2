// gameswf_xml.h      -- Rob Savoye <rob@welcomehome.org> 2005

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Implementation of ActionScript String class.


#include "../../Platform/X87Compat.h"
#include "base/tu_config.h"
#include "gameswf_string.h"
#include "gameswf_log.h"
#include "base/utf8.h"
#include "base/smart_ptr.h"
#include <cctype>


namespace gameswf
{
	struct tu_string_as_object;
	static void register_string_methods(tu_string_as_object* str);
	static void set_string_result(as_value* result, const tu_string& text);
	static void string_slice_result(const fn_call& fn, const tu_stringi& method, const tu_string& text);
	static void string_case_result(const fn_call& fn, const tu_string& text, bool upper);

	void string_method(const fn_call& fn, const tu_stringi& method_name, const tu_string& this_string)
	// Dispatches String builtins; failed argument checks leave the result untouched.
	{
		if (method_name == "charAt" || method_name == "charCodeAt")
		{
			// SwiffPlayer.dll 0x1009B350 / 0x1009B410: byte offsets, index zero refused.
			if (fn.nargs != 1)
			{
				log_error("String::%s needs one argument\n", method_name.c_str());
				return;
			}
			const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
			if (index > 0 && index <= this_string.length())
			{
				const char* p = this_string.c_str() + index;
				if (method_name == "charAt")
				{
					const char text[2] = { *p, 0 };
					set_string_result(fn.result, tu_string(text));
				}
				else
				{
					fn.result->set_double(utf8::decode_next_unicode_character(&p));
				}
			}
			return;
		}
		else if (method_name == "fromCharCode")
		{
			// SwiffPlayer.dll 0x1009AFE0: only the first argument is encoded.
			if (fn.nargs == 0)
			{
				log_error("String::fromCharCode needs one argument\n");
				return;
			}
			// 0x1009B00A-0x1009B02F: construct/register before argument conversion.
			as_object_interface* result = create_dice_string_object(NULL, 0);
			const int code = bf2x87::toIntTruncate(fn.arg(0).to_number());
			char text[8];
			int offset = 0;
			utf8::encode_unicode_character(text, &offset, static_cast<Uint32>(code));
			text[offset] = 0;
			assign_dice_string_object_narrow(result, text);
			fn.result->set_as_object_interface(result);
			return;
		}
		else if (method_name == "toUpperCase" || method_name == "toLowerCase")
		{
			string_case_result(fn, this_string, method_name == "toUpperCase");
			return;
		}
		else if (method_name == "indexOf" || method_name == "lastIndexOf")
		{
			// SwiffPlayer.dll 0x1009B570 / 0x1009B640: forward search from an unchecked offset.
			if (fn.nargs == 0)
			{
				log_error("String::%s needs atleast one argument\n", method_name.c_str());
				return;
			}
			const char* needle = fn.arg(0).to_string();
			int start = 0;
			if (fn.nargs > 1) start = bf2x87::toIntTruncate(fn.arg(1).to_number());
			const char* text = this_string.c_str();
			const char* found = strstr(text + start, needle);
			if (method_name == "lastIndexOf")
			{
				const char* last = NULL;
				while (found)
				{
					last = found;
					found = strstr(found + 1, needle);
				}
				found = last;
			}
			fn.result->set_int(found ? static_cast<int>(found - text) : -1);
			return;
		}
		else if (method_name == "concat")
		{
			// SwiffPlayer.dll 0x1009B4C0 returns a fresh narrow String object.
			if (fn.nargs == 0)
			{
				log_error("String::concat needs atleast one argument\n");
				return;
			}
			tu_string text = this_string;
			for (int i = 0; i < fn.nargs; ++i) text += fn.arg(i).to_string();
			set_string_result(fn.result, text);
			return;
		}
		else if (method_name == "split")
		{
			// SwiffPlayer.dll 0x1009B110: empty fields consume an index but are not stored.
			IF_VERBOSE_ACTION(log_msg("-- String split \n"));
			if (fn.nargs < 1)
			{
				log_error("String::split needs atleast one argument\n");
				return;
			}
			const tu_string delimiter = fn.arg(0).to_tu_string();
			int limit = 0x7fffffff;
			if (fn.nargs > 1) limit = bf2x87::toIntTruncate(fn.arg(1).to_number());
			smart_ptr<as_object> result(create_array_object());
			const char* text = this_string.c_str();
			for (int index = 0; index < limit; ++index)
			{
				const char* found = strstr(text, delimiter.c_str());
				const int count = found ? static_cast<int>(found - text) : static_cast<int>(strlen(text));
				if (count != 0)
				{
					char name[20];
					snprintf(name, sizeof(name), "%d", index);
					result->set_member(name, as_value(tu_string(text, count).c_str()));
				}
				if (!found) break;
				text = found + delimiter.length();
			}
			fn.result->set_as_object_interface(result.get_ptr());
			return;
		}
		else if (method_name == "length")
		{
			fn.result->set_int(this_string.length());
			return;
		}
		else if (method_name == "slice" || method_name == "substr" || method_name == "substring")
		{
			string_slice_result(fn, method_name, this_string);
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
		//  the shipped object's std::wstring at +60: set only when the object
		// is built from wide text (Locale.getString), a terminating 0 included; empty otherwise.
		array<Uint16> m_wide;
		// SwiffPlayer.dll 0x1009C140 / 0x1009C110 destroys wide text, narrow
		// text, then the object base. [CHOSEN, not recovered] S3 bridge:
		// implicit destruction keeps the native wide array and object ownership.

		tu_string_as_object()
		{
			// SwiffPlayer.dll 0x1009BB10: empty narrow/wide values and builtin methods.
			// [CHOSEN, not recovered] S3 bridge: native strings and parent interface.
			register_string_methods(this);
		}

		//  `length` on a String object, as on a string value.
		// [#38] the shipped getter (sub_1009B090) answers the WIDE size first (+80) and falls back to
		// the narrow length only when no wide text is set.
		virtual bool	get_member(const tu_stringi& name, as_value* val)
		{
			if (name == "length")
			{
				// SwiffPlayer.dll 0x1009B090: narrow length is measured in bytes.
				val->set_int(m_wide.size() > 1 ? m_wide.size() - 1 : m_string.length());
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

	//  the forwarders for the methods the shipped String object registers.
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
		// SwiffPlayer.dll 0x1009AFE0 never reads its receiver; the class has no payload.
		string_method(fn, tu_stringi("fromCharCode"), tu_string());
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
		// SwiffPlayer.dll 0x1009AFD0 returns the same object, including its wide payload.
		fn.result->set_as_object_interface(fn.this_ptr);
	}


	as_object* create_dice_string_class()
	{
		// SwiffPlayer.dll 0x1009BA20: an object with fromCharCode, no text payload.
		// [CHOSEN, not recovered] S3 bridge to the native parent/interface storage.
		as_object* object = new as_object;
		object->set_member("fromCharCode", &string_from_char_code);
		return object;
	}

	// SwiffPlayer.dll 0x1009BA20 registers fromCharCode in the base; 0x1009BB10
	// registers the instance methods in the order below.
	static void	register_string_methods(tu_string_as_object* str)
	{
		str->set_member("fromCharCode", &string_from_char_code);
		str->set_member("split", &string_split);
		str->set_member("charAt", &string_char_at);
		str->set_member("charCodeAt", &string_char_code_at);
		str->set_member("concat", &string_concat);
		str->set_member("indexOf", &string_index_of);
		str->set_member("lastIndexOf", &string_last_index_of);
		str->set_member("slice", &string_slice);
		str->set_member("toLowerCase", &string_to_lower_case);
		str->set_member("toUpperCase", &string_to_upper_case);
		str->set_member("substr", &string_substr);
		str->set_member("substring", &string_substring);
		str->set_member("toString", &string_to_string);
	}

	// [CHOSEN, not recovered] S3 bridge to the current String storage and factory.
	// Retail builtin results construct 0x1009BB10, then assign only the narrow payload.
	static void set_string_result(as_value* result, const tu_string& text)
	{
		smart_ptr<tu_string_as_object> str(new tu_string_as_object);
		str->m_string = text;
		result->set_as_object_interface(str.get_ptr());
	}

	static void string_slice_result(const fn_call& fn, const tu_stringi& method, const tu_string& text)
	{
		// SwiffPlayer.dll 0x1009B730 / 0x1009B910 / 0x1009BFB0 preserve distinct
		// length/conversion order. All allocate the result before copying the suffix.
		if (fn.nargs == 0)
		{
			log_error("String::%s needs atleast one argument\n", method.c_str());
			return;
		}
		int length = 0;
		if (method == "substr") length = text.length();
		int start = bf2x87::toIntTruncate(fn.arg(0).to_number());
		if (method == "substr" && start < 0) start += length;
		if (method == "substring" && start < 0) start = 0;
		if (method == "substring") length = text.length();
		int end = method == "substr" ? length - start : length;
		if (fn.nargs > 1)
		{
			end = bf2x87::toIntTruncate(fn.arg(1).to_number());
			if (method == "substring" && end < 0) end = 0;
		}
		if (method == "slice") length = text.length();
		smart_ptr<tu_string_as_object> result(new tu_string_as_object);
		if (start >= 0 && start < length)
		{
			result->m_string = text.c_str() + start;
			// Retail forwards negative resize lengths too. The base tag owns the
			// unresolved resize contract at 0x1006E3B0; no clamp belongs here.
			if (method == "substr") result->m_string.resize(end);
			else if (end < length) result->m_string.resize(end - start);
		}
		fn.result->set_as_object_interface(result.get_ptr());
	}

	static void string_case_result(const fn_call& fn, const tu_string& text, bool upper)
	{
		// SwiffPlayer.dll 0x1009B830 / 0x1009B8A0: construct, copy the narrow
		// bytes, call the CRT's in-place case conversion, then publish the object.
		smart_ptr<tu_string_as_object> result(new tu_string_as_object);
		result->m_string = text;
		char* bytes = const_cast<char*>(result->m_string.c_str());
#if defined(_WIN32)
		if (upper) _strupr(bytes);
		else _strlwr(bytes);
#else
		// [CHOSEN, not recovered] POSIX CRT bridge: bytewise native locale
		// mapping, without UTF-8 decoding. MSVCR71 locale equivalence is unverified.
		for (char* p = bytes; *p; ++p)
		{
			const unsigned char c = static_cast<unsigned char>(*p);
			*p = static_cast<char>(upper ? std::toupper(c) : std::tolower(c));
		}
#endif
		fn.result->set_as_object_interface(result.get_ptr());
	}

	//  DICE's String object built from wide text - what the shipped
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
		return str;
	}

	//  Locale.getString1004C650 constructs its String
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

	//  The narrow twin. Mod.getCurrentModName (sub_1004F950) builds the same
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
		// SwiffPlayer.dll 0x1008D670: construct/register before converting the argument.
		smart_ptr<tu_string_as_object> str = new tu_string_as_object;

		if (fn.nargs > 0)
		{
			str->m_string = fn.arg(0).to_tu_string();
		}

		fn.result->set_as_object_interface(str.get_ptr());
	}
  
} // namespace gameswf
