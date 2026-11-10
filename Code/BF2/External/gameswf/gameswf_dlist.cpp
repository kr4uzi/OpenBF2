// gameswf_dlist.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// A list of active characters.



#include "gameswf_dlist.h"
#include "gameswf_log.h"
#include "gameswf_render.h"
#include "gameswf.h"
#include "gameswf_prefix_trace.h"
#include <typeinfo>



namespace gameswf
{
	// SwiffPlayer.dll 0x100A7800: signed depth comparison yields -1, 0 or 1.
	/*static*/ int	display_object_info::compare(const void* _a, const void* _b)
	{
		display_object_info*	a = (display_object_info*) _a;
		display_object_info*	b = (display_object_info*) _b;

		if (a->m_character->get_depth() < b->m_character->get_depth())
		{
			return -1;
		}
		else if (a->m_character->get_depth() == b->m_character->get_depth())
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}

	
	int	display_list::find_display_index(int depth)
	// Find the index in the display list of the first object
	// matching the given depth.  Failing that, return the index
	// of the first object with a larger depth.
	{
		// SwiffPlayer.dll 0x100A7820. On an
		// equal depth retail keeps the binary-search hit if referenced, otherwise
		// searches the equal-depth run for a referenced entry before using the hit.
		const int size = m_display_object_array.size();
		if (size == 0) return 0;
		int jump = size >> 1;
		int index = jump;
		for (;;)
		{
			jump >>= 1;
			if (jump < 1) jump = 1;
			const int current = m_display_object_array[index].m_character->get_depth();
			if (depth > current)
			{
				if (index == size - 1) return size;
				index += jump;
			}
			else if (depth < current)
			{
				if (index == 0 || m_display_object_array[index - 1].m_character->get_depth() < depth) return index;
				index -= jump;
			}
			else
			{
				if (m_display_object_array[index].m_ref) return index;
				int candidate = index;
				while (candidate != 0 && m_display_object_array[candidate].m_character->get_depth() == depth)
				{
					if (m_display_object_array[candidate].m_ref) return candidate;
					candidate--;
				}
				if (m_display_object_array[candidate].m_character->get_depth() != depth) candidate++;
				for (; candidate < size; candidate++)
				{
					if (m_display_object_array[candidate].m_character->get_depth() != depth) return index;
					if (m_display_object_array[candidate].m_ref) return candidate;
				}
				return index;
			}
		}
	}
	

	// SwiffPlayer.dll 0x100A78F0.
	int	display_list::get_display_index(int depth)
		// Like the above, but looks for an exact match, and returns -1 if failed.
	{
		int	index = find_display_index(depth);
		if (index >= m_display_object_array.size()
			|| get_display_object(index).m_character->get_depth() != depth)
		{
			// No object at that depth.
			return -1;
		}
		return index;
	}


	// SwiffPlayer.dll 0x100A7920.
	character*	display_list::get_character_at_depth(int depth)
	{
		int	index = get_display_index(depth);
		if (index != -1)
		{
			character*	ch = m_display_object_array[index].m_character.get_ptr();
			if (ch->get_depth() == depth)
			{
				return ch;
			}
		}

		return NULL;
	}


	// SwiffPlayer.dll 0x100A7D10 visits referenced entries in display-list order.
	character*	display_list::get_character_by_name(const tu_string& name)
	{
		// See if we have a match on the display list.
		for (int i = 0, n = get_character_count(); i < n; i++)
		{
			character*	ch = get_character(i);
			if (ch && ch->get_name() == name.c_str())
			{
				// Found it.
				return ch;
			}
		}
		return NULL;
	}

	character*	display_list::get_character_by_name_i(const tu_stringi& name)
	// Return first character with matching (case insensitive) name, if any.
	{
		// Search through dlist for a match.
		for (int i = 0, n = get_character_count(); i < n; i++)
		{
			character*	ch = get_character(i);
			if (ch && name == ch->get_name().c_str())
			{
				// Found it.
				return ch;
			}
		}

		return NULL;
	}


