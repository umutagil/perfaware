#pragma once

#include "basedef.h"

const f64 EARTH_RADIUS = 6372.8;

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


static f64 Square(f64 A)
{
    f64 Result = (A*A);
    return Result;
}

static f64 RadiansFromDegrees(f64 Degrees)
{
    f64 Result = 0.01745329251994329577 * Degrees;
    return Result;
}

f64 ReferenceHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1, f64 EarthRadius);