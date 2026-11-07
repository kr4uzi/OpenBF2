// gameswf_render.cpp	-- Willem Kokke <willem@mindparity.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Interface to back-end renderer.


#include "gameswf_render.h"
#include "gameswf_log.h"


namespace gameswf 
{
	static render_handler* s_render_handler;

	void set_render_handler(render_handler* r)
	{
		s_render_handler = r;
	}

	render_handler* get_render_handler()
	{
		return s_render_handler;
	}


	namespace render
	{
		struct bogus_bi : public bitmap_info
		{
		};


		bitmap_info*	create_bitmap_info_empty()
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_empty();
			else return new bogus_bi;
		}

		bitmap_info*	create_bitmap_info_alpha(int w, int h, unsigned char* data)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_alpha(w, h, data);
			else return new bogus_bi;
		}

		bitmap_info*	create_bitmap_info_rgb(image::rgb* im)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_rgb(im);
			else return new bogus_bi;
		}

		bitmap_info*	create_bitmap_info_rgba(image::rgba* im)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_rgba(im);
			else return new bogus_bi;
		}

		// [DICE-MOD SwiffPlayer: the by-name loader (sub_1009A620 - handler slot, else a bogus
		// 32-byte bitmap_info). Unlike the others a NULL from the handler stays NULL: the loader
		// callers treat it as "no image".]
		bitmap_info*	create_bitmap_info_file(const char* path)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_file(path);
			else return new bogus_bi;
		}

		void	delete_bitmap_info(bitmap_info* bi)
		{
			if (s_render_handler) s_render_handler->delete_bitmap_info(bi);
		}

		// Bracket the displaying of a frame from a movie.
		// Fill the background color, and set up default
		// transforms, etc.
		void	begin_display(
			rgba background_color,
			int viewport_x0, int viewport_y0,
			int viewport_width, int viewport_height,
			float x0, float x1, float y0, float y1)
		{
			if (s_render_handler)
			{
				s_render_handler->begin_display(
					background_color, viewport_x0, viewport_y0,
					viewport_width, viewport_height,
					x0, x1, y0, y1);
			}
// 			else
// 			{
// 				log_error("begin_display called, but no render_handler was registered by the app!\n");
// 			}
		}


		void	end_display()
		{
			if (s_render_handler) s_render_handler->end_display();
		}


		// Geometric and color transforms for mesh and line_strip rendering.
		void	set_matrix(const matrix& m)
		{
			if (s_render_handler) s_render_handler->set_matrix(m);
		}
		void	set_cxform(const cxform& cx)
		{
			if (s_render_handler) s_render_handler->set_cxform(cx);
		}

		// Draw triangles using the current fill-style 0.
		// Clears the style list after rendering.
		//
		// coords is a list of (x,y) coordinate pairs, in
		// triangle-strip order.  The type of the array should
		// be float[vertex_count*2]
		void	draw_mesh_strip(const Sint16 coords[], int vertex_count)
		{
			if (s_render_handler) s_render_handler->draw_mesh_strip(coords, vertex_count);
		}

		// Draw a line-strip using the current line style.
		// Clear the style list after rendering.
		//
		// Coords is a list of (x,y) coordinate pairs, in
		// sequence.
		void	draw_line_strip(const Sint16 coords[], int vertex_count)
		{
			if (s_render_handler) s_render_handler->draw_line_strip(coords, vertex_count);
		}

// 		// Set line and fill styles for mesh & line_strip
// 		// rendering.
// 		enum bitmap_wrap_mode
// 		{
// 			WRAP_REPEAT,
// 			WRAP_CLAMP
// 		};

		void	fill_style_disable(int fill_side)
		{
			if (s_render_handler) s_render_handler->fill_style_disable(fill_side);
		}

		void	fill_style_color(int fill_side, rgba color)
		{
			if (s_render_handler) s_render_handler->fill_style_color(fill_side, color);
		}

		void	fill_style_bitmap(int fill_side, const bitmap_info* bi, const matrix& m, render_handler::bitmap_wrap_mode wm)
		{
			if (s_render_handler) s_render_handler->fill_style_bitmap(fill_side, bi, m, wm);
		}

		void	line_style_disable()
		{
			if (s_render_handler) s_render_handler->line_style_disable();
		}

		void	line_style_color(rgba color)
		{
			if (s_render_handler) s_render_handler->line_style_color(color);
		}

		void	line_style_width(float width)
		{
			if (s_render_handler) s_render_handler->line_style_width(width);
		}

		void	begin_submit_mask()
		{
			if (s_render_handler) s_render_handler->begin_submit_mask();
		}

		void	end_submit_mask()
		{
			if (s_render_handler) s_render_handler->end_submit_mask();
		}

		void	disable_mask()
		{
			if (s_render_handler) s_render_handler->disable_mask();
		}

		// [DICE-MOD SwiffPlayer #36] the text-line slots; each shipped forwarder guards on the handler
		// exactly like these (the profiler wrappers around setText/drawText are not reproduced).
		void*	text_create(const char* font_name, int height_px)
		{
			return s_render_handler ? s_render_handler->text_create(font_name, height_px) : NULL;
		}
		void	text_release(void* line)
		{
			if (s_render_handler) s_render_handler->text_release(line);
		}
		void	text_set(void* line, const wchar_t* text)
		{
			if (s_render_handler) s_render_handler->text_set(line, text);
		}
		void	text_detach(void* line)
		{
			if (s_render_handler) s_render_handler->text_detach(line);
		}
		void	text_draw(void* line, const matrix* m, unsigned color, int flags)
		{
			if (s_render_handler) s_render_handler->text_draw(line, m, color, flags);
		}
		bool	text_extent(void* line, int count, float* w, float* h)
		{
			return s_render_handler ? s_render_handler->text_extent(line, count, w, h) : false;
		}
		// 0x1009A8E0: ecx = line, edx = out_pos, the two float dwords passed on to handler +0x6C.
		bool	text_hit(void* line, int* out_pos, float x, float y)
		{
			return s_render_handler ? s_render_handler->text_hit(line, out_pos, x, y) : false;
		}
		bool	text_selection(void* line, int* a, int* b)
		{
			// [DICE-MOD SwiffPlayer #46] the handler's Boolean (sub_1009A910 returns the
			// render_handler +112 answer) says whether a selection exists at all.
			if (s_render_handler) return s_render_handler->text_selection(line, a, b);
			if (a) *a = 0;
			if (b) *b = 0;
			return false;
		}
		// 0x1009A940: the four float dwords passed on to handler +0x74.
		bool	text_set_selection(void* line, float x1, float y1, float x2, float y2)
		{
			return s_render_handler ? s_render_handler->text_set_selection(line, x1, y1, x2, y2) : false;
		}
		void	text_blur(void* line)
		{
			if (s_render_handler) s_render_handler->text_blur(line);
		}
		void	text_set_size(void* line, float size, int flag)
		{
			if (s_render_handler) s_render_handler->text_set_size(line, size, flag);
		}
		void	text_caret(void* line, int pos)
		{
			if (s_render_handler) s_render_handler->text_caret(line, pos);
		}

		// Special function to draw a rectangular bitmap;
		// intended for textured glyph rendering.  Ignores
		// current transforms.
		void	draw_bitmap(const matrix& m, const bitmap_info* bi, const rect& coords, const rect& uv_coords, rgba color)
		{
			if (s_render_handler)
			{
				s_render_handler->draw_bitmap(
					m, bi, coords, uv_coords, color);
			}
		}
	}
}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
