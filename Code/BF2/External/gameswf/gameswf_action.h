// gameswf_action.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Implementation and helpers for SWF actions.


#ifndef GAMESWF_ACTION_H
#define GAMESWF_ACTION_H


#include "gameswf.h"
#if defined(__APPLE__)
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered] bf2x87::lowUInt32
#endif
#include "gameswf_types.h"
#include <wchar.h>

#include "base/container.h"
#include "base/smart_ptr.h"


namespace gameswf
{
	struct movie;
	struct as_environment;
	struct as_object_interface;
	struct as_value;
	struct as_as_function;
	void	log_error(const char* fmt, ...);	// [TEMP] RW-89 trace support (gameswf_log.h)
	struct as_object;
	// [DICE-MOD SwiffPlayer #31] a fresh Array and its push, for String.split (the array type lives in
	// gameswf_action.cpp; the String unit only needs these two).
	as_object*	create_array_object();
	// [DICE-MOD SwiffPlayer #38] DICE's String object from wide text (gameswf_string.cpp).
	as_object_interface*	create_dice_string_object(const Uint16* wide, int count);
	// [DICE-MOD SwiffPlayer #38] Assign the wide payload of an existing factory object.
	void	assign_dice_string_object_wide(as_object_interface* object, const Uint16* wide);
	// [DICE-MOD SwiffPlayer #38] the narrow twin: Mod.getCurrentModName (sub_1004F950) assigns the
	// +44 tu_string from a plain host string and CLEARS the +60 wide side, the mirror of the above.
	void	assign_dice_string_object_narrow(as_object_interface* object, const char* text);

#if !defined(_WIN32)
	// [CHOSEN, not recovered] gameswf and DICE's String object keep text as 16-bit Windows
	// wchar_t units (Uint16). The host's std::wstring/wchar_t is 32 bits on POSIX, so the
	// shipped `reinterpret_cast<const Uint16*>(wide)` handed over 'W',0,'e',0,... and every
	// localized menu string ended after its first character. dice_utf16 converts one wchar_t
	// to one Uint16 (count-preserving; above U+FFFF becomes U+FFFD). The temporary lives to
	// the end of the full expression that names it (docs/macos-c-asan-heap.md).
	struct dice_utf16
	{
		explicit dice_utf16(const wchar_t* wide)
		{
			if (!wide) { m_null = true; return; }
			for (; *wide; ++wide) m_units.push_back(unit(*wide));
			m_units.push_back(0);
		}
		operator const Uint16*() const { return m_null ? 0 : &m_units[0]; }
	private:
		static Uint16	unit(wchar_t c) { return (unsigned int) c > 0xFFFFu ? (Uint16) 0xFFFD : (Uint16) c; }
		array<Uint16>	m_units;
		bool	m_null = false;
	};
	// The opposite boundary: Uint16 text to the renderer's native wchar_t (text_set).
	struct dice_wide
	{
		explicit dice_wide(const Uint16* units)
		{
			if (units) for (; *units; ++units) m_chars.push_back((wchar_t) *units);
			m_chars.push_back(0);
		}
		operator const wchar_t*() const { return &m_chars[0]; }
	private:
		array<wchar_t>	m_chars;
	};
#define BF2_DICE_UTF16(wide)	((const Uint16*) ::gameswf::dice_utf16(wide))
#define BF2_DICE_WIDE(units)	((const wchar_t*) ::gameswf::dice_wide(units))
#else
#define BF2_DICE_UTF16(wide)	(reinterpret_cast<const Uint16*>(wide))
#define BF2_DICE_WIDE(units)	((const wchar_t*) (units))
#endif
	void	array_object_push(as_object* arr, const as_value& v);


	//
	// event_id
	//
	// For keyDown and stuff like that.

	struct event_id
	{
		// These must match the function names in event_id::get_function_name()
		enum id_code
		{
			INVALID,

			// These are for buttons & sprites.
			PRESS,
			RELEASE,
			RELEASE_OUTSIDE,
			ROLL_OVER,
			ROLL_OUT,
			DRAG_OVER,
			DRAG_OUT,
			KEY_PRESS,

			// These are for sprites only.
			INITIALIZE,
			LOAD,
			UNLOAD,
			ENTER_FRAME,
			MOUSE_DOWN,
			MOUSE_UP,
			MOUSE_MOVE,
			KEY_DOWN,
			KEY_UP,
			DATA,
			
                        // These are for the MoveClipLoader ActionScript only
                        LOAD_START,
                        LOAD_ERROR,
                        LOAD_PROGRESS,
                        LOAD_INIT,
			
                        // These are for the XMLSocket ActionScript only
                        SOCK_CLOSE,
                        SOCK_CONNECT,
                        SOCK_DATA,
                        SOCK_XML,
			
                        // These are for the XML ActionScript only
                        XML_LOAD,
                        XML_DATA,
			
                        // This is for setInterval
                        TIMER,
			
