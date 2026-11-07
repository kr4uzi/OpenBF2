// gameswf_impl.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some implementation code for the gameswf SWF player library.


#ifndef GAMESWF_IMPL_H
#define GAMESWF_IMPL_H


#include "gameswf.h"
#include "gameswf_action.h"
#include "gameswf_types.h"
#include "gameswf_log.h"
#include <assert.h>
#include "base/container.h"
#include "base/utility.h"
#include "base/smart_ptr.h"
#include <stdarg.h>


namespace jpeg { struct input; }


namespace gameswf
{
	struct action_buffer;
	struct bitmap_character_def;
        struct bitmap_info;
	struct character;
	struct character_def;
	struct display_info;
	struct execute_tag;
	struct font;
	struct movie_root;
	struct sound_sample : public resource //virtual public ref_counted
	{
		virtual sound_sample*	cast_to_sound_sample() { return this; }
	};
	struct stream;
	struct swf_event;

	void save_extern_movie(movie_interface* m);

	// Extra internal interfaces added to movie_definition
	struct movie_definition_sub : public movie_definition
	{
		virtual const array<execute_tag*>&	get_playlist(int frame_number) = 0;
		virtual const array<execute_tag*>*	get_init_actions(int frame_number) = 0;
		virtual smart_ptr<resource>	get_exported_resource(const tu_string& symbol) = 0;
		// [DICE-MOD SwiffPlayer #37: the name a text field's font reaches the renderer under. The
		// shipped field asks its movie definition (vslot +60) for a font NAME by id, and the golden
		// stream shows every line of the login page created as "StandardTextBold" - the EXPORT name
		// (ExportAssets) of the font whose face name the field's font shares. [INFERRED: the slot's
		// body is not located; the rule reproduces the golden.] False when no exported font matches.]
		virtual bool	get_exported_font_name(const font* f, tu_string* out) { return false; }
		virtual character_def*	get_character_def(int id) = 0;
		// Movie teardown: drops the classes Object.registerClass bound to this definition's
		// exports, so a definition kept in the library for the next player holds no function of
		// the released movie. [CHOSEN, not recovered] issue #7; see release_script_objects.
		virtual void	release_registered_classes() {}

		virtual bool	get_labeled_frame(const char* label, int* frame_number) = 0;

		// For use during creation.
		virtual int	get_loading_frame() const = 0;
		virtual void	add_character(int id, character_def* ch) = 0;
		virtual void	add_font(int id, font* ch) = 0;
		virtual font*	get_font(int id) = 0;
		virtual void	add_execute_tag(execute_tag* c) = 0;
		virtual void	add_init_action(int sprite_id, execute_tag* c) = 0;
		virtual void	add_frame_name(const char* name) = 0;
		virtual void	set_jpeg_loader(jpeg::input* j_in) = 0;
		virtual jpeg::input*	get_jpeg_loader() = 0;
		virtual bitmap_character_def*	get_bitmap_character(int character_id) = 0;
		virtual void	add_bitmap_character(int character_id, bitmap_character_def* ch) = 0;
		virtual sound_sample*	get_sound_sample(int character_id) = 0;
		virtual void	add_sound_sample(int character_id, sound_sample* sam) = 0;
		virtual void	export_resource(const tu_string& symbol, resource* res) = 0;
		virtual void	add_import(const char* source_url, int id, const char* symbol_name) = 0;
		virtual void	add_bitmap_info(bitmap_info* ch) = 0;

		virtual create_bitmaps_flag	get_create_bitmaps() const = 0;
		virtual create_font_shapes_flag	get_create_font_shapes() const = 0;
	};


	// For internal use.
	movie_definition_sub*	create_movie_sub(const char* filename);
	movie_definition_sub*	create_library_movie_sub(const char* filename);
	movie_interface*	create_library_movie_inst_sub(movie_definition_sub* md);

//v for extern movies
	movie_interface*	create_library_movie_inst(movie_definition* md);
	movie_interface*        get_current_root();
	void set_current_root(movie_interface* m);
	const char* get_workdir();
	void set_workdir(const char* dir);
	void delete_unused_root();

	struct movie : public movie_interface
	{
		virtual void set_extern_movie(movie_interface* m) { }
		virtual movie_interface*	get_extern_movie() { return NULL; }

		virtual movie_definition*	get_movie_definition() { return NULL; }
		virtual movie_root*	        get_root() { return NULL; }
		virtual movie_interface*	get_root_interface() { return NULL; }
		virtual movie*	                get_root_movie() { return NULL; }

