// gameswf_types.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some basic types for gameswf.

#include "../../Platform/X87Compat.h"
#include "gameswf_types.h"

#include "gameswf_log.h"
#include "gameswf_stream.h"
#include "gameswf_render.h"
#include "gameswf.h"
#include <string.h>


namespace gameswf
{

	//
	// point
	//


	// SwiffPlayer.dll 0x1009D800.
	bool	point::bitwise_equal(const point& p) const
	// Bitwise comparison; return true if *this is bitwise
	// identical to p.
	{
		return memcmp(this, &p, sizeof(p)) == 0;
	}


	//
	// matrix
	//

	matrix	matrix::identity;

	// SwiffPlayer.dll 0x1009E5F0.
	matrix::matrix()
	{
		// Default to identity.
		set_identity();
	}


	bool	matrix::is_valid() const
	{
		return isfinite(m_[0][0])
			&& isfinite(m_[0][1])
			&& isfinite(m_[0][2])
			&& isfinite(m_[1][0])
			&& isfinite(m_[1][1])
			&& isfinite(m_[1][2]);
	}


	// SwiffPlayer.dll 0x1009D820.
	void	matrix::set_identity()
	// Set the matrix to identity.
	{
		memset(&m_[0], 0, sizeof(m_));
		m_[0][0] = 1;
		m_[1][1] = 1;
	}


	// SwiffPlayer.dll 0x1009E620: six float temporaries before copying the result.
	void	matrix::concatenate(const matrix& m)
	// Concatenate m's transform onto ours.  When
	// transforming points, m happens first, then our
	// original xform.
	{
		using bf2x87::Real;
		using bf2x87::mul;
		using bf2x87::add;
		using bf2x87::store;
		matrix	t;
		t.m_[0][0] = store(add(mul(Real(m.m_[0][0]), Real(m_[0][0])), mul(Real(m_[0][1]), Real(m.m_[1][0]))));
		t.m_[1][0] = store(add(mul(Real(m_[1][0]), Real(m.m_[0][0])), mul(Real(m_[1][1]), Real(m.m_[1][0]))));
		t.m_[0][1] = store(add(mul(Real(m.m_[0][1]), Real(m_[0][0])), mul(Real(m.m_[1][1]), Real(m_[0][1]))));
		t.m_[1][1] = store(add(mul(Real(m.m_[1][1]), Real(m_[1][1])), mul(Real(m.m_[0][1]), Real(m_[1][0]))));
		t.m_[0][2] = store(add(add(mul(Real(m.m_[0][2]), Real(m_[0][0])), mul(Real(m.m_[1][2]), Real(m_[0][1]))), Real(m_[0][2])));
		t.m_[1][2] = store(add(add(mul(Real(m.m_[1][2]), Real(m_[1][1])), mul(Real(m.m_[0][2]), Real(m_[1][0]))), Real(m_[1][2])));

		*this = t;
	}


	// SwiffPlayer.dll 0x1009D850.
	void	matrix::concatenate_translation(float tx, float ty)
	// Concatenate a translation onto the front of our
	// matrix.  When transforming points, the translation
	// happens first, then our original xform.
	{
		m_[0][2] += m_[0][0] * tx + m_[0][1] * ty;
		m_[1][2] += m_[1][0] * tx + m_[1][1] * ty;
	}


	// SwiffPlayer.dll 0x1009D880.
	void	matrix::concatenate_scale(float scale)
	// Concatenate a uniform scale onto the front of our
	// matrix.  When transforming points, the scale
	// happens first, then our original xform.
	{
		m_[0][0] *= scale;
		m_[0][1] *= scale;
		m_[1][0] *= scale;
		m_[1][1] *= scale;
	}


