// [TEMP] [CHOSEN, not recovered] Content-free observers of the production
// tessellator. Including the TU exposes its sorting and clipping boundaries;
// no production body is replaced. No renderer, game content or sockets.
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include "../BF2/External/gameswf/gameswf_tesselate.cpp"

namespace tess = gameswf::tesselate;
using gameswf::point;

static unsigned checks;
static void check(bool condition, const char* message)
{
	++checks;
	if (!condition)
	{
		std::fprintf(stderr, "FAIL %s\n", message);
		std::exit(1);
	}
}

struct observer : tess::trapezoid_accepter
{
	std::vector<tess::trapezoid> fills;
	std::vector<int> fill_styles;
	std::vector<point> line;
	int line_style = -1;
	unsigned line_calls = 0;

	void accept_trapezoid(int style, const tess::trapezoid& tr) override
	{
		fill_styles.push_back(style);
		fills.push_back(tr);
	}
	void accept_line_strip(int style, const point* coords, int count) override
	{
		++line_calls;
		line_style = style;
		line.assign(coords, coords + count);
	}
};

static void ordering()
{
	tess::fill_segment a(point(0, -16777216.0f), point(0, 0), 3, 4, 5);
	tess::fill_segment b(point(0, -16777216.0f), point(0, 1), 3, 4, 5);
	check(tess::compare_segment_y(&a, &b) == -1, "heights compare before float rounding");
	check(tess::compare_segment_y(&b, &a) == 1, "reverse height order");
	check(tess::compare_segment_y(&a, &a) == 0, "equal y and height");
	b.m_begin.m_x = 1;
	check(tess::compare_segment_x(&a, &b) == -1, "begin x order");
	b.m_begin.m_x = 0;
	b.m_end.m_x = 1;
	check(tess::compare_segment_x(&a, &b) == -1, "end x tie break");
	check(tess::compare_segment_x(&a, &a) == 0, "equal x endpoints");
	b.m_begin.m_x = std::numeric_limits<float>::quiet_NaN();
	check(tess::compare_segment_x(&a, &b) == 1, "unordered x returns positive");
	b.m_begin.m_y = std::numeric_limits<float>::quiet_NaN();
	check(tess::compare_segment_y(&a, &b) == 1, "unordered y returns positive");
	tess::fill_segment down(point(1, 9), point(2, 3), 3, 4, 5);
	check(down.m_begin == point(2, 3) && down.m_end == point(1, 9), "downward endpoints flip");
	check(down.m_left_style == 4 && down.m_right_style == 3 && down.m_line_style == 5,
		"flip exchanges fills and preserves line style");
}

static void paths_and_curves()
{
	observer out;
	const float tolerances[] = {0, -1, std::numeric_limits<float>::quiet_NaN()};
	for (float tolerance : tolerances)
	{
		tess::begin_shape(&out, tolerance);
		check(tess::s_tolerance == 1, "invalid tolerance defaults to one");
		tess::end_shape();
	}

	tess::begin_shape(&out, 2);
	tess::begin_path(-1, -1, 7, 16777216.0f, 0);
	tess::add_curve_segment(16777218.0f, 8, 16777218.0f, 0);
	tess::end_path();
	check(out.line_calls == 1 && out.line_style == 7, "line callback style");
	check(out.line.size() == 4, "curve subdivision retains register midpoint precision");
	check(out.line[0] == point(16777216.0f, 0) && out.line[1] == point(16777216.0f, 3)
		&& out.line[2] == point(16777218.0f, 4) && out.line[3] == point(16777218.0f, 0),
		"curve points preserve first-half then second-half order");
	tess::end_shape();
	check(out.fills.empty(), "line-only shape has no fills");

	tess::begin_shape(&out, 1);
	tess::begin_path(2, -1, -1, 0, 0);
	tess::end_path();
	tess::end_shape();
	check(out.fills.empty(), "fill style with no edges emits nothing");

	tess::begin_shape(&out, 1);
	tess::begin_path(-1, -1, 7, 0, 0);
	tess::end_path();
	check(out.line_calls == 1, "one-point path emits no line");
	tess::begin_path(-1, -1, -1, 0, 0);
	tess::add_line_segment(2, 2);
	tess::end_path();
	check(out.line_calls == 1, "negative line style emits no line");
	tess::end_shape();

	tess::begin_shape(&out, 1);
	tess::begin_path(1, -1, 1, 0, 0);
	tess::add_line_segment(1, 1);
	tess::begin_shape(&out, 1);
	check(tess::s_current_segments.size() == 0 && tess::s_current_path.size() == 0
		&& !tess::s_shape_has_fill && !tess::s_shape_has_line, "begin_shape resets unfinished work");
	tess::end_shape();

	tess::begin_shape(&out, 1);
	tess::begin_path(-1, -1, -1, 0, 0);
	tess::add_line_segment(1, 1);
	tess::end_shape();
	check(tess::s_current_path.size() == 2 && tess::s_accepter == NULL,
		"end_shape retains an unfinished path and clears the accepter");
	tess::begin_shape(&out, 1);
	tess::end_shape();
}

static void clipping()
{
	observer out;
	tess::begin_shape(&out, 1);
	tess::begin_path(2, -1, -1, 16777216.0f, 0);
	tess::add_line_segment(-16777216.0f, 3);
	tess::begin_path(-1, -1, -1, 33554432.0f, 0);
	tess::add_line_segment(33554432.0f, 3);
	tess::peel_off_and_emit(0, 2, 0, 1);
	check(out.fills.size() == 1 && out.fill_styles[0] == 2, "left fill callback");
	check(out.fills[0].m_lx1 == 5592405.5f, "interpolation quotient stays wide until intersection store");
	check(tess::s_current_segments[0].m_begin == point(5592405.5f, 1), "remaining segment starts at cut");
	tess::peel_off_and_emit(0, 2, 1, 1);
	check(out.fills.size() == 1, "zero-height slab emits nothing");
	// Finish this deliberately partial shape without re-emitting its upper interval.
	tess::s_current_segments.resize(0);
	tess::s_shape_has_fill = false;
	tess::end_shape();

	for (int side = 0; side < 2; ++side)
	{
		observer box;
		tess::begin_shape(&box, 1);
		// Two vertical edges start together; a third event at y=1 cuts the first slab.
		tess::begin_path(side ? -1 : 3, side ? 4 : -1, -1, 0, 0);
		tess::add_line_segment(0, 3);
		tess::begin_path(-1, -1, -1, 2, 0);
		tess::add_line_segment(2, 3);
		tess::begin_path(-1, -1, -1, 4, 1);
		tess::add_line_segment(4, 2);
		tess::end_path();
		tess::end_shape();
		check(box.fills.size() == 3, "new y event and shorter edge split active slab");
		for (unsigned i = 0; i < box.fills.size(); ++i)
		{
			const tess::trapezoid& tr = box.fills[i];
			check(box.fill_styles[i] == (side ? 4 : 3), "slab-wide fill sense");
			check(tr.m_y0 == float(i) && tr.m_y1 == float(i + 1)
				&& tr.m_lx0 == 0 && tr.m_lx1 == 0 && tr.m_rx0 == 2 && tr.m_rx1 == 2,
				"trapezoid bounds and output order");
		}
	}
}

int main()
{
	ordering();
	paths_and_curves();
	clipping();
	std::printf("PASS %u tessellator checks\n", checks);
}