		virtual float	                get_pixel_scale() const { return 1.0f; }
		virtual character*	        get_character(int id) { return NULL; }
		// [DICE-MOD SwiffPlayer: a SAFE downcast to character, the cast_to_font / cast_to_as_object
		// pattern. The Color builtin's setRGB needs its target's cxform (see gameswf_action.cpp).]
		virtual character*	        cast_to_character() { return NULL; }

		virtual matrix	                get_world_matrix() const { return matrix::identity; }
		virtual cxform	                get_world_cxform() const { return cxform::identity; }

		//
		// display-list management.
		//

		virtual execute_tag*	find_previous_replace_or_add_tag(int current_frame, int depth, int id)
		{
			return NULL;
		}

		virtual character*	add_display_object(
			Uint16 character_id,
			const char*		 name,
			const array<swf_event*>& event_handlers,
			Uint16			 depth,
			bool			 replace_if_depth_is_occupied,
			const cxform&		 color_transform,
			const matrix&		 mat,
			float			 ratio,
			Uint16			clip_depth)
		{
			return NULL;
		}

		virtual void	move_display_object(
			Uint16		depth,
			bool		use_cxform,
			const cxform&	color_transform,
			bool		use_matrix,
			const matrix&	mat,
			float		ratio,
			Uint16		clip_depth)
		{
		}

		virtual void	replace_display_object(
			Uint16		character_id,
			const char*	name,
			Uint16		depth,
			bool		use_cxform,
			const cxform&	color_transform,
			bool		use_matrix,
			const matrix&	mat,
			float		ratio,
			Uint16		clip_depth)
		{
		}

		virtual void	replace_display_object(
			character*	ch,
			const char*	name,
			Uint16		depth,
			bool		use_cxform,
			const cxform&	color_transform,
			bool		use_matrix,
			const matrix&	mat,
			float		ratio,
			Uint16		clip_depth)
		{
		}

		virtual void	remove_display_object(Uint16 depth, int id)	{}

		virtual void	set_background_color(const rgba& color) {}
		virtual rgba	get_background_color() const { return rgba(); }	// [DICE-MOD SwiffPlayer: see gameswf.h]
		virtual void	set_background_alpha(float alpha) {}
		virtual float	get_background_alpha() const { return 1.0f; }
		virtual void	set_display_viewport(int x0, int y0, int width, int height) {}

		virtual void	add_action_buffer(action_buffer* a) { assert(0); }

		virtual void	goto_frame(int target_frame_number) { assert(0); }
		virtual bool	goto_labeled_frame(const char* label) { assert(0); return false; }

		virtual void	set_play_state(play_state s) {}
		virtual play_state	get_play_state() const { assert(0); return STOP; }

		virtual void	notify_mouse_state(int x, int y, int buttons)
		// The host app uses this to tell the movie where the
		// user's mouse pointer is.
		{
		}

		virtual void	get_mouse_state(int* x, int* y, int* buttons)
		// Use this to retrieve the last state of the mouse, as set via
		// notify_mouse_state().
		{
			assert(0);
		}

		struct drag_state
		{
			movie*	m_character;
			bool	m_lock_center;
			bool	m_bound;
			float	m_bound_x0;
			float	m_bound_y0;
			float	m_bound_x1;
			float	m_bound_y1;

			drag_state()
				:
				m_character(0), m_lock_center(0), m_bound(0),
				m_bound_x0(0), m_bound_y0(0), m_bound_x1(1), m_bound_y1(1)
			{
			}
		};
		virtual void	get_drag_state(drag_state* st) { assert(0); *st = drag_state(); }
		virtual void	set_drag_state(const drag_state& st) { assert(0); }
		virtual void	stop_drag() { assert(0); }


		// External
		virtual void	set_variable(const char* path_to_var, const char* new_value)
		{
			assert(0);
		}

		// External
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value)
		{
			assert(0);
		}

		// External
		virtual const char*	get_variable(const char* path_to_var) const
		{
			assert(0);
			return "";
		}

		virtual void * get_userdata() { assert(0); return NULL; }
		virtual void set_userdata(void *) { assert(0); }

		// External
		virtual bool	has_looped() const { return true; }


		//
		// Mouse/Button interface.
		//