	// SwiffPlayer.dll 0x1009D8B0.
	void	matrix::set_lerp(const matrix& m1, const matrix& m2, float t)
	// Set this matrix to a blend of m1 and m2, parameterized by t.
	{
		m_[0][0] = flerp(m1.m_[0][0], m2.m_[0][0], t);
		m_[1][0] = flerp(m1.m_[1][0], m2.m_[1][0], t);
		m_[0][1] = flerp(m1.m_[0][1], m2.m_[0][1], t);
		m_[1][1] = flerp(m1.m_[1][1], m2.m_[1][1], t);
		m_[0][2] = flerp(m1.m_[0][2], m2.m_[0][2], t);
		m_[1][2] = flerp(m1.m_[1][2], m2.m_[1][2], t);
	}

	
	// SwiffPlayer.dll 0x1009D930.
	void	matrix::set_scale_rotation(float x_scale, float y_scale, float angle)
	// Set the scale & rotation part of the matrix.
	// angle in radians.
	{
		float	cos_angle = cosf(angle);
		float	sin_angle = sinf(angle);
		m_[0][0] = x_scale * cos_angle;
		m_[0][1] = y_scale * -sin_angle;
		m_[1][0] = x_scale * sin_angle;
		m_[1][1] = y_scale * cos_angle;
	}


	// SwiffPlayer.dll 0x1009D960.
	void	matrix::read(stream* in)
	// Initialize from the stream.
	{
		in->align();

		set_identity();

		int	has_scale = in->read_uint(1);
		if (has_scale)
		{
			int	scale_nbits = in->read_uint(5);
			m_[0][0] = in->read_sint(scale_nbits) / 65536.0f;
			m_[1][1] = in->read_sint(scale_nbits) / 65536.0f;
		}
		int	has_rotate = in->read_uint(1);
		if (has_rotate)
		{
			int	rotate_nbits = in->read_uint(5);
			m_[1][0] = in->read_sint(rotate_nbits) / 65536.0f;
			m_[0][1] = in->read_sint(rotate_nbits) / 65536.0f;
		}

		int	translate_nbits = in->read_uint(5);
		if (translate_nbits > 0)
		{
			m_[0][2] = (float) in->read_sint(translate_nbits);
			m_[1][2] = (float) in->read_sint(translate_nbits);
		}

		IF_VERBOSE_PARSE(log_msg("has_scale = %d, has_rotate = %d\n", has_scale, has_rotate));
	}


	// SwiffPlayer.dll 0x1009DA80.
	void	matrix::print() const
	// Debug log.
	{
		log_msg("| %4.4f %4.4f %4.4f |\n", m_[0][0], m_[0][1], TWIPS_TO_PIXELS(m_[0][2]));
		log_msg("| %4.4f %4.4f %4.4f |\n", m_[1][0], m_[1][1], TWIPS_TO_PIXELS(m_[1][2]));
	}

	// SwiffPlayer.dll 0x1009DAE0.
	void	matrix::transform(point* result, const point& p) const
	// Transform point 'p' by our matrix.  Put the result in
	// *result.
	{
		assert(result);

		result->m_x = m_[0][0] * p.m_x + m_[0][1] * p.m_y + m_[0][2];
		result->m_y = m_[1][0] * p.m_x + m_[1][1] * p.m_y + m_[1][2];
	}
	
	// SwiffPlayer.dll 0x1009DB20.
	void	matrix::transform_vector(point* result, const point& v) const
	// Transform vector 'v' by our matrix. Doesn't apply translation.
	// Put the result in *result.
	{
		assert(result);

		result->m_x = m_[0][0] * v.m_x + m_[0][1] * v.m_y;
		result->m_y = m_[1][0] * v.m_x + m_[1][1] * v.m_y;
	}

