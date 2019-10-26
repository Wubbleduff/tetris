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

struct Mesh;
struct Shader;
struct Texture;
struct Model
{
  const char *debug_name = "";
  bool show = true;

  v3 position = v3();
  v2 scale = v2(1.0f, 1.0f);
  float rotation = 0.0f; // In degrees

  v4 blend_color = v4(1.0f, 1.0f, 1.0f, 1.0f);

  bool wireframe = false;

  Mesh *mesh = 0;
  Shader *shader = 0;
  Texture *texture = 0;
};

typedef unsigned ModelHandle;


ModelHandle draw_rect(v3 position, v2 scale, float rotation, Color color = Color(1.0f, 1.0f, 1.0f, 1.0f));
Model *get_temp_model_pointer(ModelHandle model);

v2 window_to_world_space(v2 window_position);

void set_camera_position(v2 position);
void set_camera_width(float width);



void init_renderer(HWND window, unsigned framebuffer_width, unsigned framebuffer_height, bool is_fullscreen, bool is_vsync);
void render();
void swap_frame();
void shutdown_renderer();

ID3D11Device *get_d3d_device();
ID3D11DeviceContext *get_d3d_device_context();
