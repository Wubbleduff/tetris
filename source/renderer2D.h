#pragma once

#include "my_math.h"

#include <windows.h>

#include <vector> // vector for meshes

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcommon.h>
#include "DX\d3dx10math.h"
#include "DX\d3dx11async.h"
#include "DX\d3dx11tex.h"

struct Color
{
  float r, g, b, a;

  Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
  Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
};


void draw_cell(v2i position, Color color);
void draw_cell_in_left_bar(v2i position, Color color);
void draw_cell_in_right_bar(v2i position, Color color);

v2 window_to_world_space(v2 window_position);

void init_renderer(HWND window, unsigned framebuffer_width, unsigned framebuffer_height, bool is_fullscreen, bool is_vsync);
void render();
void swap_frame();
void shutdown_renderer();

ID3D11Device *get_d3d_device();
ID3D11DeviceContext *get_d3d_device_context();
