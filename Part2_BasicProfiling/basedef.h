#pragma once

using u8 = char unsigned;
using u16 = short unsigned;
using u32 = int unsigned;
using u64 = long long unsigned;

using s8 = char;
using s16 = short;
using s32 = int;
using s64 = long long;

using b32 = s32;

using f32 = float;
using f64 = double;

struct Point
{
	f64 x = 0;
	f64 y = 0;
};

struct HaversinePair
{
	Point p0;
	Point p1;

	HaversinePair() = default;

	HaversinePair(const f64 x0, const f64 y0, const f64 x1, const f64 y1)
		: p0{ x0, y0 }
		, p1{ x1, y1 }
	{
	}
};

#define PROFILER 1
