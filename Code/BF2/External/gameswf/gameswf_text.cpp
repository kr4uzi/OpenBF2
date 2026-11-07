// gameswf_text.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Code for the text tags.


#include "base/utf8.h"
#if defined(__APPLE__)
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered] bf2x87::lowUInt32
#endif
#include "base/utility.h"
#include "gameswf_impl.h"
#include "gameswf_shape.h"
#include "gameswf_stream.h"
#include "gameswf_log.h"
#include "gameswf_font.h"
#include "gameswf_fontlib.h"
#include "gameswf_render.h"
#include "gameswf_textformat.h"

namespace gameswf
{
	//
	// text_character
	//


	// Helper struct.
	struct text_style
	{
		int	m_font_id;
		mutable font*	m_font;
		rgba	m_color;
		float	m_x_offset;
		float	m_y_offset;
		float	m_text_height;
		bool	m_has_x_offset;
		bool	m_has_y_offset;

		text_style()
			:
			m_font_id(-1),
			m_font(NULL),
			m_x_offset(0),
			m_y_offset(0),
			m_text_height(1.0f),
			m_has_x_offset(false),
			m_has_y_offset(false)
		{
		}

		void	resolve_font(movie_definition_sub* root_def) const
		{
			if (m_font == NULL)
			{
				assert(m_font_id >= 0);

				m_font = root_def->get_font(m_font_id);
				if (m_font == NULL)
				{
					log_error("error: text style with undefined font; font_id = %d\n", m_font_id);
				}
			}
		}
	};


	// Helper struct.
	struct text_glyph_record
	{
		struct glyph_entry
		{
			int	m_glyph_index;
			float	m_glyph_advance;
		};
		text_style	m_style;
		array<glyph_entry>	m_glyphs;

		void	read(stream* in, int glyph_count, int glyph_bits, int advance_bits)
		{
			m_glyphs.resize(glyph_count);
			for (int i = 0; i < glyph_count; i++)
			{
				m_glyphs[i].m_glyph_index = in->read_uint(glyph_bits);
				m_glyphs[i].m_glyph_advance = (float) in->read_sint(advance_bits);
			}
		}
	};


	// Render the given glyph records.
	static void	display_glyph_records(
		const matrix& this_mat,
		character* inst,
		const array<text_glyph_record>& records,
		movie_definition_sub* root_def)
	{
		static array<fill_style>	s_dummy_style;	// used to pass a color on to shape_character::display()
		static array<line_style>	s_dummy_line_style;
		s_dummy_style.resize(1);

		matrix	mat = inst->get_world_matrix();
		mat.concatenate(this_mat);

		cxform	cx = inst->get_world_cxform();
		float	pixel_scale = inst->get_pixel_scale();

//		display_info	sub_di = di;
//		sub_di.m_matrix.concatenate(mat);

//		matrix	base_matrix = sub_di.m_matrix;
		matrix	base_matrix = mat;
		float	base_matrix_max_scale = base_matrix.get_max_scale();

		float	scale = 1.0f;
		float	x = 0.0f;
		float	y = 0.0f;

		for (int i = 0; i < records.size(); i++)
		{
			// Draw the characters within the current record; i.e. consecutive
			// chars that share a particular style.
			const text_glyph_record&	rec = records[i];

			rec.m_style.resolve_font(root_def);

			font*	fnt = rec.m_style.m_font;
			if (fnt == NULL)
			{
				continue;
			}

			scale = rec.m_style.m_text_height / 1024.0f;	// the EM square is 1024 x 1024
			float	text_screen_height = base_matrix_max_scale
				* scale
				* 1024.0f
				/ 20.0f
				* pixel_scale;

			int	nominal_glyph_height = fnt->get_texture_glyph_nominal_size();
			float	max_glyph_height = fontlib::get_texture_glyph_max_height(fnt);
#ifdef GAMESWF_ALWAYS_USE_TEXTURES_FOR_TEXT_WHEN_POSSIBLE
			const bool	use_glyph_textures = true;
#else
			bool	use_glyph_textures =
				text_screen_height <= max_glyph_height * 1.0f;
#endif

			if (rec.m_style.m_has_x_offset)
			{
				x = rec.m_style.m_x_offset;
			}
			if (rec.m_style.m_has_y_offset)
			{
				y = rec.m_style.m_y_offset;
			}

			s_dummy_style[0].set_color(rec.m_style.m_color);

			rgba	transformed_color = cx.transform(rec.m_style.m_color);

			for (int j = 0; j < rec.m_glyphs.size(); j++)
			{
				int	index = rec.m_glyphs[j].m_glyph_index;
					
				mat = base_matrix;
				mat.concatenate_translation(x, y);
				mat.concatenate_scale(scale);

				if (index == -1)
				{
					// Invalid glyph; render it as an empty box.
					render::set_matrix(mat);
					render::line_style_color(transformed_color);

					// The EM square is 1024x1024, but usually isn't filled up.
					// We'll use about half the width, and around 3/4 the height.
					// Values adjusted by eye.
					// The Y baseline is at 0; negative Y is up.
					static const Sint16	s_empty_char_box[5 * 2] =
					{
						 32,   32,
						480,   32,
						480, -656,
						 32, -656,
						 32,   32
					};
					render::draw_line_strip(s_empty_char_box, 5);
				}
				else
				{
					const texture_glyph&	tg = fnt->get_texture_glyph(index);
					shape_character_def*	glyph = fnt->get_glyph(index);

					if (tg.is_renderable()
					    && (use_glyph_textures || glyph == NULL))
					{
						fontlib::draw_glyph(mat, tg, transformed_color, nominal_glyph_height);
					}
					else
					{

						// Draw the character using the filled outline.
						if (glyph)
						{
							glyph->display(mat, cx, pixel_scale, s_dummy_style, s_dummy_line_style);
						}
					}
				}
				x += rec.m_glyphs[j].m_glyph_advance;
			}
		}
	}


	struct text_character_def : public character_def
	{
		movie_definition_sub*	m_root_def;
		rect	m_rect;
		matrix	m_matrix;
		array<text_glyph_record>	m_text_glyph_records;

		text_character_def(movie_definition_sub* root_def)
			:
			m_root_def(root_def)
		{
			assert(m_root_def);
		}

		void	read(stream* in, int tag_type, movie_definition_sub* m)
		{
			assert(m != NULL);
			assert(tag_type == 11 || tag_type == 33);

			m_rect.read(in);
			m_matrix.read(in);

			int	glyph_bits = in->read_u8();
			int	advance_bits = in->read_u8();

			IF_VERBOSE_PARSE(log_msg("begin text records\n"));

			bool	last_record_was_style_change = false;

			text_style	style;
			for (;;)
			{
				int	first_byte = in->read_u8();
				
				if (first_byte == 0)
				{
					// This is the end of the text records.
					IF_VERBOSE_PARSE(log_msg("end text records\n"));
					break;
				}

				// Style changes and glyph records just alternate.
				// (Contrary to what most SWF references say!)
				if (last_record_was_style_change == false)
				{
					// This is a style change.

					last_record_was_style_change = true;

					bool	has_font = (first_byte >> 3) & 1;
					bool	has_color = (first_byte >> 2) & 1;
					bool	has_y_offset = (first_byte >> 1) & 1;
					bool	has_x_offset = (first_byte >> 0) & 1;

					IF_VERBOSE_PARSE(log_msg("  text style change\n"));

					if (has_font)
					{
						Uint16	font_id = in->read_u16();
						style.m_font_id = font_id;
						IF_VERBOSE_PARSE(log_msg("  has_font: font id = %d\n", font_id));
					}
					if (has_color)
					{
						if (tag_type == 11)
						{
							style.m_color.read_rgb(in);
						}
						else
						{
							assert(tag_type == 33);
							style.m_color.read_rgba(in);
						}
						IF_VERBOSE_PARSE(log_msg("  has_color\n"));
					}
					if (has_x_offset)
					{
						style.m_has_x_offset = true;
						style.m_x_offset = in->read_s16();
						IF_VERBOSE_PARSE(log_msg("  has_x_offset = %g\n", style.m_x_offset));
					}
					else
					{
						style.m_has_x_offset = false;
						style.m_x_offset = 0.0f;
					}
					if (has_y_offset)
					{
						style.m_has_y_offset = true;
						style.m_y_offset = in->read_s16();
						IF_VERBOSE_PARSE(log_msg("  has_y_offset = %g\n", style.m_y_offset));
					}
					else
					{
						style.m_has_y_offset = false;
						style.m_y_offset = 0.0f;
					}
					if (has_font)
					{
						style.m_text_height = in->read_u16();
						IF_VERBOSE_PARSE(log_msg("  text_height = %g\n", style.m_text_height));
					}
				}
				else
				{
					// Read the glyph record.

 					last_record_was_style_change = false;

					int	glyph_count = first_byte;

// 					if (! last_record_was_style_change)
// 					{
// 						glyph_count &= 0x7F;
// 					}
// 					// else { Don't mask the top bit; the first record is allowed to have > 127 glyphs. }

					m_text_glyph_records.resize(m_text_glyph_records.size() + 1);
					m_text_glyph_records.back().m_style = style;
					m_text_glyph_records.back().read(in, glyph_count, glyph_bits, advance_bits);

					IF_VERBOSE_PARSE(log_msg("  glyph_records: count = %d\n", glyph_count));
				}
			}
		}