			// [DICE-MOD SwiffPlayer #27: the SWF 7 `construct` clip event (PlaceObject2 flag bit 18,
			// 0x40000). Flash fires it right after instantiation and BEFORE a registered class's
			// constructor; it is how the authoring tool bakes component-inspector parameters into the
			// movie (`__authorMode = false`, `callbackFrequency = 0`, ...). mainMenu.swf carries 1,178
			// of them. Stock 2006 gameswf had no such event, so every DICE component kept its
			// authoring-mode default and drew its IDE placeholder rectangle - the grey-box menu.]
			CONSTRUCT,
			// [DICE-MOD SwiffPlayer #51] the shipped table's 22nd event, "onDoublePress": a press the
			// host reports with the mouse buttons as 2 (the sprite mouse pass, sub_10080010).
			DOUBLE_PRESS,
			EVENT_COUNT
		};

		unsigned char	m_id;
		unsigned char	m_key_code;

		event_id() : m_id(INVALID), m_key_code(key::INVALID) {}

		event_id(id_code id, key::code c = key::INVALID)
			:
			m_id((unsigned char) id),
			m_key_code((unsigned char) c)
		{
			// For the button key events, you must supply a keycode.
			// Otherwise, don't.
			assert((m_key_code == key::INVALID && (m_id != KEY_PRESS))
				|| (m_key_code != key::INVALID && (m_id == KEY_PRESS)));
		}

		bool	operator==(const event_id& id) const { return m_id == id.m_id && m_key_code == id.m_key_code; }

		// Return the name of a method-handler function corresponding to this event.
		const tu_string&	get_function_name() const;
	};

	//
	// with_stack_entry
	//
	// The "with" stack is for Pascal-like with-scoping.

	struct with_stack_entry
	{
		smart_ptr<as_object_interface>	m_object;
		int	m_block_end_pc;
		
		with_stack_entry()
			:
			m_object(NULL),
			m_block_end_pc(0)
		{
		}

		with_stack_entry(as_object_interface* obj, int end)
			:
			m_object(obj),
			m_block_end_pc(end)
		{
		}
	};


	// Base class for actions.
	struct action_buffer
	{
		action_buffer();
		void	read(stream* in);
		void	execute(as_environment* env);
		void	execute(
			as_environment* env,
			int start_pc,
			int exec_bytes,
			as_value* retval,
			const array<with_stack_entry>& initial_with_stack,
			bool is_function2);

		bool	is_null()
		{
			return m_buffer.size() < 1 || m_buffer[0] == 0;
		}

		int	get_length() const { return m_buffer.size(); }

	private:
		// Don't put these as values in array<>!  They contain
		// internal pointers and cannot be moved or copied.
		// If you need to keep an array of them, keep pointers
		// to new'd instances.
		action_buffer(const action_buffer& a) { assert(0); }
		void operator=(const action_buffer& a) { assert(0); }

		void	process_decl_dict(int start_pc, int stop_pc);

		// data:
		array<unsigned char>	m_buffer;
		array<const char*>	m_dictionary;
		int	m_decl_dict_processed_at;
	};


	struct fn_call;
	typedef void (*as_c_function_ptr)(const fn_call& fn);


	struct as_property_interface
	{
		virtual bool	set_property(int index, const as_value& val) = 0;
	};


	// ActionScript value type.
	struct as_value
	{
		enum type
		{
			UNDEFINED,
			NULLTYPE,
			BOOLEAN,
			STRING,
			NUMBER,
			OBJECT,
			C_FUNCTION,
			AS_FUNCTION,	// ActionScript function.
		};
		type	m_type;
		mutable tu_string	m_string_value;
		union
		{
			bool m_boolean_value;
			// @@ hm, what about PS2, where double is bad?  should maybe have int&float types.
			mutable	double	m_number_value;
			as_object_interface*	m_object_value;
			as_c_function_ptr	m_c_function_value;
			as_as_function*	m_as_function_value;
		};

		as_value()
			:
			m_type(UNDEFINED),
			m_number_value(0.0)
		{
		}

		as_value(const as_value& v)
			:
			m_type(UNDEFINED),
			m_number_value(0.0)
		{
			*this = v;
		}

		as_value(const char* str)
			:
			m_type(STRING),
			m_string_value(str),
			m_number_value(0.0)
		{
		}

		as_value(const wchar_t* wstr)
			:
			m_type(STRING),
			m_string_value(""),
			m_number_value(0.0)
		{
			// Encode the string value as UTF-8.
			//
			// Is this dumb?  Alternatives:
			//
			// 1. store a tu_wstring instead of tu_string?
			// Bloats typical ASCII strings, needs a
			// tu_wstring type, and conversion back the
			// other way to interface with char[].
			// 
			// 2. store a tu_wstring as a union with
			// tu_string?  Extra complexity.
			//
			// 3. ??
			//
			// Storing UTF-8 seems like a pretty decent
			// way to do it.  Everything else just
			// continues to work.

#if !defined(_WIN32) && (WCHAR_MAX > 0xFFFF)
			// [CHOSEN, not recovered] MAXINT is undefined here, so the stock test below picked
			// the 16-bit decoder for the 32-bit POSIX wchar_t. Decode native characters.
			tu_string::encode_utf8_from_wchar(&m_string_value, (const uint32 *)wstr);
#elif (WCHAR_MAX != MAXINT)
			tu_string::encode_utf8_from_wchar(&m_string_value, (const uint16 *)wstr);
#else
# if (WCHAR_MAX != MAXSHORT)
# error "Can't determine the size of wchar_t"
# else
			tu_string::encode_utf8_from_wchar(&m_string_value, (const uint32 *)wstr);
# endif
#endif
		}