		// [DICE-MOD SwiffPlayer #51] The shipped player has no "topmost mouse entity" pass. Every
		// character runs its own per-frame mouse state machine (vtable slot 4, mouse_update): the root
		// sprite is asked once per advance, a sprite handles itself and then every visible child from
		// the top down, and a child's answer - "I consumed the pointer" - is what the children below it
		// see. Consumption comes only from sprites that carry a mouse handler (m_mouse_enabled) and from
		// text fields; a bare shape, a mask or a decorative clip never claims the pointer, which is how
		// a click on a Textbox's bevel still reaches its edit text and a click beside a closed dropdown
		// still reaches the button under its list mask. Slot 61, point_test_world, is the geometric
		// half: does this character cover the world-space point (twips). The character defaults are
		// the shipped 0x1007EEF0 / 0x1007EF00, both false.
		virtual bool	mouse_update(bool consumed) { return false; }
		virtual bool	point_test_world(float x, float y) { return false; }
		// The root's mouse capture (the shipped movie_root's +0xB4 / +0xB8): the character that took a
		// press holds the pointer until it releases. Sprites forward to their movie_root.
		virtual movie*	get_active_mouse_entity() { return NULL; }
		virtual void	set_active_mouse_entity(movie* m) {}
		virtual bool	get_track_as_menu() const { return false; }


		//
		// ActionScript.
		//


		virtual movie*	get_relative_target(const tu_string& name)
		{
			assert(0);	
			return NULL;
		}

		// ActionScript event handler.  Returns true if a handler was called.
		virtual bool	on_event(event_id id) { return false; }

		int    add_interval_timer(void *timer)
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
			return -1;	// ???
		}
		
		void    clear_interval_timer(int x)
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
		}
		
		virtual void    do_something(void *timer)
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
		}
		
		// Special event handler; sprites also execute their frame1 actions on this event.
		virtual void	on_event_load() { on_event(event_id::LOAD); }

#if 0
		// tulrich: @@ is there a good reason these are in the
		// vtable?  I.e. can the caller just call
		// on_event(event_id::SOCK_DATA) instead of
		// on_event_xmlsocket_ondata()?
		virtual void	on_event_xmlsocket_ondata() { on_event(event_id::SOCK_DATA); }
		virtual void	on_event_xmlsocket_onxml() { on_event(event_id::SOCK_XML); }
		virtual void	on_event_interval_timer() { on_event(event_id::TIMER); }
		virtual void	on_event_load_progress() { on_event(event_id::LOAD_PROGRESS); }