		void	display(character* inst)
		// Draw the string.
		{
			display_glyph_records(m_matrix, inst, m_text_glyph_records, m_root_def);
		}
	};


	void	define_text_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Read a DefineText tag.
	{
		assert(tag_type == 11 || tag_type == 33);

		Uint16	character_id = in->read_u16();
		
		text_character_def*	ch = new text_character_def(m);
		IF_VERBOSE_PARSE(log_msg("text_character, id = %d\n", character_id));
		ch->read(in, tag_type, m);

		// IF_VERBOSE_PARSE(print some stuff);

		m->add_character(character_id, ch);
	}


	//
	// edit_text_character_def
	//


	struct edit_text_character_def : public character_def
	// A definition for a text display character, whose text can
	// be changed at runtime (by script or host).
	{
		movie_definition_sub*	m_root_def;
		rect			m_rect;
		tu_string		m_default_name;
		text_format		m_format;
		bool			m_word_wrap;
		bool			m_multiline;
		bool			m_password;	// show asterisks instead of actual characters
		bool			m_readonly;
		bool			m_auto_size;	// resize our bound to fit the text
		bool			m_no_select;
		bool			m_border;	// forces white background and black border -- silly, but sometimes used
		bool			m_html;

		// Allowed HTML (from Alexi's SWF Reference):
		//
		// <a href=url target=targ>...</a> -- hyperlink
		// <b>...</b> -- bold
		// <br> -- line break
		// <font face=name size=[+|-][0-9]+ color=#RRGGBB>...</font>  -- font change; size in TWIPS
		// <i>...</i> -- italic
		// <li>...</li> -- list item
		// <p>...</p> -- paragraph
		// <tab> -- insert tab
		// <TEXTFORMAT>  </TEXTFORMAT>
		//   [ BLOCKINDENT=[0-9]+ ]
		//   [ INDENT=[0-9]+ ]
		//   [ LEADING=[0-9]+ ]
		//   [ LEFTMARGIN=[0-9]+ ]
		//   [ RIGHTMARGIN=[0-9]+ ]
		//   [ TABSTOPS=[0-9]+{,[0-9]+} ]
		//
		// Change the different parameters as indicated. The
		// sizes are all in TWIPs. There can be multiple
		// positions for the tab stops. These are seperated by
		// commas.
		// <U>...</U> -- underline


		bool	m_use_outlines;	// when true, use specified SWF internal font.  Otherwise, renderer picks a default font

		int	m_font_id;
		font*	m_font;
		float	m_text_height;

		rgba	m_color;
		int	m_max_length;

		enum alignment
		{
			ALIGN_LEFT = 0,
			ALIGN_RIGHT,
			ALIGN_CENTER,
			ALIGN_JUSTIFY	// probably don't need to implement...
		};
		alignment	m_alignment;
		
		float	m_left_margin;	// extra space between box border and text
		float	m_right_margin;
		float	m_indent;	// how much to indent the first line of multiline text
		float	m_leading;	// extra space between lines (in addition to default font line spacing)
		tu_string	m_default_text;

		edit_text_character_def(movie_definition_sub* root_def)
			:
			m_root_def(root_def),
			m_word_wrap(false),
			m_multiline(false),
			m_password(false),
			m_readonly(false),
			m_auto_size(false),
			m_no_select(false),
			m_border(false),
			m_html(false),
			m_use_outlines(false),
			m_font_id(-1),
			m_font(NULL),
			m_text_height(1.0f),
			m_max_length(0),
			m_alignment(ALIGN_LEFT),
			m_left_margin(0.0f),
			m_right_margin(0.0f),
			m_indent(0.0f),
			m_leading(0.0f)
		{
			assert(m_root_def);

			m_color.set(0, 0, 0, 255);
		}

		// Set the format of the text
		void	set_format(text_format &format)
		{
			m_format = format;
		}
		
		~edit_text_character_def()
		{
		}


		character*	create_character_instance(movie* parent, int id);


		void	read(stream* in, int tag_type, movie_definition_sub* m)
		{
			assert(m != NULL);
			assert(tag_type == 37);

			m_rect.read(in);

			in->align();
			bool	has_text = in->read_uint(1) ? true : false;
			m_word_wrap = in->read_uint(1) ? true : false;
			m_multiline = in->read_uint(1) ? true : false;
			m_password = in->read_uint(1) ? true : false;
			m_readonly = in->read_uint(1) ? true : false;
			bool	has_color = in->read_uint(1) ? true : false;
			bool	has_max_length = in->read_uint(1) ? true : false;
			bool	has_font = in->read_uint(1) ? true : false;

			in->read_uint(1);	// reserved
			m_auto_size = in->read_uint(1) ? true : false;
			bool	has_layout = in->read_uint(1) ? true : false;
			m_no_select = in->read_uint(1) ? true : false;
			m_border = in->read_uint(1) ? true : false;
			in->read_uint(1);	// reserved
			m_html = in->read_uint(1) ? true : false;
			m_use_outlines = in->read_uint(1) ? true : false;

			if (has_font)
			{
				m_font_id = in->read_u16();
				m_text_height = (float) in->read_u16();
			}

			if (has_color)
			{
				m_color.read_rgba(in);
			}

			if (has_max_length)
			{
				m_max_length = in->read_u16();
			}

			if (has_layout)
			{
				m_alignment = (alignment) in->read_u8();
				m_left_margin = (float) in->read_u16();
				m_right_margin = (float) in->read_u16();
				m_indent = (float) in->read_s16();
				m_leading = (float) in->read_s16();
			}

			char*	name = in->read_string();
			m_default_name = name;
			delete [] name;

			if (has_text)
			{
				char*	str = in->read_string();
				m_default_text = str;
				delete [] str;
			}

			IF_VERBOSE_PARSE(log_msg("edit_text_char, varname = %s, text = %s\n",
						 m_default_name.c_str(), m_default_text.c_str()));
		}
	};


	//
	// edit_text_character
	//


	// [DICE-MOD SwiffPlayer #37: DICE's text field. The shipped SwiffPlayer replaced gameswf's
	// edit_text_character with a field that lays its text out through RENDERER-SIDE text-line objects
	// (render::text_* - one "font wrapper" per laid-out line, filled a character at a time so the
	// renderer can measure for word wrap), draws them at device size, and is the game's keyboard
	// input: focus, caret, selection, clipboard, Tab order, restrict/maxChars/password. Read from
	// SwiffPlayer.dll (2).i64: ctor sub_100875C0, formatText sub_10086080 / set_line sub_10085F50,
	// display sub_100882F0, mouse sub_10088650, key sub_10088ED0, insert sub_10088A70, delete
	// sub_10088DB0, focus sub_10087AC0, members sub_10089440 / sub_10087C60, notify paths
	// SwiffPlayer_notifyChar / setTextInput / getTextInput0/1. The stock glyph layout stays as the
	// fallback the shipped keeps (sub_10086C00) for a field whose font has no renderer name.]
	struct edit_text_character;
	static edit_text_character*		s_focused_field = NULL;		// dword_100F8EE0
	static array<edit_text_character*>	s_tab_table;			// dword_100F8CB4 / dword_100F8CB8
	static tu_string			s_text_clipboard;			// `src`
	static bool	s_mod_shift = false, s_mod_alt = false, s_mod_ctrl = false, s_mod_caps = false;	// byte_100F8EE8..EB

	// The digit-row keymap byte_100EC988 (three entries per key code 48..57: plain, shifted, AltGr).
	// A Swedish layout: shift-2 is ", shift-4 is ¤, AltGr-2 is @, AltGr-7 is {.
	static const unsigned char	s_digit_keymap[30] =
	{
		0x30, 0x3D, 0x7D,  0x31, 0x21, 0x00,  0x32, 0x22, 0x40,  0x33, 0x23, 0xA3,  0x34, 0x24, 0xA4,
		0x35, 0x25, 0x80,  0x36, 0x26, 0x00,  0x37, 0x2F, 0x7B,  0x38, 0x28, 0x5B,  0x39, 0x29, 0x5D,
	};

	struct edit_text_character : public character, public text_input
	{
		edit_text_character_def*	m_def;
		array<text_glyph_record>	m_text_glyph_records;
		array<fill_style>	m_dummy_style;	// used to pass a color on to shape_character::display()
		array<line_style>	m_dummy_line_style;
		rect	m_text_bounding_box;	// bounds of dynamic text, as laid out
		tu_string	m_text;				// +300

