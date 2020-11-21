

#include "my_math.h"
#include "game_presentation.h"


void renderer_add_cell(v2i position, Color color);
void renderer_add_cell_in_left_bar(v2i position, Color color);
void renderer_add_cell_in_right_bar(v2i position, Color color);



void init_graphics();

void platform_events();

void render();

bool window_open();

void shutdown_graphics();

