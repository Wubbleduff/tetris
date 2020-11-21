#pragma once

#include "../game_presentation.h"
#include "../my_math.h"

void init_network_client(const char *ip_address, int port, unsigned width, unsigned height);

void send_network_data();

void shutdown_network_client();

void network_add_cell(v2i position, Color color);
void network_add_cell_in_left_bar(v2i position, Color color);
void network_add_cell_in_right_bar(v2i position, Color color);