		// DICE's members (shipped offsets in the comments).
		array<void*>	m_lines;		// +8696/+8700: the renderer text-line objects, one per laid-out line
		tu_string	m_font_name;		// +8680
		array<Uint16>	m_wide_text;	// +316 (a std::wstring): set through a wide String object; empty -> m_text
		bool	m_border;				// +264
		bool	m_background;			// +269
		rgba	m_border_color;			// +265 (black)
		rgba	m_background_color;		// +270 (white)
		rgba	m_text_color;			// +274
		int	m_mouse_state;				// +280: bit 0 = inside, bit 1 = button down
		int	m_prev_mouse_state;			// +284
		bool	m_first_mouse_frame;	// +288
		bool	m_dragging;				// +289
		float	m_drag_x, m_drag_y;		// +292 / +296 (twips, local)
		Uint16	m_sel_start, m_sel_end, m_caret;	// +244 / +246 / +248
		float	m_blink_time;			// +252: advance() accumulates, display blinks on (int)(t*3)&1, edits reset
		bool	m_lines_dirty;			// +344
		bool	m_password;				// +345
		int	m_tab_index;				// +348
		int	m_max_chars;				// +352
		// [DICE-MOD SwiffPlayer #53] +368 (0x170): the last height sub_10084DE0 was asked for
		// (0x10084E01 stores its int argument before the scale test), i.e. formatText's laid-out
		// height in pixels. `textHeight` answers it (vslot 0x10C = 0x10087A90, `mov eax,[ecx+170h]`);
		// the constructor seeds 666 (0x1008772C).
		int	m_text_height_px;
		tu_string	m_restrict;			// +356/+360
		as_value	m_on_changed;		// +376
		as_value	m_on_set_focus;		// +408
		as_environment	m_as_environment;	// +440

		edit_text_character(movie* parent, edit_text_character_def* def, int id)
			:
			character(parent, id),
			m_def(def),
			m_border(false),
			m_background(false),
			m_border_color(0, 0, 0, 255),
			m_background_color(255, 255, 255, 255),
			m_text_color(0, 0, 0, 255),
			m_mouse_state(0),
			m_prev_mouse_state(0),
			m_first_mouse_frame(true),
			m_dragging(false),
			m_drag_x(0), m_drag_y(0),
			m_sel_start(0), m_sel_end(0), m_caret(0),
			m_blink_time(0),
			m_lines_dirty(false),
			m_password(false),
			m_tab_index(-1),
			m_max_chars(0),
			m_text_height_px(666)
		{
			assert(parent);
			assert(m_def);
			m_as_environment.set_target(this);

			// sub_100875C0: the def's border flag drives both border and background; colours black on
			// white; the text colour, password and maxChars from the def.
			m_border = m_def->m_border;
			m_background = m_def->m_border;
			m_text_color = m_def->m_color;
			m_password = m_def->m_password;
			m_max_chars = m_def->m_max_length;

			// The Tab index the parent hands out (shipped: parent vslot +112). [INFERRED: the
			// character id - the value the shipped call returns is not attested beyond "an int >= 0".]
			set_tab_index(id);

			// The renderer font: when the movie resolves the def's font to a NAME (the export name of
			// the font sharing its face - "StandardTextBold" for the login page's TradeGothic fields,
			// see get_exported_font_name), the first text line is created now (height in pixels);
			// without one the field keeps the stock glyph layout and draws no text - as the shipped
			// does for a face the archive has no file for.
			if (m_def->m_font && m_def->m_root_def)
			{
				tu_string	exported;
				if (m_def->m_root_def->get_exported_font_name(m_def->m_font, &exported) && exported.length() > 0)
				{
					m_font_name = exported;
					void*	line = render::text_create(m_font_name.c_str(), (int) (m_def->m_text_height * 0.05f));
					if (line)
					{
						m_lines.push_back(line);
					}
				}
			}

			set_text_value(m_def->m_default_text.c_str());
			m_dummy_style.push_back(fill_style());
			reset_bounding_box(0, 0);
		}

		~edit_text_character()
		{
			// sub_10089D50: release the renderer lines, leave the Tab table and the focus.
			for (int i = 0; i < m_lines.size(); i++)
			{
				if (m_lines[i]) render::text_release(m_lines[i]);
			}
			m_lines.resize(0);
			if (m_tab_index >= 0 && m_tab_index < s_tab_table.size() && s_tab_table[m_tab_index] == this)
			{
				s_tab_table[m_tab_index] = NULL;
			}
			if (s_focused_field == this)
			{
				s_focused_field = NULL;
			}
		}

		// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. Adds the
		// field's environment and its two handler values to the base character's values.
		virtual void	release_script_values(array<as_value>* held)
		{
			character::release_script_values(held);
			m_as_environment.release_values(held);
			hold_script_value(held, m_on_changed);
			m_on_changed.set_undefined();
			hold_script_value(held, m_on_set_focus);
			m_on_set_focus.set_undefined();
		}

		void	set_tab_index(int index)	// sub_10087B50
		{
			if (m_tab_index >= 0 && m_tab_index < s_tab_table.size() && s_tab_table[m_tab_index] == this)
			{
				s_tab_table[m_tab_index] = NULL;
			}
			m_tab_index = index;
			if (index >= 0)
			{
				if (index >= s_tab_table.size())
				{
					const int	old = s_tab_table.size();
					s_tab_table.resize(index + 1);
					for (int i = old; i < s_tab_table.size(); i++) s_tab_table[i] = NULL;
				}
				s_tab_table[index] = this;
			}
		}

		// TWIPS, the 2006 character convention: sprite_instance::get_width() derives a clip's width from
		// its children's, and the stock `_width` setter divides PIXELS_TO_TWIPS(value) by it - reporting
		// pixels here scaled every label clip (whose only child is its field) twenty-fold (retail run X,
		// 1640 px line widths, no wrapping). The shipped vslot 252 is in pixels; the AS-visible `_width`
		// stays in pixels through the conversions at the use sites below.
		virtual float	get_width()  { return m_def->m_rect.width(); }
		virtual float	get_height() { return m_def->m_rect.height(); }
		// [DICE-MOD SwiffPlayer #39] the unscaled local extent Flash's `_width`/`_height` build on.
		virtual float	get_local_width()  { return m_def->m_rect.width(); }
		virtual float	get_local_height() { return m_def->m_rect.height(); }

		virtual const char*	get_text_name() const { return m_def->m_default_name.c_str(); }


		void	reset_bounding_box(float x, float y)
		// Reset our text bounding box to the given point.
		{
			m_text_bounding_box.m_x_min = x;
			m_text_bounding_box.m_x_max = x;
			m_text_bounding_box.m_y_min = y;
			m_text_bounding_box.m_y_max = y;
		}


		virtual void	set_text_value(const char* new_text)
		// Set our text to the given string. sub_10087510: same text -> nothing; the field's own
		// maxChars clamps; then the layout. The WIDE store is NOT touched: the two stores are
		// independent in the shipped field (this setter writes +300, the wide one +316), and the
		// line layout prefers the wide one - which is how a label survives the components'
		// `arrange` writing the localisation KEY over a translation that arrived as a String
		// object (retail run W showed the keys when this cleared m_wide_text).
		{
			if (m_text == new_text)
			{
				return;
			}
			m_text = new_text;
			if (m_max_chars > 0 && m_text.length() > m_max_chars)
			{
				m_text.resize(m_max_chars);
			}
			format_text();
		}

		// The wide setter (shipped vslot +272, sub_100893E0): a String object carrying UTF-16 text.
		// Same text -> nothing; else the wide store alone is replaced (the narrow one at +300 is NOT
		// synced), clamped to maxChars, and the layout runs.
		void	set_wide_text_value(const Uint16* wide)
		{
			int	n = 0;
			for (const Uint16* p = wide; p && *p; ++p) n++;
			if (m_wide_text.size() == n + 1)
			{
				bool	same = true;
				for (int i = 0; i < n; i++) { if (m_wide_text[i] != wide[i]) { same = false; break; } }
				if (same) return;
			}
			m_wide_text.resize(0);
			for (int i = 0; i < n; i++)
			{
				m_wide_text.push_back(wide[i]);
			}
			if (m_max_chars > 0 && m_wide_text.size() > m_max_chars)
			{
				m_wide_text.resize(m_max_chars);
			}
			m_wide_text.push_back(0);
			format_text();
		}

		virtual const char*	get_text_value() const
		{
			return m_text.c_str();
		}