		as_value(bool val)
			:
			m_type(BOOLEAN),
			m_boolean_value(val)
		{
		}

		as_value(int val)
			:
			m_type(NUMBER),
			m_number_value(double(val))
		{
		}

		as_value(float val)
			:
			m_type(NUMBER),
			m_number_value(double(val))
		{
		}

		as_value(double val)
			:
			m_type(NUMBER),
			m_number_value(val)
		{
		}

		as_value(as_object_interface* obj);

		as_value(as_c_function_ptr func)
			:
			m_type(C_FUNCTION),
			m_c_function_value(func)
		{
			m_c_function_value = func;
		}

		as_value(as_as_function* func);

		~as_value() { drop_refs(); }

		// Useful when changing types/values.
		void	drop_refs();

		type	get_type() const { return m_type; }

		// Return true if this value is callable.
		bool is_function() const
		{
			return m_type == C_FUNCTION || m_type == AS_FUNCTION;
		}

		const char*	to_string() const;
		const tu_string&	to_tu_string() const;
		const tu_string&	to_tu_string_versioned(int version) const;
		const tu_stringi&	to_tu_stringi() const;
		double	to_number() const;
		bool	to_bool() const;
		as_object_interface*	to_object() const;
		as_c_function_ptr	to_c_function() const;
		as_as_function*	to_as_function() const;

		void	convert_to_number();
		void	convert_to_string();
		void	convert_to_string_versioned(int version);

		// These set_*()'s are more type-safe; should be used
		// in preference to generic overloaded set().  You are
		// more likely to get a warning/error if misused.
		void	set_tu_string(const tu_string& str) { drop_refs(); m_type = STRING; m_string_value = str; }
		void	set_string(const char* str) { drop_refs(); m_type = STRING; m_string_value = str; }
		void	set_double(double val) { drop_refs(); m_type = NUMBER; m_number_value = val; }
		void	set_bool(bool val) { drop_refs(); m_type = BOOLEAN; m_boolean_value = val; }
		void	set_int(int val) { set_double(val); }
		void	set_as_object_interface(as_object_interface* obj);
		void	set_as_c_function_ptr(as_c_function_ptr func)
		{
			drop_refs(); m_type = C_FUNCTION; m_c_function_value = func;
		}
		void	set_as_as_function(as_as_function* func);
		void	set_undefined() { drop_refs(); m_type = UNDEFINED; }
		void	set_null() { drop_refs(); m_type = NULLTYPE; }

		void	operator=(const as_value& v)
		{
			if (v.m_type == UNDEFINED) set_undefined();
			else if (v.m_type == NULLTYPE) set_null();
			else if (v.m_type == BOOLEAN) set_bool(v.m_boolean_value);
			else if (v.m_type == STRING) set_tu_string(v.m_string_value);
			else if (v.m_type == NUMBER) set_double(v.m_number_value);
			else if (v.m_type == OBJECT) set_as_object_interface(v.m_object_value);
			else if (v.m_type == C_FUNCTION) set_as_c_function_ptr(v.m_c_function_value);
			else if (v.m_type == AS_FUNCTION) set_as_as_function(v.m_as_function_value);
		}

		bool	operator==(const as_value& v) const;
		bool	operator!=(const as_value& v) const;
		bool	operator<(const as_value& v) const { return to_number() < v.to_number(); }
		void	operator+=(const as_value& v) { set_double(this->to_number() + v.to_number()); }
		void	operator-=(const as_value& v) { set_double(this->to_number() - v.to_number()); }
		void	operator*=(const as_value& v) { set_double(this->to_number() * v.to_number()); }
		void	operator/=(const as_value& v) { set_double(this->to_number() / v.to_number()); }  // @@ check for div/0
		void	operator&=(const as_value& v) { set_int(int(this->to_number()) & int(v.to_number())); }
		void	operator|=(const as_value& v) { set_int(int(this->to_number()) | int(v.to_number())); }
		void	operator^=(const as_value& v) { set_int(int(this->to_number()) ^ int(v.to_number())); }
		void	shl(const as_value& v) { set_int(int(this->to_number()) << int(v.to_number())); }
		void	asr(const as_value& v) { set_int(int(this->to_number()) >> int(v.to_number())); }
#if defined(__APPLE__)
		// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
		void	lsr(const as_value& v) { set_int((bf2x87::lowUInt32(this->to_number()) >> int(v.to_number()))); }
#else
		void	lsr(const as_value& v) { set_int((Uint32(this->to_number()) >> int(v.to_number()))); }
#endif

		void	string_concat(const tu_string& str);

		tu_string* get_mutable_tu_string() { assert(m_type == STRING); return &m_string_value; }
	};


// tulrich: I'm not too sure this is useful.  For things like
// xml_as_object, is it sufficient to always store the event handlers
// as ordinary members using their canonical names, instead of this
// special table?  I have a feeling that's what Macromedia does
// (though I'm not sure).
#if 0
	// This class is just as_object_interface, with an event
	// handler table added.
	struct as_object_with_handlers : public as_object_interface
	{
                // ActionScript event handler table.
                hash<event_id, gameswf::as_value>        m_event_handlers;