	// SwiffPlayer.dll 0x1009E6D0: build the inverse before reading the point;
	// each coordinate narrows once, and aliased input sees the first output store.
	void	matrix::transform_by_inverse(point* result, const point& p) const
	// Transform point 'p' by the inverse of our matrix.  Put result in *result.
	{
		// @@ TODO optimize this!
		matrix	m;
		m.set_inverse(*this);
		using bf2x87::Real;
		using bf2x87::mul;
		using bf2x87::add;
		result->m_x = bf2x87::store(add(add(mul(Real(m.m_[0][0]), Real(p.m_x)),
			mul(Real(m.m_[0][1]), Real(p.m_y))), Real(m.m_[0][2])));
		result->m_y = bf2x87::store(add(add(mul(Real(m.m_[1][0]), Real(p.m_x)),
			mul(Real(m.m_[1][1]), Real(p.m_y))), Real(m.m_[1][2])));
	}


	// SwiffPlayer.dll 0x1009DB50.
	void	matrix::set_inverse(const matrix& m)
	// Set this matrix to the inverse of the given matrix.
	{
		assert(this != &m);

		// Invert the rotation part.
		float	det = m.m_[0][0] * m.m_[1][1] - m.m_[0][1] * m.m_[1][0];
		if (det == 0.0f)
		{
			// Not invertible.
			//assert(0);	// castano: this happens sometimes! (ie. sample6.swf)

			// Arbitrary fallback.
			set_identity();
			m_[0][2] = -m.m_[0][2];
			m_[1][2] = -m.m_[1][2];
		}
		else
		{
			float	inv_det = 1.0f / det;
			m_[0][0] = m.m_[1][1] * inv_det;
			m_[1][1] = m.m_[0][0] * inv_det;
			m_[0][1] = -m.m_[0][1] * inv_det;
			m_[1][0] = -m.m_[1][0] * inv_det;

			m_[0][2] = -(m_[0][0] * m.m_[0][2] + m_[0][1] * m.m_[1][2]);
			m_[1][2] = -(m_[1][0] * m.m_[0][2] + m_[1][1] * m.m_[1][2]);
		}
	}


	// SwiffPlayer.dll 0x1009DC00.
	bool	matrix::does_flip() const
	// Return true if this matrix reverses handedness.
	{
		float	det = m_[0][0] * m_[1][1] - m_[0][1] * m_[1][0];

		return det < 0.f;
	}


	// SwiffPlayer.dll 0x1009DC30.
	float	matrix::get_determinant() const
	// Return the determinant of the 2x2 rotation/scale part only.
	{
		return m_[0][0] * m_[1][1] - m_[1][0] * m_[0][1];
	}


	// SwiffPlayer.dll 0x1009DC40.
	float	matrix::get_max_scale() const
	// Return the maximum scale factor that this transform
	// applies.  For assessing scale, when determining acceptable
	// errors in tesselation.
	{
		// @@ not 100% sure what the heck I'm doing here.  I
		// think this is roughly what I want; take the max
		// length of the two basis vectors.
		float	basis0_length2 = m_[0][0] * m_[0][0] + m_[0][1] * m_[0][1];
		float	basis1_length2 = m_[1][0] * m_[1][0] + m_[1][1] * m_[1][1];
		float	max_length2 = tu_util::fmax(basis0_length2, basis1_length2);
		return sqrtf(max_length2);
	}

	// SwiffPlayer.dll 0x1009DC90.
	float	matrix::get_x_scale() const
	{
		float	scale = sqrtf(m_[0][0] * m_[0][0] + m_[0][1] * m_[0][1]);

		// Are we turned inside out?
		if (get_determinant() < 0.f)
		{
			scale = -scale;
		}

		return scale;
	}

	// SwiffPlayer.dll 0x1009DCD0.
	float	matrix::get_y_scale() const
	{
		return sqrtf(m_[1][1] * m_[1][1] + m_[1][0] * m_[1][0]);
	}

	// SwiffPlayer.dll 0x1009DCF0.
	float	matrix::get_rotation() const
	{
		if (get_determinant() < 0.f)
		{
			// We're turned inside out; negate the
			// x-component used to compute rotation.
			//
			// Matches get_x_scale().
			//
			// @@ this may not be how Macromedia does it!  Test this!
			return atan2f(m_[1][0], -m_[0][0]);
		}
		else
		{
			return atan2f(m_[1][0], m_[0][0]);
		}
	}