		void	set_member(const tu_stringi& name, const as_value& val)
		// We have a "text" member.
		{
			// @@ TODO need to inherit basic stuff like _x, _y, _xscale, _yscale etc

			if (dice_set_member(name, val))		// [DICE-MOD #37] the shipped member surface, sub_10089440
			{
				return;
			}
			as_standard_member	std_member = get_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_TEXT:
				//if (name == "text")
			{
				int version = get_parent()->get_movie_definition()->get_version();
				set_text_value(val.to_tu_string_versioned(version));
				return;
			}
			case M_X:
				//else if (name == "_x")
			{
				matrix	m = get_matrix();
				m.m_[0][2] = (float) PIXELS_TO_TWIPS(val.to_number());
				set_matrix(m);

				// m_accept_anim_moves = false;
				
				return;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();
				m.m_[1][2] = (float) PIXELS_TO_TWIPS(val.to_number());
				set_matrix(m);

				// m_accept_anim_moves = false;
				
				return;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				set_visible(val.to_bool());
				return;
			}
			case M_ALPHA:
				//else if (name == "_alpha")
			{
				// @@ TODO this should be generic to struct character!
				// Arg is in percent.
				cxform	cx = get_cxform();
				cx.m_[3][0] = fclamp(float(val.to_number()) / 100.f, 0, 1);
				set_cxform(cx);
				return;
			}
			case M_TEXTCOLOR:
				//else if (name == "textColor")
			{	
				// The arg is 0xRRGGBB format.
#if defined(__APPLE__)
				// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
				Uint32	rgb = bf2x87::lowUInt32(val.to_number());
#else
				Uint32	rgb = (Uint32) val.to_number();
#endif

				cxform	cx = get_cxform();
				cx.m_[0][0] = fclamp(((rgb >> 16) & 255) / 255.0f, 0, 1);
				cx.m_[1][0] = fclamp(((rgb >>  8) & 255) / 255.0f, 0, 1);
				cx.m_[2][0] = fclamp(((rgb      ) & 255) / 255.0f, 0, 1);
				set_cxform(cx);

				return;
			}
			// @@ TODO see TextField members in Flash MX docs
			}	// end switch
		}


		bool	get_member(const tu_stringi& name, as_value* val)
		{
			if (dice_get_member(name, val))		// [DICE-MOD #37] the shipped member surface, sub_10087C60
			{
				return true;
			}
			as_standard_member	std_member = get_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_TEXT:
				//if (name == "text")
			{
				val->set_tu_string(m_text);
				return true;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				val->set_bool(get_visible());
				return true;
			}
			case M_ALPHA:
				//else if (name == "_alpha")
			{
				// @@ TODO this should be generic to struct character!
				const cxform&	cx = get_cxform();
				val->set_double(cx.m_[3][0] * 100.f);
				return true;
			}
			case M_TEXTCOLOR:
				//else if (name == "textColor")
			{
				// Return color in 0xRRGGBB format
				const cxform&	cx = get_cxform();
				int	r = iclamp(int(cx.m_[0][0] * 255), 0, 255);
				int	g = iclamp(int(cx.m_[0][0] * 255), 0, 255);
				int	b = iclamp(int(cx.m_[0][0] * 255), 0, 255);
				val->set_int((r << 16) + (g << 8) + b);
				return true;
			}
			case M_X:
				//else if (name == "_x")
			{
				matrix	m = get_matrix();	// @@ get_world_matrix()???
				val->set_double(TWIPS_TO_PIXELS(m.m_[0][2]));
				return true;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();	// @@ get_world_matrix()???
				val->set_double(TWIPS_TO_PIXELS(m.m_[1][2]));
				return true;
			}
			case M_WIDTH:
				//else if (name == "_width")
			{
				// @@ TODO should implement this in
				// character and inherit into both here and sprite_instance
				rect	transformed_rect;
				transformed_rect.enclose_transformed_rect(get_world_matrix(), m_def->m_rect);
				val->set_double(TWIPS_TO_PIXELS(transformed_rect.width()));
				return true;
			}
			case M_HEIGHT:
				//else if (name == "_height")
			{
				// @@ TODO should implement this in
				// character and inherit into both here and sprite_instance
				rect	transformed_rect;
				transformed_rect.enclose_transformed_rect(get_world_matrix(), m_def->m_rect);
				val->set_double(TWIPS_TO_PIXELS(transformed_rect.height()));
				return true;
			}
			case M_TEXTWIDTH:
				//else if (name == "textWidth")
			{
				// Return the width, in pixels, of the text as laid out.
				// (I.e. the actual text content, not our defined
				// bounding box.)
				//
				// In local coords.  Verified against Macromedia Flash.
				val->set_double(TWIPS_TO_PIXELS(m_text_bounding_box.width()));

				return true;
			}
			}	// end switch