                // ActionScript event handler.
                void    set_event_handler(event_id id, const as_value& method)
                {
                        // m_event_handlers.push_back(as);
                        //m_event_handlers.set(id, method);
                }

                bool    get_event_handler(event_id id, gameswf::as_value* result)
                {
                        //return m_event_handlers.get(id, result);
			return false;
                }
	};
#endif // 0


	//
	// as_prop_flags
	//
	// flags defining the level of protection of a member
	struct as_prop_flags
	{
		// Numeric flags
		int m_flags;
		// if true, this value is protected (internal to gameswf)
		bool m_is_protected;

		// mask for flags
		const static int as_prop_flags_mask = 0x7;

		// Default constructor
		as_prop_flags() : m_flags(0), m_is_protected(false)
		{
		}

		// Constructor
		as_prop_flags(const bool read_only, const bool dont_delete, const bool dont_enum)
			:
			m_flags(((read_only) ? 0x4 : 0) | ((dont_delete) ? 0x2 : 0) | ((dont_enum) ? 0x1 : 0)),
			m_is_protected(false)
		{
		}

		// Constructor, from numerical value
		as_prop_flags(const int flags)
			: m_flags(flags), m_is_protected(false)
		{
		}

		// accessor to m_readOnly
		bool get_read_only() const { return (((this->m_flags & 0x4)!=0)?true:false); }

		// accessor to m_dontDelete
		bool get_dont_delete() const { return (((this->m_flags & 0x2)!=0)?true:false); }

		// accessor to m_dontEnum
		bool get_dont_enum() const { return (((this->m_flags & 0x1)!=0)?true:false);	}

		// accesor to the numerical flags value
		int get_flags() const { return this->m_flags; }

		// accessor to m_is_protected
		bool get_is_protected() const { return this->m_is_protected; }
		// setter to m_is_protected
		void set_get_is_protected(const bool is_protected) { this->m_is_protected = is_protected; }

		// set the numerical flags value (return the new value )
		// If unlocked is false, you cannot un-protect from over-write,
		// you cannot un-protect from deletion and you cannot
		// un-hide from the for..in loop construct
		int set_flags(const int setTrue, const int set_false = 0)
		{
			if (!this->get_is_protected())
			{
				this->m_flags = this->m_flags & (~set_false);
				this->m_flags |= setTrue;
			}

			return get_flags();
		}
	};

	//
	// as_member
	//
	// member for as_object: value + flags
	struct as_member {
		// value
		as_value m_value;
		// Properties flags
		as_prop_flags m_flags;

		// Default constructor
		as_member() {
		}

		// Constructor
		as_member(const as_value &value,const as_prop_flags flags=as_prop_flags())
			:
			m_value(value),
			m_flags(flags)
		{
		}

		// accessor to the value
		as_value get_member_value() const { return m_value; }
		// accessor to the properties flags
		as_prop_flags get_member_flags() const { return m_flags; }

		// set the value
		void set_member_value(const as_value &value)  { m_value = value; }
		// accessor to the properties flags
		void set_member_flags(const as_prop_flags &flags)  { m_flags = flags; }
	};


	//
	// as_object
	//
	// A generic bag of attributes.  Base-class for ActionScript
	// script-defined objects.
	// [DICE-MOD SwiffPlayer: the AS2 `Object.addProperty(name, getter, setter)` builtin. Stock 2006
	// gameswf has no getter/setter properties at all, and menu.swf calls addProperty 36 times in its
	// onLoad (measured with BF2_SWIFFPLAYER_ASLOG=1). DICE hit the same gap: their 56-byte AS object
	// class registers exactly one method, "addProperty" (sub_1009A320 -> gameswf_as_object_addproperty).
	// Defined in gameswf_action.cpp. See GAMESWF_DIVERGENCES.md.]
	void	as_object_addproperty(const fn_call& fn);

	struct as_object : public as_object_interface
	{
		stringi_hash<as_member>	m_members;
		as_object_interface*	m_prototype;

		// [DICE-MOD SwiffPlayer: addProperty storage - the getter/setter function per property name.
		// A name present in m_getters/m_setters shadows any plain member of the same name.]
		stringi_hash<as_value>	m_getters;
		stringi_hash<as_value>	m_setters;

		virtual as_object*	cast_to_as_object() { return this; }

		// [DICE-MOD SwiffPlayer #49] The shipped as_object vtable slot +0x28 (sub_10099CB0): push the
		// names a `for (k in obj)` loop visits - the prototype chain first (virtual, so it recurses
		// base-most first), then this object's own members whose VALUE is a string, number, boolean
		// or object (shipped types 1..4); undefined and both function types are skipped, and the
		// shipped member entry carries no flags, so there is no DontEnum test. Arrays override it
		// (sub_1009D020) to push only their element indices. Enumerate (0x46) and Enumerate2 (0x55)
		// call this after pushing the null terminator; stock 2006 walked m_members inline, methods
		// included, so a for-in over an Array met its own push/pop/... C functions.
		virtual void	enumerate(as_environment* env);


