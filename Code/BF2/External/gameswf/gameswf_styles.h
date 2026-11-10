// gameswf_styles.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Fill and line style types.


#ifndef GAMESWF_STYLES_H
#define GAMESWF_STYLES_H


#include "gameswf_impl.h"


namespace gameswf
{
	struct stream;


	struct gradient_record
	{
		gradient_record();
		void	read(stream* in, int tag_type);

	//data:
		Uint8	m_ratio;
		rgba	m_color;
	};

	struct base_fill_style
	{
		// Implicit vptr-only constructor: SwiffPlayer.dll 0x100A3D60.
		virtual void apply(int fill_side, float ratio) const = 0;
	};

	struct fill_style : public base_fill_style
	// For the interior of outline shapes.
	{
		fill_style();
		virtual ~fill_style();
		// Implicit member assignment: SwiffPlayer.dll 0x10086550 copies type,
		// color, gradient matrix/records, both held images, then bitmap matrix.

		void	read(stream* in, int tag_type, movie_definition_sub* m);
		rgba	sample_gradient(int ratio) const;
		gameswf::bitmap_info*	create_gradient_bitmap() const;
		// Regenerate the gradient and register its bitmap with the owning movie.
		void	generate_gradient_bitmap(movie_definition_sub* m);
		virtual void	apply(int fill_side, float ratio) const;

		// SwiffPlayer.dll 0x100A3CF0.
		rgba	get_color() const { return m_color; }
		void	set_color(rgba new_color) { m_color = new_color; }
		// SwiffPlayer.dll 0x100A3D00.
		int	get_type() const { return m_type; }

		// runtime-built fills. The drawing API (beginFill) needs a solid
		// style and the loadMovie image loader a tiled bitmap fill (type 0x40, the shipped loader's
		// `v33 = 64`, matrix scale 0.05); the parser-only 2006 class had no way to build either.
		// Store order in SwiffPlayer.dll 0x10097940.
		void	set_solid(rgba color) { m_color = color; m_type = 0x00; }
		void	set_bitmap(bitmap_character_def* bc, const matrix& m, bool clamp)
		{
			m_type = clamp ? 0x41 : 0x40;
			m_bitmap_character = bc;
			m_bitmap_matrix = m;
		}

		// [CHOSEN, not recovered] S3 bridge exposing the fields written by
		// SwiffPlayer.dll 0x1007C3B0 without exposing the native object layout.
		void set_gradient(int type, const matrix& m) { m_type = type; m_gradient_matrix = m; }
		void append_gradient(const gradient_record& record) { m_gradients.push_back(record); }

		// For shape morphing
		void	set_lerp(const fill_style& a, const fill_style& b, float t);

	private:
		friend struct morph2_character_def;

		int	m_type;
		rgba	m_color;
		matrix	m_gradient_matrix;
		array<gradient_record>	m_gradients;
		smart_ptr<gameswf::bitmap_info>	m_gradient_bitmap_info;
		smart_ptr<bitmap_character_def>	m_bitmap_character;
		matrix	m_bitmap_matrix;
	};


	struct morph_fill_style : public base_fill_style
	{
		morph_fill_style();
		morph_fill_style(stream* in, movie_definition_sub* m);
		virtual ~morph_fill_style();
		
		void read(stream* in, movie_definition_sub* m);
		rgba sample_gradient(int ratio, float morph);
		bitmap_info* create_gradient_bitmap(float morph) const;
		virtual void apply(int fill_side, float morph) const;
		rgba get_color(float morph) const;
		void set_colors(rgba new_color_orig, rgba new_color_target);
	private:
		int m_type;
		rgba m_color[2];
		matrix m_gradient_matrix[2];
		array<gradient_record> m_gradients[2];
		smart_ptr<bitmap_info> m_gradient_bitmap_info[2];
		smart_ptr<bitmap_character_def> m_bitmap_character;
		matrix m_bitmap_matrix[2];
	};

	struct base_line_style
	{
		// Implicit vptr-only constructor: SwiffPlayer.dll 0x100A3DA0.
		virtual void apply(float ratio) const = 0;
	};

	struct line_style : public base_line_style
	// For the outside of outline shapes, or just bare lines.
	{
		line_style();
		void	read(stream* in, int tag_type);
		virtual void	apply(float ratio) const;

		Uint16	get_width() const { return m_width; }
		// SwiffPlayer.dll 0x10096250 / 0x10096260; [INFERRED] source-level names.
		void set_width(Uint16 width) { m_width = width; }
		void set_color(const rgba& color) { m_color = color; }
		// SwiffPlayer.dll 0x1009F570; implicit member assignment at 0x1009F6F0.
		const rgba&	get_color() const { return m_color; }
		// runtime-built line styles for the drawing API (lineStyle).
		// Store order in SwiffPlayer.dll 0x10097980.
		void	set(Uint16 width_twips, const rgba& color) { m_color = color; m_width = width_twips; }

	private:
		friend struct morph2_character_def;

		Uint16	m_width;	// in TWIPS
		rgba	m_color;
	};

	struct morph_line_style : public base_line_style
	{
		morph_line_style();
		morph_line_style(stream* in);

		void read(stream* in);
		virtual void apply(float morph) const;
		
	private:
		Uint16 m_width[2];
		rgba   m_color[2];
	};
}


#endif // GAMESWF_STYLES_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