			return false;
		}

		
		// @@ WIDTH_FUDGE is a total fudge to make it match the Flash player!  Maybe
		// we have a bug?
		#define WIDTH_FUDGE 80.0f


		void	align_line(edit_text_character_def::alignment align, int last_line_start_record, float x)
		// Does LEFT/CENTER/RIGHT alignment on the records in
		// m_text_glyph_records[], starting with
		// last_line_start_record and going through the end of
		// m_text_glyph_records.
		{
			float	extra_space = (m_def->m_rect.width() - m_def->m_right_margin) - x - WIDTH_FUDGE;
			assert(extra_space >= 0.0f);

			float	shift_right = 0.0f;

			if (align == edit_text_character_def::ALIGN_LEFT)
			{
				// Nothing to do; already aligned left.
				return;
			}
			else if (align == edit_text_character_def::ALIGN_CENTER)
			{
				// Distribute the space evenly on both sides.
				shift_right = extra_space / 2;
			}
			else if (align == edit_text_character_def::ALIGN_RIGHT)
			{
				// Shift all the way to the right.
				shift_right = extra_space;
			}

			// Shift the beginnings of the records on this line.
			for (int i = last_line_start_record; i < m_text_glyph_records.size(); i++)
			{
				text_glyph_record&	rec = m_text_glyph_records[i];

				if (rec.m_style.m_has_x_offset)
				{
					rec.m_style.m_x_offset += shift_right;
				}
			}
		}

		// Convert the characters in m_text into a series of
		// text_glyph_records to be rendered.
		// [DICE-MOD #37] sub_10086C00: with renderer lines the renderer lays the text out, otherwise the
		// stock glyph layout below runs (the shipped keeps both).
		void	format_text()
		{
			if (m_lines.size() > 0)
			{
				format_text_lines();
				return;
			}
			format_text_glyphs();
		}

		// sub_10085F50: put `text` on line `index`, creating the line when it is the next one.
		void	set_line_text(int index, float width_px, const array<Uint16>& text)
		{
			if (index >= m_lines.size())
			{
				void*	line = render::text_create(m_font_name.c_str(), (int) (m_def->m_text_height * 0.05f));
				m_lines.push_back(line);
			}
			void*	line = m_lines[index];
			render::text_set_size(line, width_px, (int) m_def->m_alignment);
			if (!m_password)
			{
				render::text_set(line, BF2_DICE_WIDE(&text[0]));	// [CHOSEN, not recovered] POSIX: widen, see dice_wide
				return;
			}
			// A password field shows one asterisk per character (the shipped fills a 512-wide buffer).
			static Uint16	s_asterisks[512];
			if (s_asterisks[0] == 0)
			{
				for (int i = 0; i < 511; i++) s_asterisks[i] = '*';
				s_asterisks[511] = 0;
			}
			int	n = 0;
			while (text[n]) n++;
			if (n > 511) n = 511;
			render::text_set(line, BF2_DICE_WIDE(&s_asterisks[511 - n]));
		}

		// sub_10084DE0: grow the field's y scale so at least `height_px` is visible (auto-size).
		void	set_height_at_least(float height_px)
		{
			m_text_height_px = (int) height_px;		// 0x10084E01, before the test
			matrix	m = get_matrix();
			const float	h = TWIPS_TO_PIXELS(get_height());
			if (h * m.m_[1][1] < height_px && h > 0)
			{
				m.m_[1][1] = height_px / h;
				set_matrix(m);
			}
		}

		// sub_10086080 - formatText: the renderer-line layout.
		void	format_text_lines()
		{
			if (m_font_name.length() == 0)
			{
				return;
			}

			// The wide source: the wide copy when one was set, else the narrow text widened byte by
			// byte (the shipped mbstowcs of +300).
			array<Uint16>	wide;
			if (m_wide_text.size() > 1)
			{
				wide = m_wide_text;
			}
			else
			{
				for (int i = 0; i < m_text.length(); i++) wide.push_back((Uint16) (unsigned char) m_text[i]);
				wide.push_back(0);
			}

			const matrix	world = get_world_matrix();
			const float	width_px = TWIPS_TO_PIXELS(get_width()) * world.m_[0][0];
			const float	height_limit = get_height() * world.m_[1][1];		// twips
			const float	line_height = m_def->m_leading + m_def->m_text_height;		// twips (+100 + +72)
			const float	wrap_limit = width_px - m_def->m_right_margin * 0.05f;

			array<Uint16>	buf;
			buf.push_back(0);
			float	y = 0.0f;
			int	line = 0;
			int	last_space = -1;
			bool	overflowed = false;
			int	i = 0;
			for (;;)
			{
				const Uint16	c = wide[i++];
				if (y > height_limit)
				{
					overflowed = true;
					break;
				}
				if (c == 13 || c == 10)
				{
					set_line_text(line++, width_px, buf);
					buf.resize(0);
					buf.push_back(0);
					y += line_height;
				}
				else
				{
					if (c == 32)
					{
						last_space = buf.size() - 1;
					}
					buf[buf.size() - 1] = c;
					buf.push_back(0);
					set_line_text(line, width_px, buf);
					if (m_def->m_word_wrap)
					{
						float	w = 0, h = 0;
						render::text_extent(m_lines[line], buf.size() - 1, &w, &h);
						if (wrap_limit > w)
						{
							// fits
						}
						else if (last_space == -1)
						{
							// No space to break at: the last character opens the next line.
							buf.resize(buf.size() - 2);
							buf.push_back(0);
							set_line_text(line++, width_px, buf);
							buf.resize(0);
							buf.push_back(c);
							buf.push_back(0);
							y += line_height;
						}
						else
						{
							// Break at the last space: the tail after it moves to the next line.
							array<Uint16>	tail;
							for (int k = last_space + 1; k < buf.size(); k++) tail.push_back(buf[k]);
							buf.resize(last_space);
							buf.push_back(0);
							set_line_text(line++, width_px, buf);
							buf = tail;
							last_space = -1;
							y += line_height;
						}
					}
				}
				if (c == 0)
				{
					break;
				}
			}
			if (overflowed)
			{
				line--;
			}
			else if (buf.size() <= 1)
			{
				line--;
			}

			const int	count = m_lines.size();
			if (line >= count && line > 0)
			{
				line = count - 1;
			}
			for (int k = 0; k < m_lines.size(); k++)
			{
				render::text_caret(m_lines[k], m_caret);
			}
			for (int k = line + 1; k < m_lines.size(); k++)
			{
				render::text_release(m_lines[k]);
			}
			if (line + 1 != m_lines.size() && line + 1 >= 0)
			{
				m_lines.resize(line + 1);
			}

			// [DICE-MOD SwiffPlayer #52] sub_10086080's tail (0x10086433..0x10086495): the height the
			// field must show is the font's LEADING (+0x40, the same term as the caret's, #47) plus the
			// definition's text height, plus the laid-out y for word-wrapped fields or the definition's
			// leading otherwise, in pixels. Until 2026-09-13 this port added the font's ASCENT instead,
			// which grew every input field's y scale three to four times: the caret and text sat low
			// in the box and the field's hit rectangle reached two rows down, so a click on the e-mail
			// field still belonged to ACCOUNT NAME (owner's report, the CREATE ACCOUNT page).
			const float	leading = m_def->m_font ? m_def->m_font->get_leading() : 0.0f;
			float	height_px;
			if (m_def->m_word_wrap)
			{
				height_px = (leading + m_def->m_text_height + y) * 0.05f;
			}
			else
			{
				height_px = (leading + m_def->m_leading + m_def->m_text_height) * 0.05f;
			}
			set_height_at_least((float) (int) height_px);
			m_lines_dirty = true;
		}

		void	format_text_glyphs()
		{
			m_text_glyph_records.resize(0);

			if (m_def->m_font == NULL)
			{
				return;
			}

			// @@ mostly for debugging
			// Font substitution -- if the font has no
			// glyphs, try some other defined font!
			if (m_def->m_font->get_glyph_count() == 0)
			{
				// Find a better font.
				font*	newfont = m_def->m_font;
				for (int i = 0, n = fontlib::get_font_count(); i < n; i++)
				{
					font*	f = fontlib::get_font(i);
					assert(f);

					if (f->get_glyph_count() > 0)
					{
						// This one looks good.
						newfont = f;
						break;
					}
				}

				if (m_def->m_font != newfont)
				{
					log_error("error: substituting font!  font '%s' has no glyphs, using font '%s'\n",
						  fontlib::get_font_name(m_def->m_font),
						  fontlib::get_font_name(newfont));

					m_def->m_font = newfont;
				}
			}


			float	scale = m_def->m_text_height / 1024.0f;	// the EM square is 1024 x 1024

			text_glyph_record	rec;	// one to work on
			rec.m_style.m_font = m_def->m_font;
			rec.m_style.m_color = m_def->m_color;
			rec.m_style.m_x_offset = tu_util::fmax(0, m_def->m_left_margin + m_def->m_indent);
			rec.m_style.m_y_offset = m_def->m_text_height
				+ (m_def->m_font->get_leading() - m_def->m_font->get_descent()) * scale;
			rec.m_style.m_text_height = m_def->m_text_height;
			rec.m_style.m_has_x_offset = true;
			rec.m_style.m_has_y_offset = true;

			float	x = rec.m_style.m_x_offset;
			float	y = rec.m_style.m_y_offset;

			// Start the bbox at the upper-left corner of the first glyph.
 			reset_bounding_box(x, y - m_def->m_font->get_descent() * scale + m_def->m_text_height);

			float	leading = m_def->m_leading;
			leading += m_def->m_font->get_leading() * scale;

			int	last_code = -1;
			int	last_space_glyph = -1;
			int	last_line_start_record = 0;

			const char*	text = &m_text[0];
			while (Uint32 code = utf8::decode_next_unicode_character(&text))
			{
// @@ try to truncate overflow text??
#if 0
				if (y + m_def->m_font->get_descent() * scale > m_def->m_rect.height())
				{
					// Text goes below the bottom of our bounding box.
					rec.m_glyphs.resize(0);
					break;
				}
#endif // 0

				//Uint16	code = m_text[j];

				x += m_def->m_font->get_kerning_adjustment(last_code, (int) code) * scale;
				last_code = (int) code;

				// Expand the bounding-box to the lower-right corner of each glyph as
				// we generate it.
				m_text_bounding_box.expand_to_point(x, y + m_def->m_font->get_descent() * scale);

				if (code == 13 || code == 10)
				{
					// newline.

					// Frigging Flash seems to use '\r' (13) as its
					// default newline character.  If we get DOS-style \r\n
					// sequences, it'll show up as double newlines, so maybe we
					// need to detect \r\n and treat it as one newline.

					// Close out this stretch of glyphs.
					m_text_glyph_records.push_back(rec);
					align_line(m_def->m_alignment, last_line_start_record, x);

					x = tu_util::fmax(0, m_def->m_left_margin + m_def->m_indent);	// new paragraphs get the indent.
					y += m_def->m_text_height + leading;

					// Start a new record on the next line.
					rec.m_glyphs.resize(0);
					rec.m_style.m_font = m_def->m_font;
					rec.m_style.m_color = m_def->m_color;
					rec.m_style.m_x_offset = x;
					rec.m_style.m_y_offset = y;
					rec.m_style.m_text_height = m_def->m_text_height;
					rec.m_style.m_has_x_offset = true;
					rec.m_style.m_has_y_offset = true;

					last_space_glyph = -1;
					last_line_start_record = m_text_glyph_records.size();

					continue;
				}

				if (code == 8)
				{
					// backspace (ASCII BS).

					// This is a limited hack to enable overstrike effects.
					// It backs the cursor up by one character and then continues
					// the layout.  E.g. you can use this to display an underline
					// cursor inside a simulated text-entry box.
					//
					// ActionScript understands the '\b' escape sequence
					// for inserting a BS character.
					//
					// ONLY WORKS FOR BACKSPACING OVER ONE CHARACTER, WON'T BS
					// OVER NEWLINES, ETC.

					if (rec.m_glyphs.size() > 0)
					{
						// Peek at the previous glyph, and zero out its advance
						// value, so the next char overwrites it.
						float	advance = rec.m_glyphs.back().m_glyph_advance;
						x -= advance;	// maintain formatting
						rec.m_glyphs.back().m_glyph_advance = 0;	// do the BS effect
					}
					continue;
				}

				// Remember where word breaks occur.
				if (code == 32)
				{
					last_space_glyph = rec.m_glyphs.size();
				}

				int	index = m_def->m_font->get_glyph_index((Uint16) code);
				if (index == -1)
				{
					// error -- missing glyph!
					
					// Log an error, but don't log too many times.
					static int	s_log_count = 0;
					if (s_log_count < 10)
					{
						s_log_count++;
						log_error("edit_text_character::display() -- missing glyph for char %d "
							  "-- make sure character shapes for font %s are being exported "
							  "into your SWF file!\n",
							  code,
							  m_def->m_font->get_name());
					}

					// Drop through and use index == -1; this will display
					// using the empty-box glyph
				}
				text_glyph_record::glyph_entry	ge;
				ge.m_glyph_index = index;
				ge.m_glyph_advance = scale * m_def->m_font->get_advance(index);

				rec.m_glyphs.push_back(ge);

				x += ge.m_glyph_advance;

				
				if (x >= m_def->m_rect.width() - m_def->m_right_margin - WIDTH_FUDGE)
				{
					// Whoops, we just exceeded the box width.  Do word-wrap.

					// Insert newline.

					// Close out this stretch of glyphs.
					m_text_glyph_records.push_back(rec);
					float	previous_x = x;

					x = m_def->m_left_margin;
					y += m_def->m_text_height + leading;

					// Start a new record on the next line.
					rec.m_glyphs.resize(0);
					rec.m_style.m_font = m_def->m_font;
					rec.m_style.m_color = m_def->m_color;
					rec.m_style.m_x_offset = x;
					rec.m_style.m_y_offset = y;
					rec.m_style.m_text_height = m_def->m_text_height;
					rec.m_style.m_has_x_offset = true;
					rec.m_style.m_has_y_offset = true;
					
					text_glyph_record&	last_line = m_text_glyph_records.back();
					if (last_space_glyph == -1)
					{
						// Pull the previous glyph down onto the
						// new line.
						if (last_line.m_glyphs.size() > 0)
						{
							rec.m_glyphs.push_back(last_line.m_glyphs.back());
							x += last_line.m_glyphs.back().m_glyph_advance;
							previous_x -= last_line.m_glyphs.back().m_glyph_advance;
							last_line.m_glyphs.resize(last_line.m_glyphs.size() - 1);
						}
					}
					else
					{
						// Move the previous word down onto the next line.

						previous_x -= last_line.m_glyphs[last_space_glyph].m_glyph_advance;

						for (int i = last_space_glyph + 1; i < last_line.m_glyphs.size(); i++)
						{
							rec.m_glyphs.push_back(last_line.m_glyphs[i]);
							x += last_line.m_glyphs[i].m_glyph_advance;
							previous_x -= last_line.m_glyphs[i].m_glyph_advance;
						}
						last_line.m_glyphs.resize(last_space_glyph);
					}

					align_line(m_def->m_alignment, last_line_start_record, previous_x);

					last_space_glyph = -1;
					last_line_start_record = m_text_glyph_records.size();
				}

				// TODO: HTML markup
			}

			// Add this line to our output.
			m_text_glyph_records.push_back(rec);
			align_line(m_def->m_alignment, last_line_start_record, x);
		}


		// [DICE-MOD #37] sub_100882F0 - display: refresh dirty lines, the world matrix with its SCALE
		// reset to one (text is drawn at device size, translation only), background quad and border,
		// the blinking caret when focused, then every line through render::text_draw with the y
		// advanced by leading + height. The stock glyph records are never drawn (the shipped does not).
		void	display()
		{
			if (m_lines_dirty)
			{
				for (int i = 0; i < m_lines.size(); i++)
				{
					render::text_detach(m_lines[i]);
				}
				m_lines_dirty = false;
			}

			matrix	mat = get_world_matrix();
			mat.m_[0][0] = 1.0f;
			mat.m_[1][1] = 1.0f;
			render::set_matrix(mat);
			render::set_cxform(get_world_cxform());

			point	coords[4];
			coords[0] = m_def->m_rect.get_corner(0);
			coords[1] = m_def->m_rect.get_corner(1);
			coords[2] = m_def->m_rect.get_corner(3);
			coords[3] = m_def->m_rect.get_corner(2);
			Sint16	icoords[18] =
			{
				// strip (fill in)
				(Sint16) coords[0].m_x, (Sint16) coords[0].m_y,
				(Sint16) coords[1].m_x, (Sint16) coords[1].m_y,
				(Sint16) coords[2].m_x, (Sint16) coords[2].m_y,
				(Sint16) coords[3].m_x, (Sint16) coords[3].m_y,
				// outline
				(Sint16) coords[0].m_x, (Sint16) coords[0].m_y,
				(Sint16) coords[1].m_x, (Sint16) coords[1].m_y,
				(Sint16) coords[3].m_x, (Sint16) coords[3].m_y,
				(Sint16) coords[2].m_x, (Sint16) coords[2].m_y,
				(Sint16) coords[0].m_x, (Sint16) coords[0].m_y,
			};
			if (m_background)
			{
				render::fill_style_color(0, m_background_color);
				render::draw_mesh_strip(&icoords[0], 4);
			}
			if (m_border)
			{
				render::line_style_color(m_border_color);
				render::line_style_width(1.0f);
				render::draw_line_strip(&icoords[8], 5);
			}

			if (m_lines.size() > 0)
			{
				const float	em = m_def->m_text_height * (1.0f / 1024.0f);
				if (s_focused_field == this)
				{
					if (((int) (m_blink_time * 3.0f) & 1) == 0)
					{
						float	w = 0, h = 0;
						render::text_extent(m_lines[0], m_caret, &w, &h);
						const float	x = w * 20.0f;
						// [DICE-MOD SwiffPlayer #47] The font term is the font's LEADING, not its
						// ascent. 100885 8B..9E reads def+0x44 (the font) and multiplies em by
						// FONT+0x40, and font::read fills +0x38/+0x3C/+0x40 with ascent/descent/
						// leading in that order (100A1A7D..100A1AAB), so +0x40 is the third of the
						// three. sub_10086C00's own baseline, (font+0x40 - font+0x3C) * em +
						// text_height, is the stock (leading - descent) line, which pins the same
						// mapping from the other side. Measured: the shipped DLL draws an 11 px
						// caret in the login password field where the ascent reading drew 18.
						// The shipped body does not null-test the font here, and neither does this.
						const float	fontLeading = m_def->m_font->get_leading();
						Sint16	caret[4] =
						{
							(Sint16) x, 0,
							(Sint16) x, (Sint16) (em * fontLeading + m_def->m_leading + m_def->m_text_height),
						};
						render::line_style_color(m_text_color);
						render::line_style_width(1.0f);
						render::draw_line_strip(caret, 2);
					}
				}

				float	left = m_def->m_indent + m_def->m_left_margin;
				if (left < 0) left = 0;
				mat.m_[0][2] += left + 40.0f;
				mat.m_[1][2] += 30.0f;
				for (int i = 0; i < m_lines.size(); i++)
				{
					render::text_draw(m_lines[i], &mat, pack_color(m_text_color), get_depth());
					mat.m_[1][2] += m_def->m_leading + m_def->m_text_height;
				}
			}

			do_display_callback();
		}

		virtual void	advance(float delta_time)	// sub_10087AA0
		{
			m_blink_time += delta_time;
		}

		// The four colour bytes as the one dword the shipped passes (memory order r,g,b,a).
		static unsigned	pack_color(const rgba& c)
		{
			return (unsigned) c.m_r | ((unsigned) c.m_g << 8) | ((unsigned) c.m_b << 16) | ((unsigned) c.m_a << 24);
		}

		void	call_callback(as_value& fn)	// sub_1008EAA0: the field's AS object is `this`
		{
			if (fn.is_function())
			{
				call_method0(fn, &m_as_environment, this);
			}
		}

		void	focus()	// sub_10087AC0
		{
			s_focused_field = this;
			m_caret = (Uint16) m_text.length();
			if (m_lines.size() > 0)
			{
				render::text_caret(m_lines[0], m_caret);
			}
			m_blink_time = 0;
			call_callback(m_on_set_focus);
		}

		static bool	focus_next(int from)	// sub_100891E0: the next Tab index with a field, wrapping
		{
			const int	n = s_tab_table.size();
			for (int i = from + 1; i < n; i++)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			for (int i = 0; i < from && i < n; i++)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			return false;
		}
		static bool	focus_prev(int from)	// sub_10089230
		{
			const int	n = s_tab_table.size();
			for (int i = from - 1; i >= 0; i--)
			{
				if (i < n && s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			for (int i = n - 1; i > from; i--)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			return false;
		}

		// [DICE-MOD SwiffPlayer #51] sub_100879C0: the world-space point through the inverse world
		// matrix, then the definition's rectangle. [INFERRED: the rectangle the shipped body fetches
		// through slot 60 is the definition's.]
		virtual bool	point_test_world(float x, float y)
		{
			matrix	world = get_world_matrix();
			point	p;
			world.transform_by_inverse(&p, point(x, y));
			return m_def->m_rect.point_test(p.m_x, p.m_y);
		}

		// sub_10088650 - the mouse: per-frame state machine on (inside, button) against the previous
		// frame; a press inside starts a drag/selection, its release focuses the field, a press
		// outside a focused field blurs it. [INFERRED: the selection arguments the hit test takes.]
		// [DICE-MOD SwiffPlayer #51] This is the text field's mouse pass (vtable slot 4): it ignores
		// what the siblings above consumed - the argument is unread by the shipped body - so a click
		// reaches the field through any decoration drawn over it, and it answers "inside" (the state's
		// bit 0) for the siblings below. Read-only fields, and every field but the focused one while
		// one is focused, stay out of it. Until 2026-09-13 this port returned the field as stock's
		// "topmost entity", which the Textbox component's bevel shape pre-empted.
		virtual bool	mouse_update(bool /*consumed*/)
		{
			if (m_def->m_readonly)
			{
				return false;
			}
			if (s_focused_field != NULL && s_focused_field != this)
			{
				return false;
			}
			int	mx = 0, my = 0, mb = 0;
			movie*	root = get_root_movie();
			if (root) root->get_mouse_state(&mx, &my, &mb);

			m_prev_mouse_state = m_mouse_state;
			m_mouse_state = (mb & 1) ? 2 : 0;
			matrix	world = get_world_matrix();
			point	p;
			world.transform_by_inverse(&p, point((float) mx * 20.0f, (float) my * 20.0f));
			if (m_def->m_rect.point_test(p.m_x, p.m_y))
			{
				m_mouse_state |= 1;
			}
			// the unscaled local point, for the selection
			world.m_[0][0] = 1.0f;
			world.m_[1][1] = 1.0f;
			world.transform_by_inverse(&p, point((float) mx * 20.0f, (float) my * 20.0f));
			if (m_first_mouse_frame)
			{
				m_prev_mouse_state = m_mouse_state;
				m_first_mouse_frame = false;
			}
			if (m_dragging && m_lines.size() > 0)
			{
				float	x1 = m_drag_x * 0.05f, y1 = m_drag_y * 0.05f;
				float	x2 = p.m_x * 0.05f, y2 = p.m_y * 0.05f;
				int	pos = 0;
				// 0x100887A5..0x100887D7: x2/y2 are fstp'd floats pushed unconverted (no fistp).
				if (render::text_hit(m_lines[0], &pos, x2, y2))
				{
					m_caret = (Uint16) pos;
				}
				m_blink_time = 0;
				if (x1 > x2) { float t = x1; x1 = x2; x2 = t; }
				if (y1 > y2) { float t = y1; y1 = y2; y2 = t; }
				// 0x10088823..0x1008883B: the sorted floats are pushed as raw dwords.
				render::text_set_selection(m_lines[0], x1, y1, x2, y2);
			}
			const bool	result = (m_mouse_state & 1) != 0;
			if (m_mouse_state == m_prev_mouse_state)
			{
				return result;
			}
			switch (m_mouse_state | (m_prev_mouse_state << 2))
			{
			case 12:
			case 13:
				// released after a press inside: focus
				m_dragging = false;
				if (s_focused_field == this) return result;
				focus_from_click();
				return result;
			case 8:
				if (!m_dragging) return result;
				m_dragging = false;
				if (s_focused_field == this) return result;
				focus_from_click();
				return result;
			case 7:
				// pressed inside
				m_drag_x = p.m_x;
				m_drag_y = p.m_y;
				m_dragging = true;
				return result;
			case 2:
				// pressed outside: blur
				if (s_focused_field == this)
				{
					if (m_lines.size() > 0)
					{
						m_caret = 0;
						render::text_caret(m_lines[0], 0);
						render::text_blur(m_lines[0]);
					}
					s_focused_field = NULL;
				}
				return result;
			}
			return result;
		}
		void	focus_from_click()
		{
			m_blink_time = 0;
			s_focused_field = this;
			format_text();
			m_caret = (Uint16) m_text.length();
			if (m_lines.size() > 0)
			{
				render::text_caret(m_lines[0], m_caret);
			}
			call_callback(m_on_set_focus);
		}

		// ---- editing (sub_10088A70 insert, sub_10088DB0 delete, sub_10088D60 delete-selection) ----
		bool	get_selection(int* start, int* end)
		{
			*start = 0; *end = 0;
			if (m_lines.size() == 0) return false;
			// [DICE-MOD SwiffPlayer #46] the RENDERER answers whether a selection exists: the
			// shipped delete_selection (sub_10088D60) tests the Boolean of render::text_selection
			// (sub_1009A910 -> render_handler +112) and only then deletes [start, end). The port's
			// former `start != end` test read the forwarder's scratch start value as a selection
			// whenever the renderer had none, so Backspace and Delete deleted nothing (2026-09-12).
			return render::text_selection(m_lines[0], start, end);
		}
		void	delete_range(int new_length, int end, bool relayout)
		{
			if (new_length == end || new_length < 0) return;
			const int	len = m_text.length();
			if (new_length >= len || end < 0 || end > len) return;
			tu_string	t = m_text;
			t.resize(new_length);
			t += m_text.c_str() + end;
			m_text = t;
			// the wide store is untouched (sub_10088DB0 edits +300 only): a field filled through a
			// String object keeps displaying that text while the narrow copy is edited underneath.
			m_caret = (Uint16) new_length;
			if (relayout) format_text();
			call_callback(m_on_changed);
			if (m_lines.size() > 0) render::text_blur(m_lines[0]);
		}
		bool	delete_selection(bool relayout)
		{
			int	s = 0, e = 0;
			if (!get_selection(&s, &e)) return false;
			if (s > e) { int t = s; s = e; e = t; }
			delete_range(s, e, relayout);
			return true;
		}
		void	insert_text(const char* str)
		{
			delete_selection(false);
			tu_string	ins(str);
			if (m_restrict.length() > 0)
			{
				tu_string	kept;
				for (int i = 0; i < ins.length(); i++)
				{
					for (int k = 0; k < m_restrict.length(); k++)
					{
						if (ins[i] == m_restrict[k]) { kept += ins[i]; break; }
					}
				}
				if (kept.length() == 0) return;
				ins = kept;
			}
			if (m_caret > m_text.length()) m_caret = (Uint16) m_text.length();
			tu_string	t = m_text;
			t.resize(m_caret);
			t += ins;
			t += m_text.c_str() + m_caret;
			m_text = t;
			// the wide store is untouched (sub_10088A70 edits +300 only), as in delete_range.
			m_caret = (Uint16) (m_caret + ins.length());
			if (m_max_chars > 0 && m_text.length() > m_max_chars)
			{
				m_text.resize(m_max_chars);
			}
			if (m_caret > m_text.length()) m_caret = (Uint16) m_text.length();
			format_text();
			call_callback(m_on_changed);
			m_blink_time = 0;
		}

		// ---- text_input (the shipped +200 interface) ----
		virtual void	on_key(int vkey)	// sub_10088ED0
		{
			if (vkey == 9 && m_tab_index > 0)
			{
				if (s_mod_shift ? focus_prev(m_tab_index) : focus_next(m_tab_index)) return;
			}
			if (m_lines.size() == 0) return;
			if (s_mod_ctrl)
			{
				if (vkey == 67) copy();
				else if (vkey == 86) paste();
				else if (vkey == 88) cut();
				return;
			}
			char	ch = 0;
			if (vkey >= 65 && vkey <= 90)
			{
				ch = (char) (vkey + (s_mod_shift ? 0 : 0x20));
			}
			else if (vkey == 32)
			{
				ch = ' ';
			}
			else if (vkey >= 48 && vkey <= 57)
			{
				const int	base = 3 * (vkey - 48);
				if (s_mod_shift)
				{
					if (s_mod_alt) return;
					ch = (char) s_digit_keymap[base + 1];
				}
				else if (!s_mod_caps)
				{
					ch = (char) s_digit_keymap[base + (s_mod_alt ? 2 : 0)];
				}
				else
				{
					ch = (char) s_digit_keymap[base + 1];
				}
			}
			else
			{
				switch (vkey)
				{
				case 46:	// Delete
					if (s_mod_alt) return;
					if (!delete_selection(true)) delete_range(m_caret, m_caret + 1, true);
					break;
				case 8:		// Backspace
					if (s_mod_alt) return;
					if (!delete_selection(true)) delete_range(m_caret - 1, m_caret, true);
					break;
				case 37:	// Left
					m_caret = (Uint16) (m_caret > 0 ? m_caret - 1 : 0);
					format_text();
					break;
				case 39:	// Right
					m_caret = (Uint16) (m_caret + 1 < m_text.length() ? m_caret + 1 : m_text.length());
					format_text();
					break;
				case 36:	// Home
					m_caret = 0;
					render::text_caret(m_lines[0], 0);
					render::text_blur(m_lines[0]);
					break;
				case 35:	// End
					m_caret = (Uint16) m_text.length();
					render::text_caret(m_lines[0], m_caret);
					render::text_blur(m_lines[0]);
					break;
				case 189:
				case 190:
					if (s_mod_alt || s_mod_ctrl) return;
					ch = (char) (2 * (vkey == 189) + 43);
					break;
				case 223:
					ch = '.';
					break;
				default:
					return;
				}
			}
			if (ch)
			{
				char	s[2] = { ch, 0 };
				insert_text(s);
			}
			m_blink_time = 0;
			if (m_lines.size() > 0) render::text_blur(m_lines[0]);
		}
		virtual void	on_char(int character)	// sub_10088A50
		{
			char	s[2] = { (char) character, 0 };
			insert_text(s);
		}
		virtual void	paste()	// sub_10089280
		{
			tu_string	text;
			if (m_def->m_multiline)
			{
				text = s_text_clipboard;
			}
			else
			{
				int	n = 0;
				while (n < s_text_clipboard.length() && s_text_clipboard[n] != 10 && s_text_clipboard[n] != 13) n++;
				text = s_text_clipboard;
				text.resize(n);
			}
			if (text.length() > 0) insert_text(text.c_str());
			if (m_lines.size() > 0) render::text_blur(m_lines[0]);
		}
		virtual void	copy()	// sub_10087BB0 (guarded by a byte at character+153 [INFERRED: password])
		{
			if (m_password) return;
			int	s = 0, e = 0;
			if (!get_selection(&s, &e)) return;
			if (s > e) { int t = s; s = e; e = t; }
			if (e > m_text.length()) e = m_text.length();
			s_text_clipboard = m_text.c_str() + s;
			s_text_clipboard.resize(e - s);
		}
		virtual void	cut()	// sub_100893C0
		{
			if (m_password) return;
			copy();
			delete_selection(true);
		}

		// ---- the AS member surface (sub_10089440 set, sub_10087C60 get) ----
		bool	dice_set_member(const tu_stringi& name, const as_value& val)
		{
			if (name == "text")
			{
				// sub_10089440: a DICE String object always goes through the wide setter with its
				// wide text, empty when it carries none (the shipped narrow arm at 0x10089599 is
				// unreachable: it needs a null std::wstring c_str()); any other value goes narrow.
				as_object_interface*	obj = val.to_object();
				if (obj)
				{
					const Uint16*	wide = obj->get_wide_text_value();
					if (wide)
					{
						set_wide_text_value(wide);
					}
					else
					{
						const char*	narrow = obj->get_text_value();
						set_text_value(narrow ? narrow : val.to_string());
					}
				}
				else
				{
					set_text_value(val.to_string());
				}
				return true;
			}
			if (name == "textColor")
			{
#if defined(__APPLE__)
				// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
				const Uint32	rgb = bf2x87::lowUInt32(val.to_number());
#else
				const Uint32	rgb = (Uint32) val.to_number();
#endif
				const Uint32	cur = ((Uint32) m_text_color.m_r << 16) | ((Uint32) m_text_color.m_g << 8) | m_text_color.m_b;
				if (rgb != cur)
				{
					m_text_color = rgba((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255);
					format_text();
				}
				return true;
			}
			if (name == "_width" || name == "_height")
			{
				const bool	is_w = name == "_width";
				const float	cur = TWIPS_TO_PIXELS(is_w ? get_width() : get_height());
				const double	v = val.to_number();
				if (v == cur) return true;
				matrix	m = get_matrix();
				float&	s = is_w ? m.m_[0][0] : m.m_[1][1];
				s = (v == 0.0 || cur == 0.0f) ? 0.0f : (float) (v / cur);
				set_matrix(m);
				format_text();
				return true;
			}
			if (name == "border")          { m_border = val.to_bool(); return true; }
#if defined(__APPLE__)
			// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
			if (name == "borderColor")     { const Uint32 c = bf2x87::lowUInt32(val.to_number()); m_border_color = rgba((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); return true; }
#else
			if (name == "borderColor")     { const Uint32 c = (Uint32) val.to_number(); m_border_color = rgba((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); return true; }
#endif
			if (name == "background")      { m_background = val.to_bool(); return true; }
#if defined(__APPLE__)
			// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
			if (name == "backgroundColor") { const Uint32 c = bf2x87::lowUInt32(val.to_number()); m_background_color = rgba((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); return true; }
#else
			if (name == "backgroundColor") { const Uint32 c = (Uint32) val.to_number(); m_background_color = rgba((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); return true; }
#endif
			if (name == "password")        { m_password = val.to_bool(); return true; }
			if (name == "tabIndex")        { set_tab_index((int) val.to_number()); return true; }
			if (name == "onChanged")       { m_on_changed = val; return true; }
			if (name == "onSetFocus")      { m_on_set_focus = val; return true; }
			if (name == "maxChars")        { m_max_chars = (int) val.to_number(); return true; }
			if (name == "restrict")
			{
				m_restrict = "";
				const char*	s = val.to_string();
				for (const char* p = s; p && *p; ++p)
				{
					if (*p != ' ' && *p != '-') m_restrict += *p;
				}
				return true;
			}
			if (name == "_focus")
			{
				if (val.to_bool())
				{
					m_blink_time = 0;
					s_focused_field = this;
					format_text();
					if (m_lines.size() > 0)
					{
						m_caret = (Uint16) m_text.length();
						render::text_caret(m_lines[0], m_caret);
					}
				}
				else if (s_focused_field == this)
				{
					s_focused_field = NULL;
				}
				return true;
			}
			if (name == "_x" || name == "_y" || name == "_visible" || name == "_alpha")
			{
				return false;	// the stock switch below handles these four the same way
			}
			// Anything else lives in the field's own environment (the shipped falls through to it).
			m_as_environment.set_member(name, val);
			return true;
		}
		bool	dice_get_member(const tu_stringi& name, as_value* val)
		{
			if (name == "text")            { val->set_tu_string(m_text); return true; }
			if (name == "textColor")
			{
				val->set_double((double) (int) (((Uint32) m_text_color.m_a << 24) | ((Uint32) m_text_color.m_r << 16) | ((Uint32) m_text_color.m_g << 8) | m_text_color.m_b));
				return true;
			}
			if (name == "_width")          { val->set_double(TWIPS_TO_PIXELS(get_width())); return true; }
			if (name == "_height")         { val->set_double(TWIPS_TO_PIXELS(get_height())); return true; }
			// [DICE-MOD #53] sub_10087C60: `textHeight` is vslot 0x10C, the laid-out height formatText
			// last requested (m_text_height_px), not the font size. The loading screen's label
			// component sets `_height = textHeight`; with the font size every multi-line field shrank
			// to one line and the next formatText stopped after the first wrap.
			if (name == "textHeight")      { val->set_double((double) m_text_height_px); return true; }
			if (name == "border")          { val->set_bool(m_border); return true; }
			if (name == "borderColor")     { val->set_double((double) (int) (((Uint32) m_border_color.m_a << 24) | ((Uint32) m_border_color.m_r << 16) | ((Uint32) m_border_color.m_g << 8) | m_border_color.m_b)); return true; }
			if (name == "background")      { val->set_bool(m_background); return true; }
			if (name == "backgroundColor") { val->set_double((double) (int) (((Uint32) m_background_color.m_a << 24) | ((Uint32) m_background_color.m_r << 16) | ((Uint32) m_background_color.m_g << 8) | m_background_color.m_b)); return true; }
			if (name == "password")        { val->set_bool(m_password); return true; }
			if (name == "tabIndex")        { if (m_tab_index < 0) return false; val->set_int(m_tab_index); return true; }
			if (name == "onChanged")       { *val = m_on_changed; return true; }
			if (name == "onSetFocus")      { *val = m_on_set_focus; return true; }
			if (name == "maxChars")        { val->set_int(m_max_chars); return true; }
			if (name == "restrict")        { val->set_string(""); return true; }
			if (name == "_focus")          { val->set_bool(s_focused_field == this); return true; }
			if (name == "_parent")
			{
				val->set_as_object_interface(get_parent() ? static_cast<as_object_interface*>(get_parent()) : NULL);
				return true;
			}
			if (name == "_x" || name == "_y" || name == "_visible" || name == "_alpha")
			{
				return false;	// the stock switch below
			}
			return m_as_environment.get_member(name, val);
		}
	};

	// [DICE-MOD #37] the focused-field surface SwiffPlayer's key/char/clipboard entries use.
	text_input*	get_focused_text_input()
	{
		return s_focused_field;
	}
	void	set_text_input_modifiers(bool shift, bool alt, bool ctrl, bool caps)
	{
		s_mod_shift = shift;
		s_mod_alt = alt;
		s_mod_ctrl = ctrl;
		s_mod_caps = caps;
	}
	tu_string&	text_input_clipboard()
	{
		return s_text_clipboard;
	}


	character*	edit_text_character_def::create_character_instance(movie* parent, int id)
	{
		if (m_font == NULL)
		{
			// Resolve the font, if possible.
			m_font = m_root_def->get_font(m_font_id);
			if (m_font == NULL)
			{
				log_error("error: text style with undefined font; font_id = %d\n", m_font_id);
			}
		}

		edit_text_character*	ch = new edit_text_character(parent, this, id);
		ch->set_name(m_default_name.c_str());
		return ch;
	}


	void	define_edit_text_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Read a DefineText tag.
	{
		assert(tag_type == 37);

		Uint16	character_id = in->read_u16();

		edit_text_character_def*	ch = new edit_text_character_def(m);
		IF_VERBOSE_PARSE(log_msg("edit_text_char, id = %d\n", character_id));
		ch->read(in, tag_type, m);

		m->add_character(character_id, ch);
	}

}	// end namespace gameswf


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:

