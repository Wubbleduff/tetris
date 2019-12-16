
////////////////////////////////////////////////////////////////////////////////
// This file is meant to fork all gameplay "drawing" calls to every system that
// needs to know about them.
////////////////////////////////////////////////////////////////////////////////

#include "game_presentation.h"

#include "renderer.h"

void draw_cell(v2i position, Color color)
{
  renderer_add_cell(position, color);

  network_add_cell(position, color);
}

void draw_cell_in_left_bar(v2i position, Color color)
{
  renderer_add_cell_in_left_bar(position, color);
}

void draw_cell_in_right_bar(v2i position, Color color)
{
  renderer_add_cell_in_right_bar(position, color);
}