		// Declared here, defined in gameswf_action.cpp (they need call_method to invoke the
		// getter/setter, and a scratch as_environment to pass its arguments through).
		void	add_property(const tu_stringi& name, const as_value& getter, const as_value& setter);
		bool	get_property(const tu_stringi& name, as_value* val);	// true if a getter handled it
		bool	set_property(const tu_stringi& name, const as_value& val);	// true if a setter handled it
		// [DICE-MOD SwiffPlayer #28: a getter/setter registered on a CLASS PROTOTYPE applies to every
		// instance, called with the instance as `this`. mainMenu.swf registers 177 of them on the
		// component prototypes (`UIObject.prototype.addProperty("__authorMode", __get__..., __set__...)`)
		// and the construct handlers then assign `__authorMode = false` on the instance. The lookup
		// walks the prototype chain; the call keeps the original receiver.]
		bool	find_getter(const tu_stringi& name, as_value* getter, int depth = 0);
		bool	find_setter(const tu_stringi& name, as_value* setter, int depth = 0);
		void	note_function_home(const as_value& fn);	// [DICE-MOD #30] out of line: as_as_function is incomplete here
		static as_value	invoke_getter(const as_value& getter, as_object_interface* self);
		static void	invoke_setter(const as_value& setter, as_object_interface* self, const as_value& val);

		as_object() : m_prototype(NULL) { }
		as_object(as_object_interface* proto) : m_prototype(proto)
		{
			if (m_prototype)
			{
				m_prototype->add_ref();
			}
		}

		virtual ~as_object()
		{
			if (m_prototype)
			{
				m_prototype->drop_ref();
			}
		}
		
		virtual const char*	get_text_value() const { return NULL; }

		virtual void	set_member(const tu_stringi& name, const as_value& val ) {
			//printf("SET MEMBER: %s at %p for object %p\n", name.c_str(), val.to_object(), this);
			// [DICE-MOD SwiffPlayer #30: remember where a function was first stored (see m_home_object).]
			if (val.get_type() == as_value::AS_FUNCTION && name != "prototype" && name != "__constructor__")
			{
				note_function_home(val);
			}
			if (name == "prototype")
			{
				if (m_prototype) m_prototype->drop_ref();
				m_prototype = val.to_object();
				if (m_prototype) m_prototype->add_ref();
			}
			else if (set_property(name, val))	// [DICE-MOD] an addProperty setter takes precedence
			{
			}
			else
			{
				stringi_hash<as_member>::const_iterator it = this->m_members.find(name);
				
				if ( it != this->m_members.end() ) {

					const as_prop_flags flags = (it.get_value()).get_member_flags();

					// is the member read-only ?
					if (!flags.get_read_only()) {
						m_members.set(name, as_member(val, flags));
					}

				} else {
					m_members.set(name, as_member(val));
				}
			}
		}

		virtual bool	get_member(const tu_stringi& name, as_value* val)
		{
			//printf("GET MEMBER: %s at %p for object %p\n", name.c_str(), val, this);
			if (name == "prototype")
			{
				val->set_as_object_interface(m_prototype);
				return true;
			}
			else if (get_property(name, val))	// [DICE-MOD] an addProperty getter takes precedence
			{
				return true;
			}
			else {
				as_member m;

				if (m_members.get(name, &m) == false)
				{
					if (m_prototype != NULL)
					{
						if (m_prototype->get_member(name, val))
						{
							return true;
						}
					}
					// [DICE-MOD SwiffPlayer: expose the AS2 `addProperty` builtin on every object,
					// as the real player does via Object.prototype. Checked LAST, so a real member
					// or a prototype member of the same name still wins.]
					if (name == "addProperty")
					{
						val->set_as_c_function_ptr(as_object_addproperty);
						return true;
					}
					return false;
				} else {
					*val=m.get_member_value();
					return true;
				}
			}
			return true;
		}

		virtual bool get_member(const tu_stringi& name, as_member* member) const
		{
			//printf("GET MEMBER: %s at %p for object %p\n", name.c_str(), val, this);
			assert(member != NULL);
			return m_members.get(name, member);
		}

		virtual bool	set_member_flags(const tu_stringi& name, const int flags)
		{
			as_member member;
			if (this->get_member(name, &member)) {
				as_prop_flags f = member.get_member_flags();
				f.set_flags(flags);
				member.set_member_flags(f);

				m_members.set(name, member);

				return true;
			}

			return false;
		}

		virtual movie*	to_movie()
		// This object is not a movie; no conversion.
		{
			return NULL;
		}

		void	clear()
		{
			m_members.clear();
			if (m_prototype)
			{
				m_prototype->drop_ref();
				m_prototype = NULL;
			}
		}
	};


	//
	// as_as_function
	//
	// ActionScript function.

	struct as_as_function : public ref_counted
	{
		action_buffer*	m_action_buffer;
		as_environment*	m_env;	// @@ might need some kind of ref count here, but beware cycles
		array<with_stack_entry>	m_with_stack;	// initial with-stack on function entry.
		int	m_start_pc;
		int	m_length;
		struct arg_spec
		{
			int	m_register;
			tu_string	m_name;
		};
		array<arg_spec>	m_args;
		bool	m_is_function2;
		uint8	m_local_register_count;	// parsed; sizes nothing since #55 (the shipped call saves a fixed r0..r9)
		uint16	m_function2_flags;	// used by function2 to control implicit arg register assignments