	//
	// cxform
	//


	cxform	cxform::identity;


	// SwiffPlayer.dll 0x1009DD20.
	cxform::cxform()
	// Initialize to identity transform.
	{
		m_[0][0] = 1;
		m_[1][0] = 1;
		m_[2][0] = 1;
		m_[3][0] = 1;
		m_[0][1] = 0;
		m_[1][1] = 0;
		m_[2][1] = 0;
		m_[3][1] = 0;
	}

	// SwiffPlayer.dll 0x1009DD50.
	void	cxform::concatenate(const cxform& c)
	// Concatenate c's transform onto ours.  When
	// transforming colors, c's transform is applied
	// first, then ours.
	{
		m_[0][1] += m_[0][0] * c.m_[0][1];
		m_[1][1] += m_[1][0] * c.m_[1][1];
		m_[2][1] += m_[2][0] * c.m_[2][1];
		m_[3][1] += m_[3][0] * c.m_[3][1];

		m_[0][0] *= c.m_[0][0];
		m_[1][0] *= c.m_[1][0];
		m_[2][0] *= c.m_[2][0];
		m_[3][0] *= c.m_[3][0];
	}

	
	// SwiffPlayer.dll 0x1009DDB0.
	rgba	cxform::transform(const rgba in) const
	// Apply our transform to the given color; return the result.
	{
		rgba	result;

		result.m_r = (Uint8) fclamp(in.m_r * m_[0][0] + m_[0][1], 0, 255);
		result.m_g = (Uint8) fclamp(in.m_g * m_[1][0] + m_[1][1], 0, 255);
		result.m_b = (Uint8) fclamp(in.m_b * m_[2][0] + m_[2][1], 0, 255);
		result.m_a = (Uint8) fclamp(in.m_a * m_[3][0] + m_[3][1], 0, 255);

		return result;
	}


	// SwiffPlayer.dll 0x1009DF10; the additive alpha term is one when present.
	void	cxform::read_rgb(stream* in)
	{
		in->align();

		int	has_add = in->read_uint(1);
		int	has_mult = in->read_uint(1);
		int	nbits = in->read_uint(4);

		if (has_mult) {
			m_[0][0] = in->read_sint(nbits) / 255.0f;
			m_[1][0] = in->read_sint(nbits) / 255.0f;
			m_[2][0] = in->read_sint(nbits) / 255.0f;
			m_[3][0] = 1;
		}
		else {
			for (int i = 0; i < 4; i++) { m_[i][0] = 1; }
		}
		if (has_add) {
			m_[0][1] = (float) in->read_sint(nbits);
			m_[1][1] = (float) in->read_sint(nbits);
			m_[2][1] = (float) in->read_sint(nbits);
			m_[3][1] = 1;
		}
		else {
			for (int i = 0; i < 4; i++) { m_[i][1] = 0; }
		}
	}

	// SwiffPlayer.dll 0x1009E010.
	void	cxform::read_rgba(stream* in)
	{
		in->align();

		int	has_add = in->read_uint(1);
		int	has_mult = in->read_uint(1);
		int	nbits = in->read_uint(4);

		if (has_mult) {
			m_[0][0] = in->read_sint(nbits) / 255.0f;
			m_[1][0] = in->read_sint(nbits) / 255.0f;
			m_[2][0] = in->read_sint(nbits) / 255.0f;
			m_[3][0] = in->read_sint(nbits) / 255.0f;
		}
		else {
			for (int i = 0; i < 4; i++) { m_[i][0] = 1; }
		}
		if (has_add) {
			m_[0][1] = (float) in->read_sint(nbits);
			m_[1][1] = (float) in->read_sint(nbits);
			m_[2][1] = (float) in->read_sint(nbits);
			m_[3][1] = (float) in->read_sint(nbits);
		}
		else {
			for (int i = 0; i < 4; i++) { m_[i][1] = 0; }
		}
	}

