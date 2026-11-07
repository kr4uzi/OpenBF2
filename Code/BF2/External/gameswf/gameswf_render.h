// gameswf_render.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Interface to renderer back-end.


#ifndef GAMESWF_RENDER_H
#define GAMESWF_RENDER_H


#include "gameswf_types.h"
#include "gameswf.h"
#include "base/image.h"


namespace gameswf
{
	render_handler*	get_render_handler();

	namespace render
	{
		bitmap_info*	create_bitmap_info_empty();
		bitmap_info*	create_bitmap_info_alpha(int w, int h, unsigned char* data);
		bitmap_info*	create_bitmap_info_rgb(image::rgb* im);
		bitmap_info*	create_bitmap_info_rgba(image::rgba* im);
		bitmap_info*	create_bitmap_info_file(const char* path);	// [DICE-MOD SwiffPlayer: see gameswf.h]
		void	delete_bitmap_info(bitmap_info* bi);

		// Bracket the displaying of a frame from a movie.
		// Fill the background color, and set up default
		// transforms, etc.
		void	begin_display(
			rgba background_color,
			int viewport_x0, int viewport_y0,
			int viewport_width, int viewport_height,
			float x0, float x1, float y0, float y1);
		void	end_display();

		// Geometric and color transforms for mesh and line_strip rendering.
		void	set_matrix(const matrix& m);
		void	set_cxform(const cxform& cx);

		// Draw triangles using the current fill-style 0.
		// Clears the style list after rendering.
		//
		// coords is a list of (x,y) coordinate pairs, in
		// triangle-strip order.  The type of the array should
		// be float[vertex_count*2]
		void	draw_mesh_strip(const Sint16 coords[], int vertex_count);

		// Draw a line-strip using the current line style.
		// Clear the style list after rendering.
		//
		// Coords is a list of (x,y) coordinate pairs, in
		// sequence.
		void	draw_line_strip(const Sint16 coords[], int vertex_count);

		void	fill_style_disable(int fill_side);
		void	fill_style_color(int fill_side, rgba color);
		void	fill_style_bitmap(int fill_side, const bitmap_info* bi, const matrix& m, render_handler::bitmap_wrap_mode wm);

		void	line_style_disable();
		void	line_style_color(rgba color);
		void	line_style_width(float width);

		void	begin_submit_mask();
		void	end_submit_mask();
		void	disable_mask();

		// [DICE-MOD SwiffPlayer #36] the text-line slots (shipped render:: forwarders 0x1009A850..
		// 0x1009AE90), see render_handler in gameswf.h.
		void*	text_create(const char* font_name, int height_px);
		void	text_release(void* line);
		void	text_set(void* line, const wchar_t* text);
		void	text_detach(void* line);
		void	text_draw(void* line, const matrix* m, unsigned color, int flags);
		bool	text_extent(void* line, int count, float* w, float* h);
		// [DICE-MOD SwiffPlayer] float coordinates: SwiffPlayer.dll 0x100887A5..0x100887D7 / 0x10088823..0x1008883B
		bool	text_hit(void* line, int* out_pos, float x, float y);
		bool	text_selection(void* line, int* a, int* b);	// [DICE-MOD SwiffPlayer #46] true = a selection exists
		bool	text_set_selection(void* line, float x1, float y1, float x2, float y2);
		void	text_blur(void* line);
		void	text_set_size(void* line, float size, int flag);
		void	text_caret(void* line, int pos);

		// Special function to draw a rectangular bitmap;
		// intended for textured glyph rendering.  Ignores
		// current transforms.
		void	draw_bitmap(const matrix& m, const bitmap_info* bi, const rect& coords, const rect& uv_coords, rgba color);

	};	// end namespace render
};	// end namespace gameswf


#endif // GAMESWF_RENDER_H

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