		// ActionScript functions have a property namespace!
		// Typically used for class constructors, for "prototype", "constructor",
		// and class properties.
		as_object*	m_properties;
		// [DICE-MOD SwiffPlayer #30: the object this function was first stored on as a member - for a
		// class method, the class PROTOTYPE. `super` inside a method is the parent of the class the
		// method was DEFINED on (that prototype's __constructor__, left there by ActionExtends), not
		// anything derived from `this`, or a two-level chain would loop. Stock 2006 gameswf set the
		// function2 super register to nothing ("TODO: implement 'super'"), so every subclass
		// init/draw/arrange's super.xxx() call died on an undefined object once the real subclass
		// methods ran. Raw pointer, unowned: prototypes outlive the movie's functions.]
		as_object*	m_home_object;

		// NULL environment is allowed -- if so, then
		// functions will be executed in the caller's
		// environment, rather than the environment where they
		// were defined.
		as_as_function(action_buffer* ab, as_environment* env, int start, const array<with_stack_entry>& with_stack)
			:
			m_action_buffer(ab),
			m_env(env),
			m_with_stack(with_stack),
			m_start_pc(start),
			m_length(0),
			m_is_function2(false),
			m_local_register_count(0),
			m_function2_flags(0),
			m_properties(NULL),
			m_home_object(NULL)
		{
			assert(m_action_buffer);
		}

		// Releases the property object lazy_create_properties took a reference on (stock 2006 had
		// no destructor and leaked it with every function used as an object).
		// [CHOSEN, not recovered] issue #7 (https://github.com/B2BF/OpenBF2/issues/7).
		~as_as_function()
		{
			if (m_properties)
			{
				m_properties->drop_ref();
			}
		}

		void	set_is_function2() { m_is_function2 = true; }
		void	set_local_register_count(uint8 ct) { assert(m_is_function2); m_local_register_count = ct; }
		void	set_function2_flags(uint16 flags) { assert(m_is_function2); m_function2_flags = flags; }

		void	add_arg(int arg_register, const char* name)
		{
			assert(arg_register == 0 || m_is_function2 == true);
			m_args.resize(m_args.size() + 1);
			m_args.back().m_register = arg_register;
			m_args.back().m_name = name;
		}

		void	set_length(int len) { assert(len >= 0); m_length = len; }

		// Dispatch.
		void	operator()(const fn_call& fn);

		void	lazy_create_properties()
		// This ensures that this as_function has a valid
		// prototype in its properties.  This is done lazily
		// so that functions/methods which are not used as
		// constructors don't carry along extra unnecessary
		// baggage.
		{
			if (m_properties == NULL)
			{
				m_properties = new as_object();
				m_properties->add_ref();

				// Create new empty prototype
				as_value	proto(new as_object());
				m_properties->set_member("prototype", proto);
			}
		}
	};


	// ActionScript "environment", essentially VM state?
	struct as_environment
	{
		array<as_value>	m_stack;
		// [DICE-MOD SwiffPlayer #55] ONE register bank per environment, 256 slots, shared by timeline
		// code and every function body that executes in this environment - the shipped
		// as_environment carries `as_value registers[256]` at +0x10 (ctor 0x10077D00), and
		// StoreRegister / the push-register operand address env+0x10+32*reg for all code (execute
		// 0x100925A0, case 0x87 and push type 4). Stock 2006 kept four timeline registers plus a
		// FRESH stack of local registers per DefineFunction2 call, so an argument the caller did not
		// pass read undefined; in the shipped player it reads whatever the environment's register
		// already held (see as_as_function::operator()). Sized to 256 on first use.
		array<as_value>	m_registers;
		movie*	m_target;
		stringi_hash<as_value>	m_variables;

		// For local vars.  Use empty names to separate frames.
		struct frame_slot
		{
			tu_string	m_name;
			as_value	m_value;

			frame_slot() {}
			frame_slot(const tu_string& name, const as_value& val) : m_name(name), m_value(val) {}
		};
		array<frame_slot>	m_local_frames;

		// [DICE-MOD SwiffPlayer: the running function's bound `this` object and its superclass
		// constructor (`super`). The shipped as_environment keeps them at +8212 (checked by
		// get_variable_raw before the target movie's members, used as `this` by CallMethod's
		// direct-call arm) and +8216 (preloaded into the `super` register/local at function entry).
		// Stock 2006 bound `this` to the target MOVIE for every call and never implemented `super`,
		// so a class constructor run on a plain object or another sprite acted on the wrong object
		// and super() went nowhere. Non-owning; saved/restored around each call.]
		as_object_interface*	m_this_object;
		as_as_function*		m_super_ctor;

		// [DICE-MOD SwiffPlayer #57] While a function body defined in this environment runs for
		// ANOTHER environment, that caller's register bank; NULL otherwise (this environment's own).
		// Set and restored around each call by as_as_function::operator(). Non-owning.
		as_environment*	m_register_env;

		as_environment()
			:
			m_target(0),
			m_this_object(0),
			m_super_ctor(0),
			m_register_env(0)
		{
		}