	void cxform::clamp()
	// Force component values to be in legal range.
	{
		m_[0][0] = fclamp(m_[0][0], 0, 1);
		m_[1][0] = fclamp(m_[1][0], 0, 1);
		m_[2][0] = fclamp(m_[2][0], 0, 1);
		m_[3][0] = fclamp(m_[3][0], 0, 1);

		m_[0][1] = fclamp(m_[0][1], -255.0f, 255.0f);
		m_[1][1] = fclamp(m_[1][1], -255.0f, 255.0f);
		m_[2][1] = fclamp(m_[2][1], -255.0f, 255.0f);
		m_[3][1] = fclamp(m_[3][1], -255.0f, 255.0f);
	}


	void	cxform::print() const
	// Debug log.
	{
		log_msg("    *         +\n");
		log_msg("| %4.4f %4.4f|\n", m_[0][0], m_[0][1]);
		log_msg("| %4.4f %4.4f|\n", m_[1][0], m_[1][1]);
		log_msg("| %4.4f %4.4f|\n", m_[2][0], m_[2][1]);
		log_msg("| %4.4f %4.4f|\n", m_[3][0], m_[3][1]);
	}


	//
	// rgba
	//

	// SwiffPlayer.dll 0x1009E7B0.
	void	rgba::read(stream* in, int tag_type)
	{
		if (tag_type <= 22)
		{
			read_rgb(in);
		}
		else
		{
			read_rgba(in);
		}
	}

	// SwiffPlayer.dll 0x1009E740: RGB (including opaque alpha), then the alpha byte.
	void	rgba::read_rgba(stream* in)
	{
		read_rgb(in);
		m_a = in->read_u8();
	}

	// SwiffPlayer.dll 0x1009E130.
	void	rgba::read_rgb(stream* in)
	{
		m_r = in->read_u8();
		m_g = in->read_u8();
		m_b = in->read_u8();
		m_a = 0x0FF;
	}

	// SwiffPlayer.dll 0x1009E170.
	void	rgba::print()
	// For debugging.
	{
		log_msg("rgba: %d %d %d %d\n", m_r, m_g, m_b, m_a);
	}

	
	// SwiffPlayer.dll 0x1009E1A0: interpolate, add 0.5, truncate and keep the low byte.
	void	rgba::set_lerp(const rgba& a, const rgba& b, float f)
	{
		m_r = (Uint8) bf2x87::toInt64Truncate(a.m_r + ((long double) b.m_r - a.m_r) * f + 0.5L);
		m_g = (Uint8) bf2x87::toInt64Truncate(a.m_g + ((long double) b.m_g - a.m_g) * f + 0.5L);
		m_b = (Uint8) bf2x87::toInt64Truncate(a.m_b + ((long double) b.m_b - a.m_b) * f + 0.5L);
		m_a = (Uint8) bf2x87::toInt64Truncate(a.m_a + ((long double) b.m_a - a.m_a) * f + 0.5L);
	}


	//
	// rect
	//


	// SwiffPlayer.dll 0x1009E260.
	void	rect::read(stream* in)
	{
		in->align();
		int	nbits = in->read_uint(5);
		m_x_min = (float) in->read_sint(nbits);
		m_x_max = (float) in->read_sint(nbits);
		m_y_min = (float) in->read_sint(nbits);
		m_y_max = (float) in->read_sint(nbits);

//		IF_DEBUG(log_msg("rect::read() nbits = %d\n", nbits));
	}

	// SwiffPlayer.dll 0x1009E2D0.
	void	rect::print() const
	// Debug spew.
	{
		log_msg("xmin = %g, ymin = %g, xmax = %g, ymax = %g\n",
			TWIPS_TO_PIXELS(m_x_min),
			TWIPS_TO_PIXELS(m_y_min),
			TWIPS_TO_PIXELS(m_x_max),
			TWIPS_TO_PIXELS(m_y_max));
	}

	
	// SwiffPlayer.dll 0x1009E320: inclusive edges; unordered comparisons pass.
	bool	rect::point_test(float x, float y) const
	// Return true if the specified point is inside this rect.
	{
		if (x < m_x_min
		    || x > m_x_max
		    || y < m_y_min
		    || y > m_y_max)
		{
			return false;
		}
		else
		{
			return true;
		}
	}


