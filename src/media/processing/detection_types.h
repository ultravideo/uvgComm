#pragma once

#include <array>

struct Point {
    int x, y;
};

struct Rect {
    int x, y, width, height;
};

struct Detection
{
    Rect bbox;
    std::array<Point, 5> landmarks;
};