#endif
		// as_object_interface stuff
		virtual void	set_member(const tu_stringi& name, const as_value& val) { assert(0); }
		virtual bool	get_member(const tu_stringi& name, as_value* val) { assert(0); return false; }

		virtual void	call_frame_actions(const as_value& frame_spec) { assert(0); }

		virtual float	get_timer() const { return 0.0f; }
		virtual movie*	to_movie() { return this; }

		virtual void	clone_display_object(const tu_string& name, const tu_string& newname, Uint16 depth) { assert(0); }
		virtual void	remove_display_object(const tu_string& name) { assert(0); }

		// Forward vararg call to version taking va_list.
		virtual const char*	call_method(const char* method_name, const char* method_arg_fmt, ...)
		{
			va_list	args;
			va_start(args, method_arg_fmt);
			const char*	result = call_method_args(method_name, method_arg_fmt, args);
			va_end(args);

			return result;
		}

		virtual const char*	call_method_args(const char* method_name, const char* method_arg_fmt, va_list args)
		// Override this if you implement call_method.
		{
			assert(0);
			return NULL;
		}

		virtual void	execute_frame_tags(int frame, bool state_only = false) {}

		// External.
		virtual void	attach_display_callback(const char* path_to_object, void (*callback)(void*), void* user_ptr)
		{
			assert(0);
		}

		virtual void	set_display_callback(void (*callback)(void*), void* user_ptr)
		// Override me to provide this functionality.
		{
		}
	};


	// Information about how to display a character.
	struct display_info
	{
		movie*	m_parent;
		int	m_depth;
		cxform	m_color_transform;
		matrix	m_matrix;
		float	m_ratio;
                Uint16 	m_clip_depth;

		display_info()
			:
			m_parent(NULL),
			m_depth(0),
			m_ratio(0.0f),
                        m_clip_depth(0)
		{
		}

		void	concatenate(const display_info& di)
		// Concatenate the transforms from di into our
		// transforms.
		{
			m_depth = di.m_depth;
			m_color_transform.concatenate(di.m_color_transform);
			m_matrix.concatenate(di.m_matrix);
			m_ratio = di.m_ratio;
                        m_clip_depth = di.m_clip_depth;
		}
	};


	// character is a live, stateful instance of a character_def.
	// It represents a single active element in a movie.
	struct character : public movie
	{
		int		m_id;
		movie*		m_parent;
		tu_string	m_name;
		int		m_depth;
		cxform		m_color_transform;
		matrix		m_matrix;
		float		m_ratio;
                Uint16		m_clip_depth;
		bool		m_visible;
		hash<event_id, as_value>	m_event_handlers;
		void		(*m_display_callback)(void*);
		void*		m_display_callback_user_ptr;

		// [DICE-MOD SwiffPlayer: MovieClip.setMask. The shipped character keeps the mask clip at
		// +192 and a "mask only" byte at +196 (gameswf_sprite_set_mask); display_list::display
		// draws a masked character as begin_submit_mask / mask / end_submit_mask / character /
		// disable_mask, and never draws a mask-only character on its own.]
		smart_ptr<character>	m_mask;
		bool		m_is_mask_only;
		// [DICE-MOD SwiffPlayer #51] the shipped +197: set once a mouse handler is attached (a clip
		// event through set_event_handler, an on... member through the sprite's set_member), read by
		// the sprite mouse pass, never cleared (gameswf_character_ctor zeroes it).
		bool		m_mouse_enabled;

		// [DICE-MOD SwiffPlayer] a runtime-built character keeps its own definition alive (see
		// generic_character::m_def_owner); a sprite already owns its definition through m_def.
		virtual void	hold_definition(character_def* /*def*/) {}

		// Movie teardown (release_script_objects): appends every object or function this character
		// references from script state (event handlers, the mask; overrides add their environment
		// and children) to *held, then clears its own references, so clips of a released movie no
		// longer keep each other alive. Runs no ActionScript.
		// [CHOSEN, not recovered] issue #7 (https://github.com/B2BF/OpenBF2/issues/7).
		virtual void	release_script_values(array<as_value>* held)
		{
			for (hash<event_id, as_value>::iterator it = m_event_handlers.begin(); it != m_event_handlers.end(); ++it)
			{
				hold_script_value(held, it->second);
			}
			m_event_handlers.clear();
			if (m_mask != NULL)
			{
				held->push_back(as_value(m_mask.get_ptr()));
				m_mask = NULL;
			}
		}

		character(movie* parent, int id)
			:
			m_id(id),
			m_parent(parent),
			m_depth(-1),
			m_ratio(0.0f),
			m_clip_depth(0),
			m_visible(true),
			m_display_callback(NULL),
			m_display_callback_user_ptr(NULL),
			m_is_mask_only(false),
			m_mouse_enabled(false)
		{
			assert((parent == NULL && m_id == -1)
			       || (parent != NULL && m_id >= 0));
		}

		// Accessors for basic display info.
		virtual character*	cast_to_character() { return this; }	// [DICE-MOD SwiffPlayer: see movie]
		int	get_id() const { return m_id; }
		movie*	get_parent() const { return m_parent; }
		void set_parent(movie* parent) { m_parent = parent; }  // for extern movie
		int	get_depth() const { return m_depth; }
		void	set_depth(int d) { m_depth = d; }
		const matrix&	get_matrix() const { return m_matrix; }
		void	set_matrix(const matrix& m)
		{
			assert(m.is_valid());
			m_matrix = m;
		}
		const cxform&	get_cxform() const { return m_color_transform; }
		void	set_cxform(const cxform& cx) { m_color_transform = cx; }
		void	concatenate_cxform(const cxform& cx) { m_color_transform.concatenate(cx); }
		void	concatenate_matrix(const matrix& m) { m_matrix.concatenate(m); }
		float	get_ratio() const { return m_ratio; }
		void	set_ratio(float f) { m_ratio = f; }
		Uint16	get_clip_depth() const { return m_clip_depth; }
		void	set_clip_depth(Uint16 d) { m_clip_depth = d; }

		void	set_name(const char* name) { m_name = name; }
		const tu_string&	get_name() const { return m_name; }

		// For edit_text support (Flash 5).  More correct way
		// is to do "text_character.text = whatever", via
		// set_member().
		virtual const char*	get_text_name() const { return ""; }
		virtual void	set_text_value(const char* new_text) { assert(0); }

		virtual matrix	get_world_matrix() const
		// Get our concatenated matrix (all our ancestor transforms, times our matrix).  Maps
		// from our local space into "world" space (i.e. root movie space).
		{
			matrix	m;
			if (m_parent)
			{
				m = m_parent->get_world_matrix();
			}
			m.concatenate(get_matrix());

			return m;
		}

		virtual cxform	get_world_cxform() const
		// Get our concatenated color transform (all our ancestor transforms,
		// times our cxform).  Maps from our local space into normal color space.
		{
			cxform	m;
			if (m_parent)
			{
				m = m_parent->get_world_cxform();
			}
			m.concatenate(get_cxform());

			return m;
		}

		// Event handler accessors.
		bool	get_event_handler(event_id id, as_value* result)
		{
			return m_event_handlers.get(id, result);
		}
		void	set_event_handler(event_id id, const as_value& method)
		{
			// [DICE-MOD SwiffPlayer #51] sub_10077E10: a mouse handler marks the character mouse-enabled
			// before it is stored - the eleven ids the shipped body tests.
			switch (id.m_id)
			{
			case event_id::PRESS:
			case event_id::RELEASE:
			case event_id::RELEASE_OUTSIDE:
			case event_id::ROLL_OVER:
			case event_id::ROLL_OUT:
			case event_id::DRAG_OVER:
			case event_id::DRAG_OUT:
			case event_id::MOUSE_DOWN:
			case event_id::MOUSE_UP:
			case event_id::MOUSE_MOVE:
			case event_id::DOUBLE_PRESS:
				m_mouse_enabled = true;
				break;
			default:
				break;
			}
			m_event_handlers.set(id, method);
		}

		// Movie interfaces.  By default do nothing.  sprite_instance and some others override these.
		virtual void	display() {}
		virtual float	get_height() { return 0; }
		virtual float	get_width() { return 0; }

		// [DICE-MOD SwiffPlayer #39] the character's UNSCALED local extent (twips, its own space).
		// Flash's `_width`/`_height` are this times the own matrix scale, and their setters divide by
		// it - absolute, independent of ancestors. See GAMESWF_DIVERGENCES.md #39.
		virtual float	get_local_height() { return 0; }
		virtual float	get_local_width() { return 0; }

		// [DICE-MOD SwiffPlayer #42] Flash's dead reference: a clip removed from its parent's display
		// list (RemoveObject2, removeMovieClip, the parent's own destruction) answers undefined for
		// every member from then on, so a script callback that still holds it finds nothing to call
		// and the movie's UpdateManager - which keeps a callback only while it returns true - drops
		// it after one tick. Stock 2006 left the object fully alive: the manage-accounts page's
		// callback kept running against a removed clip 524,100 times in one retail drive.
		virtual void	mark_dead() {}

		virtual movie*	get_root_movie() { return m_parent->get_root_movie(); }
		virtual int	get_current_frame() const { assert(0); return 0; }
		virtual bool	has_looped() const { assert(0); return false; }
		virtual void	restart() { /*assert(0);*/ }
		virtual void	advance(float delta_time) {}	// for buttons and sprites
		virtual void	goto_frame(int target_frame) {}
		virtual bool	get_accept_anim_moves() const { return true; }

		virtual void	get_drag_state(drag_state* st) { assert(m_parent); m_parent->get_drag_state(st); }

		// [DICE-MOD SwiffPlayer #54] sub_1007F030 (character vtable +88, read in full 2026-09-28) -
		// hiding a character RELEASES THE ROOT'S MOUSE CAPTURE when the capture holder is this
		// character or lies anywhere beneath it. The holder took the capture on its press (#51) and
		// drops it only in its own release transition, but a hidden character's mouse pass never
		// runs again - the parent's child loop skips invisible children (0x10080010 tests the +0xBA
		// byte get_visible reads, as #51 does) - so without this a clip that hides what it was
		// pressed in keeps every other character's state machine gated off for good. mainMenu.swf's
		// SelectBox does exactly that: a ColumnList row's onPress selects the row, the box's setValue
		// closes it (m_mcContainer._visible = false) with the pressed row inside, and its click
		// catcher's onPress hides the catcher itself. Owner's report 2026-09-28: after picking an
		// antialiasing level on OPTIONS > VIDEO nothing was clickable, APPLY included.
		//
		//   m_visible = visible; showing stops there
		//   active = the root's get_active_mouse_entity()                  (the root sprite's +0xB4)
		//   walk from active up through the parents, each of which must be a character; reaching
		//   this -> set_active_mouse_entity(NULL)                           (the root sprite's +0xB8)
		//
		// [CHOSEN, not recovered] The shipped asks the GLOBAL root sprite (dword_100F8CAC, the "_root"
		// movie_def_impl::create_instance 0x10084A30 stores, released at 0x100753AD); this port keeps
		// no such global for the main movie (s_current_root is only set for an external movie), so it
		// asks the nearest sprite of this character's own tree, which forwards to the same movie_root
		// - one object while one movie is up. It does not use get_root_movie, whose character body
		// dereferences the parent unguarded. [INFERRED] the shipped per-parent test (slot 0 of the +8
		// subobject's table, `mov al, 1` for sprites and buttons alike) is "is a character":
		// cast_to_character here. The BUTTON's `_visible` setter writes the byte directly in both
		// players (sub_100A34E0, gameswf_button.cpp), so hiding a button releases nothing.
		virtual void	set_visible(bool visible)
		{
			m_visible = visible;
			if (visible)
			{
				return;
			}
			release_mouse_capture_within();
		}

		// The capture walk set_visible ends with (sub_1007F030), also inlined by the display list's
		// remove_display_object (0x100A7A20: 0x100A7A67..0x100A7AAA) and replace_display_object
		// (0x100A8080: 0x100A80B3..0x100A80ED) for the character leaving the list: when the root's
		// active entity is this character or lies beneath it, the capture is dropped. Both list
		// bodies fetch the root through sub_10071710 (the global root, see the [CHOSEN] note above).
		void	release_mouse_capture_within()
		{
			movie*	owner = this;	// the nearest sprite: only a sprite answers get_root()
			while (owner != NULL && owner->get_root() == NULL)
			{
				character*	c = owner->cast_to_character();
				owner = c != NULL ? c->get_parent() : NULL;
			}
			if (owner == NULL)
			{
				return;
			}
			movie*	active = owner->get_active_mouse_entity();
			if (active == NULL)
			{
				return;
			}
			for (movie* m = active; m != this; )
			{
				character*	c = m->cast_to_character();
				movie*	parent = c != NULL ? c->get_parent() : NULL;
				if (parent == NULL || parent->cast_to_character() == NULL)
				{
					return;
				}
				m = parent;
			}
			owner->set_active_mouse_entity(NULL);
		}
		virtual bool	get_visible() const { return m_visible; }

		virtual void	set_display_callback(void (*callback)(void*), void* user_ptr)
		{
			m_display_callback = callback;
			m_display_callback_user_ptr = user_ptr;
		}

		virtual void	do_display_callback()
		{
			if (m_display_callback)
			{
				(*m_display_callback)(m_display_callback_user_ptr);
			}
		}

		virtual void	get_mouse_state(int* x, int* y, int* buttons) { get_parent()->get_mouse_state(x, y, buttons); }

		// Utility.
		void	do_mouse_drag();
	};


	// For characters that don't store unusual state in their instances.
	struct generic_character : public character
	{
		character_def*	m_def;
		// [DICE-MOD SwiffPlayer] set only for runtime-built definitions (drawing canvas, loadMovie
		// image shape) - see sprite_instance::add_display_object_def. Timeline characters keep the
		// stock raw pointer into the movie's character table.
		smart_ptr<character_def>	m_def_owner;

		generic_character(character_def* def, movie* parent, int id)
			:
			character(parent, id),
			m_def(def)
		{
			assert(m_def);
		}

		virtual void	hold_definition(character_def* def) { m_def_owner = def; }

		virtual void	display()
		{
			m_def->display(this);	// pass in transform info
			do_display_callback();
		}

		// @@ tulrich: these are used for finding bounds; TODO
		// need to do this using enclose_transformed_rect(),
		// not by scaling the local height/width!

		virtual float	get_height()
		{
			matrix	m = get_world_matrix();
			float	h = m_def->get_height_local() * m.m_[1][1];
			return h;
		}

		virtual float	get_width()
		{
			matrix	m = get_world_matrix();
			float	w = m_def->get_width_local() * m.m_[0][0];
			return w;
		}

		// [DICE-MOD SwiffPlayer #39] the definition's local extent, unscaled.
		virtual float	get_local_height() { return m_def->get_height_local(); }
		virtual float	get_local_width() { return m_def->get_width_local(); }

		// [DICE-MOD SwiffPlayer #51] sub_1007F1E0: the world-space point brought into the shape's own
		// space through the inverse of its world matrix, then the definition's local test.
		virtual bool	point_test_world(float x, float y)
		{
			matrix	m = get_world_matrix();
			point	p;
			m.transform_by_inverse(&p, point(x, y));
			return m_def->point_test_local(p.m_x, p.m_y);
		}
	};


	struct bitmap_character_def : public character_def
	{
		virtual gameswf::bitmap_info*	get_bitmap_info() = 0;

		// [DICE-MOD SwiffPlayer: safe downcast, same pattern as resource::cast_to_font. Lets the
		// exported-bitmap enumeration below tell a bitmap character from any other exported
		// character_def without a static_cast. See GAMESWF_DIVERGENCES.md.]
		virtual bitmap_character_def*	cast_to_bitmap_character_def() { return this; }
	};