	// SwiffPlayer.dll 0x1009E370: minima select the input on equality/unordered;
	// maxima keep their existing value on equality/unordered.
	void	rect::expand_to_point(float x, float y)
	// Expand this rectangle to enclose the given point.
	{
		m_x_min = tu_util::fmin(m_x_min, x);
		m_y_min = tu_util::fmin(m_y_min, y);
		m_x_max = m_x_max < x ? x : m_x_max;
		m_y_max = m_y_max < y ? y : m_y_max;
	}

	// SwiffPlayer.dll 0x1009E780.
	void rect::expand_to_rect(const rect& r)
	{
		expand_to_point(r.m_x_min, r.m_y_min);
		expand_to_point(r.m_x_max, r.m_y_max);
	}

	// SwiffPlayer.dll 0x1009E410; the maximum corner is stored first.
	void rect::transform(const matrix& m)
	{
		const long double x0 = (long double)m.m_[0][0] * m_x_min
			+ (long double)m.m_[0][1] * m_y_min + m.m_[0][2];
		const long double y0 = (long double)m.m_[1][0] * m_x_min
			+ (long double)m.m_[1][1] * m_y_min + m.m_[1][2];
		const long double x1 = (long double)m.m_[0][0] * m_x_max
			+ (long double)m.m_[0][1] * m_y_max + m.m_[0][2];
		const long double y1 = (long double)m.m_[1][0] * m_x_max
			+ (long double)m.m_[1][1] * m_y_max + m.m_[1][2];
		m_x_max = (float)x1;
		m_y_max = (float)y1;
		m_x_min = (float)x0;
		m_y_min = (float)y0;
	}


	// SwiffPlayer.dll 0x1009E3D0.
	point	rect::get_corner(int i) const
	// Get one of the rect verts.
	{
		assert(i >= 0 && i < 4);
		return point(
			(i == 0 || i == 3) ? m_x_min : m_x_max,
			(i < 2) ? m_y_min : m_y_max);
	}


	// SwiffPlayer.dll 0x1009E490: transform all four corners before writing the result.
	void	rect::enclose_transformed_rect(const matrix& m, const rect& r)
	// Set ourself to bound a rectangle that has been transformed
	// by m.  This is an axial bound of an oriented (and/or
	// sheared, scaled, etc) box.
	{
		// Get the transformed bounding box.
		point	p0, p1, p2, p3;
		m.transform(&p0, r.get_corner(0));
		m.transform(&p1, r.get_corner(1));
		m.transform(&p2, r.get_corner(2));
		m.transform(&p3, r.get_corner(3));

		m_x_min = m_x_max = p0.m_x;
		m_y_min = m_y_max = p0.m_y;
		expand_to_point(p1.m_x, p1.m_y);
		expand_to_point(p2.m_x, p2.m_y);
		expand_to_point(p3.m_x, p3.m_y);
	}


	// SwiffPlayer.dll 0x1009E580; coordinate write order is xmin, ymin, xmax, ymax.
	void	rect::set_lerp(const rect& a, const rect& b, float t)
	// Set this to the lerp of a and b.
	{
		m_x_min = (float)(a.m_x_min + ((long double)b.m_x_min - a.m_x_min) * t);
		m_y_min = (float)(a.m_y_min + ((long double)b.m_y_min - a.m_y_min) * t);
		m_x_max = (float)(a.m_x_max + ((long double)b.m_x_max - a.m_x_max) * t);
		m_y_max = (float)(a.m_y_max + ((long double)b.m_y_max - a.m_y_max) * t);
	}


};	// end namespace gameswf


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