		movie*	get_target() { return m_target; }
		void	set_target(movie* target) { m_target = target; }

		// stack access/manipulation
		// @@ TODO do more checking on these
		template<class T>
		void	push(T val) { push_val(as_value(val)); }
		void	push_val(const as_value& val) { m_stack.push_back(val); }
		// [DICE-MOD SwiffPlayer: MAKE THE VALUE STACK UNDERFLOW-TOLERANT, as the real Flash player is.
		//
		// menu.swf contains stray `Pop` (0x17) opcodes: at one site the bytecode is
		//     Push'_global'; GetVariable; Push'dice'; GetMember; Not; Not; BranchIfTrue +24
		//     [157..180: Push'_global'; GetVariable; Push'dice',0,'Object'; New; SetMember]
		//     181: Pop
		// Every one of those opcodes has its SPEC-CORRECT stack effect in this gameswf (verified by
		// instrumenting the VM with a depth-tracking opcode ring), and BOTH the taken and the
		// fall-through path reach pc=181 with an EMPTY stack - so the Pop has nothing to pop. The
		// movie is simply unbalanced there; ~20 such sites execute in its onLoad.
		//
		// The real Flash player - and DICE's gameswf, which ran this exact movie - absorb that: a pop
		// on an empty stack yields `undefined` and execution continues. Stock gameswf instead indexes
		// the array out of bounds (`m_stack.back()` at index -1) or resizes it NEGATIVE, and the
		// negative resize destructs from a negative index -> ~as_value on garbage -> CRASH. That crash,
		// not the SWF, is the defect.
		//
		// So: pop/top/bottom/drop all clamp and yield undefined rather than fault. This is FAITHFUL
		// behaviour (matching the shipped player), not a workaround for our own integration.
		// Indexed in Code/BF2/External/gameswf/GAMESWF_DIVERGENCES.md.]
		static as_value&	underflow_slot() { static as_value s_undef; s_undef.set_undefined(); return s_undef; }

		as_value	pop()
		{
			if (m_stack.size() <= 0) return as_value();	// undefined
			as_value result = m_stack.back(); m_stack.pop_back(); return result;
		}
		as_value&	top(int dist)
		{
			int i = m_stack.size() - 1 - dist;
			if (i < 0 || i >= m_stack.size()) return underflow_slot();
			return m_stack[i];
		}
		as_value&	bottom(int index)
		{
			if (index < 0 || index >= m_stack.size()) return underflow_slot();
			return m_stack[index];
		}
		// [DICE-MOD SwiffPlayer: clamp instead of resizing NEGATIVE - see the underflow note above
		// pop()/top(). This is the site that actually crashed: resize(negative) destructs from a
		// negative index.]
		void	drop(int count) { int n = m_stack.size() - count; if (n < 0) n = 0; m_stack.resize(n); }

		int	get_top_index() const { return m_stack.size() - 1; }

		as_value	get_variable(const tu_string& varname, const array<with_stack_entry>& with_stack) const;
		// no path stuff:
		as_value	get_variable_raw(const tu_string& varname, const array<with_stack_entry>& with_stack) const;

		void	set_variable(const tu_string& path, const as_value& val, const array<with_stack_entry>& with_stack);
		// no path stuff:
		void	set_variable_raw(const tu_string& path, const as_value& val, const array<with_stack_entry>& with_stack);

		void	set_local(const tu_string& varname, const as_value& val);
		void	add_local(const tu_string& varname, const as_value& val);	// when you know it doesn't exist.
		void	declare_local(const tu_string& varname);	// Declare varname; undefined unless it already exists.

		bool	get_member(const tu_stringi& varname, as_value* val) const;
		void	set_member(const tu_stringi& varname, const as_value& val);

		// Parameter/local stack frame management.
		int	get_local_frame_top() const { return m_local_frames.size(); }
		// [DICE-MOD SwiffPlayer #41] true while a function frame (an empty-named barrier slot) is active.
		bool	in_function_frame() const;
		void	set_local_frame_top(int t) { assert(t <= m_local_frames.size()); m_local_frames.resize(t); }
		void	add_frame_barrier() { m_local_frames.push_back(frame_slot()); }
		// [DICE-MOD SwiffPlayer #41] the barrier a FUNCTION call pushes, marked (a BOOLEAN value) so a
		// frame script's own barrier (an undefined value, pushed by the plain execute overload) can be
		// told apart: a `var` declared above a function barrier is a timeline var, a clip property.
		void	add_function_barrier() { m_local_frames.push_back(frame_slot(tu_string(""), as_value(true))); }

		// [DICE-MOD SwiffPlayer #55] the environment's register `reg` (the operand is a byte, so
		// 0..255 is always a valid slot). Never NULL.
		as_value*	register_ptr(int reg);
		// [DICE-MOD SwiffPlayer #57] the environment whose bank register operands address here.
		as_environment*	register_bank() { return m_register_env ? m_register_env : this; }

		// Internal.
		int	find_local(const tu_string& varname) const;
		bool	parse_path(const tu_string& var_path, tu_string* path, tu_string* var) const;
		movie*	find_target(const tu_string& path) const;
		movie*	find_target(const as_value& val) const;
		// [DICE-MOD SwiffPlayer: dotted-path resolution through OBJECT members, with the shipped
		// _global fallback - the resolver get_variable/set_variable use. See the .cpp.]
		as_object_interface*	find_object_path(const tu_string& path, as_object_interface* start) const;