#if 1
	// Bitmap character
	struct bitmap_character : public bitmap_character_def
	{
		bitmap_character(bitmap_info* bi)
			:
			m_bitmap_info(bi)
		{
		}

// 		bitmap_character(image::rgb* image)
// 		{
// 			assert(image != 0);

// 			// Create our bitmap info, from our image.
// 			m_bitmap_info = gameswf::render::create_bitmap_info_rgb(image);
// 		}

// 		bitmap_character(image::rgba* image)
// 		{
// 			assert(image != 0);

// 			// Create our bitmap info, from our image.
// 			m_bitmap_info = gameswf::render::create_bitmap_info_rgba(image);
// 		}

		gameswf::bitmap_info*	get_bitmap_info()
		{
			return m_bitmap_info.get_ptr();
		}

	private:
		smart_ptr<gameswf::bitmap_info>	m_bitmap_info;
	};

#endif

	// Execute tags include things that control the operation of
	// the movie.  Essentially, these are the events associated
	// with a frame.
	struct execute_tag
	{
		virtual ~execute_tag() {}
		virtual void	execute(movie* m) {}
		virtual void	execute_state(movie* m) {}
		virtual void	execute_state_reverse(movie* m, int frame) { execute_state(m); }
		virtual bool	is_remove_tag() const { return false; }
		virtual bool	is_action_tag() const { return false; }
		virtual uint32	get_depth_id_of_replace_or_add_tag() const { return static_cast<uint32>(-1); }

		// [DICE-MOD SwiffPlayer #45] The shipped execute_tag's third execution slot (vtable +12,
		// base 0x1006EFB0): what a tag does when a goto replays the TARGET frame's playlist after
		// the timeline-state diff has already placed, moved and removed everything. The base runs
		// execute(), so a do_action (which leaves the slot at the base) queues its actions, while
		// place_object_2 (0x10075760) and remove_object_2 (0x1006F8E0) override it with nothing.
		// The other three are the reads the shipped state builder (sub_100832F0) makes once it has
		// identified a tag: is_place_tag (vtable +24), then the place tag's depth (+70) and kind
		// (+76, place_object_2::place_type), or the remove tag's depth (+4).
		virtual void	execute_goto(movie* m) { execute(m); }
		virtual bool	is_place_tag() const { return false; }
		virtual int	get_tag_depth() const { return -1; }
		virtual int	get_place_type() const { return -1; }
	};


	//
	// Loader callbacks.
	//
	
	// Register a loader function for a certain tag type.  Most
	// standard tags are handled within gameswf.  Host apps might want
	// to call this in order to handle special tag types.
	typedef void (*loader_function)(stream* input, int tag_type, movie_definition_sub* m);
	void	register_tag_loader(int tag_type, loader_function lf);
	
	
	// Tag loader functions.
	void	null_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	set_background_color_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	jpeg_tables_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_jpeg_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_jpeg2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_jpeg3_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_shape_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_shape_morph_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_font_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_font_info_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_text_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_edit_text_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	place_object_2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_lossless_2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	sprite_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	end_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	remove_object_2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	do_action_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	button_character_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	frame_label_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	export_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	import_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_sound_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	start_sound_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	button_sound_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	do_init_action_loader(stream* in, int tag_type, movie_definition_sub* m);
	// sound_stream_loader();	// head, head2, block


}	// end namespace gameswf


#endif // GAMESWF_IMPL_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
