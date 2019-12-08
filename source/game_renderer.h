#pragma once

#include "my_math.h"

struct Color
{
  float r, g, b, a;

  Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
  Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
};


void draw_cell(v2i position, Color color);
void draw_cell_in_left_bar(v2i position, Color color);
void draw_cell_in_right_bar(v2i position, Color color);