	// SwiffPlayer.dll 0x100A8080.
	void	display_list::add_display_object(
		character* ch, 
		Uint16 depth,
		bool replace_if_depth_is_occupied,
		const cxform& color_xform, 
		const matrix& mat, 
		float ratio,
		Uint16 clip_depth)
	{
//		IF_VERBOSE_DEBUG(log_msg("dl::add(%d, '%s')\n", depth, ch->get_name()));//xxxxx

		assert(ch);
		
		// [CHOSEN, not recovered] S3 bridge: retain the 2006 argument at
		// callers; retail always unreferences an equal-depth occupant.
		(void) replace_if_depth_is_occupied;
		int index = find_display_index(depth);
		if (index >= 0 && index < m_display_object_array.size())
		{
			display_object_info& old = m_display_object_array[index];
			if (old.m_character->get_depth() == depth)
			{
				old.m_ref = false;
				old.m_character->release_mouse_capture_within();
				// SwiffPlayer.dll 0x100A80F6: a surviving old occupant is detached.
				old.m_character->set_parent(NULL);
				old.m_character->mark_dead();
			}
		}

		ch->set_depth(depth);

		display_object_info	di;
		di.m_ref = true;
		di.set_character(ch);
		di.m_character->set_depth(depth);
		di.m_character->set_cxform(color_xform);
		di.m_character->set_matrix(mat);
		di.m_character->set_ratio(ratio);
		di.m_character->set_clip_depth(clip_depth);

		// Retail searches again after setting the new character's properties.
		index = find_display_index(depth);
		m_display_object_array.insert(index, di);

		// do the frame1 actions (if applicable) and the "onClipEvent (load)" event.
		ch->on_event_load();
	}
	
	
	// SwiffPlayer.dll 0x100A7960: clip_depth is not changed by a move.
	void	display_list::move_display_object(
		Uint16 depth,
		bool use_cxform,
		const cxform& color_xform,
		bool use_matrix,
		const matrix& mat,
		float ratio,
		Uint16 clip_depth)
	// Updates the transform properties of the object at
	// the specified depth.
	{
//		IF_VERBOSE_DEBUG(log_msg("dl::move(%d)\n", depth));//xxxxx

		int	size = m_display_object_array.size();
		if (size <= 0)
		{
			// error.
			log_error("error: move_display_object() -- no objects on display list\n");
			//			assert(0);
			return;
		}
		
		int	index = find_display_index(depth);
		if (index < 0 || index >= size)
		{
			// error.
			log_error("error: move_display_object() -- can't find object at depth %d\n", depth);
			//			assert(0);
			return;
		}
		
		display_object_info&	di = m_display_object_array[index];
		character*	ch = di.m_character.get_ptr();
		if (ch->get_depth() != depth)
		{
			// error
			log_error("error: move_display_object() -- no object at depth %d\n", depth);
			//			assert(0);
			return;
		}

		di.m_ref = true;

		if (ch->get_accept_anim_moves() == false)
		{
			// This character is rejecting anim moves.  This happens after it
			// has been manipulated by ActionScript.
			return;
		}

		if (use_cxform)
		{
			ch->set_cxform(color_xform);
		}
		if (use_matrix)
		{
			ch->set_matrix(mat);
		}
		ch->set_ratio(ratio);
		// move_display_object apparently does not change clip depth!  Thanks to Alexeev Vitaly.
		// ch->set_clip_depth(clip_depth);
	}
	
	
	// SwiffPlayer.dll 0x100A81B0.
	void	display_list::replace_display_object(
		character* ch,
		Uint16 depth,
		bool use_cxform,
		const cxform& color_xform,
		bool use_matrix,
		const matrix& mat,
		float ratio,
		Uint16 clip_depth)
	// Puts a new character at the specified depth, replacing any
	// existing character.  If use_cxform or use_matrix are false,
	// then keep those respective properties from the existing
	// character.
	{
//		IF_VERBOSE_DEBUG(log_msg("dl::replace(%d)\n", depth));//xxxxx

		int	size = m_display_object_array.size();
		int	index = find_display_index(depth);
		if (index < 0 || index >= size)
		{
			// Error, no existing object found at depth.
//			IF_VERBOSE_DEBUG(log_msg("dl::replace_display_object() no obj at depth %d\n", depth));
			// Fallback -- add the object.
			add_display_object(ch, depth, true, color_xform, mat, ratio, clip_depth);
			return;
		}
		
		display_object_info&	di = m_display_object_array[index];
		if (di.m_character->get_depth() != depth)
		{
			// error
//			IF_VERBOSE_DEBUG(log_msg("warning: replace_display_object() -- no object at depth %d\n", depth));
			return;
		}
		
		smart_ptr<character>	old_ch = di.m_character;

		// Put the new character in its place.
		assert(ch);
		ch->set_depth(depth);
		ch->restart();
		
		// Set the display properties.
		di.m_ref = true;
		di.set_character(ch);

		if (use_cxform)
		{
			ch->set_cxform(color_xform);
		}
		else
		{
			// Use the cxform from the old character.
			ch->set_cxform(old_ch->get_cxform());
		}

		if (use_matrix)
		{
			ch->set_matrix(mat);
		}
		else
		{
			// Use the matrix from the old character.
			ch->set_matrix(old_ch->get_matrix());
		}

		ch->set_ratio(ratio);
		ch->set_clip_depth(clip_depth);
	}
	
	
	//  raw entries below: the character may already be unreferenced.
	void	display_list::remove_display_object(Uint16 depth)
	// Removes the object at the specified depth.
	{
//		IF_VERBOSE_DEBUG(log_msg("dl::remove(%d)\n", depth));//xxxxx

		int	size = m_display_object_array.size();
		if (size <= 0)
		{
			// error.
			log_error("remove_display_object: no characters in display list\n");
			return;
		}
		
		int	index = find_display_index(depth);
		if (index < 0
		    || index >= size
		    || m_display_object_array[index].m_character.get_ptr()->get_depth() != depth)
		{
			// error -- no character at the given depth.
			log_error("remove_display_object: no character at depth %d\n", depth);
			return;
		}

		assert(m_display_object_array[index].m_character.get_ptr()->get_depth() == depth);
		
		// SwiffPlayer.dll 0x100A7A20 takes only depth.

		// Removing the character at get_display_object(index).
		display_object_info&	di = m_display_object_array[index];
		
		// Remove reference only.
		di.m_ref = false;
		//  0x100A7A20 (0x100A7A67..0x100A7AAA): a removed character that
		// holds the root's mouse capture, or contains its holder, releases it. The holder took it on
		// its press (#51) and gives it back only in its own release transition, which a character
		// off the list never runs again; every other state machine stays gated off. mainMenu.swf's
		// CREATE ACCOUNT button: its onPress runs createOffline, which on success jumps the root
		// from the account page (frame 19) to the main menu (49) and removes the pressed button, so
		// after an SP account creation no click reached the menu.
		if (di.m_character.get_ptr()) di.m_character->release_mouse_capture_within();
		// SwiffPlayer.dll 0x100A7AB5: detached script references must not retain
		// a borrowed parent that can be destroyed before the next timer callback.
		di.m_character->set_parent(NULL);
		if (di.m_character.get_ptr()) di.m_character->mark_dead();	//  dead from the removal on
	}
	
	
	// SwiffPlayer.dll 0x100A7F10 snapshots the count, sends UNLOAD to every
	// stored character (including unreferenced entries), then clears storage.
	void	display_list::clear()
	// clear the display list.
	{
#if !defined(_MSC_VER)
		if (g_swf_mouse_pass_depth > 0)
		{
			SWF_LIST_TRACE("clear list %p (size %d, sprite destroyed)", (void*) this, m_display_object_array.size());
		}
#endif
		int i, n = m_display_object_array.size();
		for (i = 0; i < n; i++)
		{
			display_object_info&	di = m_display_object_array[i];
			//character*	ch = m_display_object_array[i].m_character;
			di.m_character->on_event(event_id::UNLOAD);
		}
		
		m_display_object_array.clear();
	}