		// Movie teardown: appends every object or function held on the stack, in the registers,
		// the variables and the local frames to *held, then empties them.
		// [CHOSEN, not recovered] issue #7; see release_script_objects.
		void	release_values(array<as_value>* held);
	};


	// Appends v to *held when it is an object or an ActionScript function, the only values that
	// can keep another object alive. A helper of the movie teardown (release_script_objects).
	// [CHOSEN, not recovered] issue #7.
	inline void	hold_script_value(array<as_value>* held, const as_value& v)
	{
		if (v.get_type() == as_value::OBJECT || v.get_type() == as_value::AS_FUNCTION)
		{
			held->push_back(v);
		}
	}

	// Breaks every script reference cycle between the live script objects, so plain reference
	// counting can free them once their owners let go. Every registered clip and object (see
	// register_script_object), reachable from the movie or not, is cleared: a clip's environment,
	// event handlers and children (without UNLOAD events), an object's members, accessors and
	// prototype. Runs no ActionScript. Afterwards every script object is unusable and _global is
	// empty: call it only when the whole player is going away, right before its movie's last
	// reference is dropped and _global is reset, and only when no other player is alive
	// (SwiffPlayer::releaseMovie counts them), so nothing still in use is cleared.
	// [CHOSEN, not recovered] issue #7 (https://github.com/B2BF/OpenBF2/issues/7): our gameswf
	// replaces the shipped player, whose own teardown is not recovered.
	void	release_script_objects();

	// Drops the Object.registerClass bindings of every library definition (gameswf_impl.cpp).
	// Part of release_script_objects. [CHOSEN, not recovered] issue #7.
	void	release_library_classes();


	// Parameters/environment for C functions callable from ActionScript.
	struct fn_call
	{
		as_value* result;
		as_object_interface* this_ptr;
		as_environment* env;
		int nargs;
		int first_arg_bottom_index;

		fn_call(as_value* res_in, as_object_interface* this_in, as_environment* env_in, int nargs_in, int first_in)
			:
			result(res_in),
			this_ptr(this_in),
			env(env_in),
			nargs(nargs_in),
			first_arg_bottom_index(first_in)
		{
		}

		as_value& arg(int n) const
		// Access a particular argument.
		{
			assert(n < nargs);
			return env->bottom(first_arg_bottom_index - n);
		}
	};


	//
	// Some handy helpers
	//

	// Clean up any stray heap stuff we've allocated.
	void	action_clear();

	// Dispatching methods from C++.
	as_value	call_method0(const as_value& method, as_environment* env, as_object_interface* this_ptr);
	as_value	call_method1(
		const as_value& method, as_environment* env, as_object_interface* this_ptr,
		const as_value& arg0);
	as_value	call_method2(
		const as_value& method, as_environment* env, as_object_interface* this_ptr,
		const as_value& arg0, const as_value& arg1);
	as_value	call_method3(
		const as_value& method, as_environment* env, as_object_interface* this_ptr,
		const as_value& arg0, const as_value& arg1, const as_value& arg2);


	const char*	call_method_parsed(
		as_environment* env,
		as_object_interface* this_ptr,
		const char* method_name,
		const char* method_arg_fmt,
		va_list args);

	// tulrich: don't use this!  To register a class constructor,
	// just assign the classname to the constructor function.  E.g.:
	//
	// my_movie->set_member("MyClass", as_value(MyClassConstructorFunction));
	// 
	//void register_as_object(const char* object_name, as_c_function_ptr handler);

	// [DICE-MOD SwiffPlayer: accessor for the _global object, so the SwiffPlayer AS bridge can hang
	// the dice.* namespace graph off it before the first advance. Forces action_init(). See
	// gameswf_action.cpp and GAMESWF_DIVERGENCES.md.]
	as_object*	get_global_object();

	// Numerical indices for standard member names.  Can use this
	// to help speed up get/set member calls, by using a switch()
	// instead of nasty string compares.
	enum as_standard_member
	{
		M_INVALID_MEMBER = -1,
		M_X,
		M_Y,
		M_XSCALE,
		M_YSCALE,
		M_CURRENTFRAME,
		M_TOTALFRAMES,
		M_ALPHA,
		M_VISIBLE,
		M_WIDTH,
		M_HEIGHT,
		M_ROTATION,
		M_TARGET,
		M_FRAMESLOADED,
		M_NAME,
		M_DROPTARGET,
		M_URL,
		M_HIGHQUALITY,
		M_FOCUSRECT,
		M_SOUNDBUFTIME,
		M_XMOUSE,
		M_YMOUSE,
		M_PARENT,
		M_TEXT,
		M_TEXTWIDTH,
		M_TEXTCOLOR,
		M_ONLOAD,

		AS_STANDARD_MEMBER_COUNT
	};
	// Return the standard enum, if the arg names a standard member.
	// Returns M_INVALID_MEMBER if there's no match.
	as_standard_member	get_standard_member(const tu_stringi& name);

}	// end namespace gameswf


#endif // GAMESWF_ACTION_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