	void	display_list::release_characters(array<as_value>* held)
	// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. Unlike clear(),
	// no UNLOAD: the movie is being destroyed with its scripts already disconnected.
	{
		for (int i = 0, n = m_display_object_array.size(); i < n; i++)
		{
			character*	ch = m_display_object_array[i].m_character.get_ptr();
			if (ch != NULL)
			{
				held->push_back(as_value(ch));
			}
		}
		m_display_object_array.clear();
	}


	// SwiffPlayer.dll 0x100A7AE0.
	void	display_list::reset()
	// reset the references to the display list.
	{
		//printf("### reset the display list!\n");
		int i, n = m_display_object_array.size();
		for (i = 0; i < n; i++)
		{
			m_display_object_array[i].m_ref = false;
		}
	}
	
	
	// SwiffPlayer.dll 0x100A7F50: backwards sweep, release the character before
	// removing its entry; the final removal frees the array. No list-storage hold.
	void	display_list::update()
	// remove unreferenced objects.
	{
		//printf("### update the display list!\n");
		
		int r = 0;
		int i, n = m_display_object_array.size();
		for (i = n-1; i >= 0; i--)
		{
			display_object_info & dobj = m_display_object_array[i];
			
			if (dobj.m_ref == false)
			{
				dobj.set_character(NULL);

				m_display_object_array.remove(i);
				r++;
			}
		}
#if !defined(_MSC_VER)
		// [TEMP] c-swf-disconnect: a sweep inside a mouse pass (the owner is named by the goto line).
		if (r > 0 && g_swf_mouse_pass_depth > 0)
		{
			SWF_LIST_TRACE("sweep list %p removed %d, %d left%s", (void*) this, r, m_display_object_array.size(),
				m_display_object_array.size() == 0 ? " - EMPTY (buffer freed)" : "");
		}
#endif
		
		//printf("### removed characters: %4d active characters: %4d\n", r, m_display_object_array.size());
	}
	
	
	void	display_list::advance(float delta_time)
	// advance referenced characters.
	{
		// SwiffPlayer.dll 0x100A82A0 snapshots pointers to referenced entries,
		// then rechecks their reference flags before dispatch. It holds no characters.
		array<display_object_info*> entries;
		const int n = m_display_object_array.size();
		for (int i = 0; i < n; i++)
		{
			if (m_display_object_array[i].m_ref == true)
			{
				entries.push_back(&m_display_object_array[i]);
			}
		}
		for (int i = 0, count = entries.size(); i < count; i++)
		{
			if (entries[i]->m_ref)
			{
				entries[i]->m_character->advance(delta_time);
			}
		}
	}
	
	
	// SwiffPlayer.dll 0x100A7B00: visibility and mask-only gates precede masking.
	void	display_list::display()
	// Display the referenced characters. Lower depths
	// are obscured by higher depths.
	{
		bool masked = false;
		int highest_masked_layer = 0;
		
		//log_msg("number of objects to be drawn %i\n", m_display_object_array.size());
		
		for (int i = 0; i < m_display_object_array.size(); i++)
		{
			display_object_info&	dobj = m_display_object_array[i];

			character*	ch = dobj.m_character.get_ptr();
			assert(ch);
			if (prefix_trace_display(ch)) prefix_trace_geometry(ch, "display-list");

			if (ch->get_visible() == false)
			{
				// Don't display.
				continue;
			}

			// a setMask() mask is drawn only inside its target's mask
			// sandwich (the shipped display_list::display, sub_100A7B00, skips characters whose
			// +196 byte is set).
			if (ch->m_is_mask_only)
			{
				continue;
			}

			if (ch->get_clip_depth() > 0)
			{
//				log_msg("depth %i, clip_depth %i\n", dobj.m_depth, dobj.m_clip_depth);
			}

			// check whether a previous mask should be disabled
			if (masked)
			{
				if (ch->get_depth() > highest_masked_layer)
				{
//					log_msg("disabled mask before drawing depth %i\n", ch->get_depth());
					masked = false;
					// turn off mask
					render::disable_mask();
				}
			}

			// check whether this object should become mask
			if (ch->get_clip_depth() > 0)
			{
				//log_msg("begin submit mask\n");
				render::begin_submit_mask();
			}
			
			if (ch->m_mask != NULL)
			{
				// the setMask() sandwich, exactly as sub_100A7B00 does it.
				render::begin_submit_mask();
				ch->m_mask->display();
				render::end_submit_mask();
				ch->display();
				render::disable_mask();
			}
			else
			{
				ch->display();
			}

			if (ch->get_clip_depth() > 0)
			{
//				log_msg("object drawn\n");
			}
			
			// if this object should have become a mask,
			// inform the renderer that it now has all
			// information about it
			if (ch->get_clip_depth() > 0)
			{
				//log_msg("end submit mask\n");
				render::end_submit_mask();
				highest_masked_layer = ch->get_clip_depth();
				masked = true;
			}
		}
		
		if (masked)
		{
			// If a mask masks the scene all the way up to the highest
			// layer, it will not be disabled at the end of drawing
			// the display list, so disable it manually.
			render::disable_mask();
		}
	}
}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
