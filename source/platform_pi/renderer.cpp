

#include "pi_renderer.h"
#include "../game_renderer.h"

#include "../my_math.h" // v2


#include <dlfcn.h>
#include <cstdio>
#include <assert.h>

typedef void (*init_led_fn)(int num_leds, unsigned **strip_buffer);
typedef void (*render_to_led_fn)();
typedef void (*clear_led_fn)();
typedef void (*shutdown_led_fn)();

struct RendererData
{
  int width, height;
  unsigned *light_data;

  init_led_fn init_led;
  render_to_led_fn render_to_led;
  clear_led_fn clear_led;
  shutdown_led_fn shutdown_led;
};

static RendererData *renderer_data;

static const int NUM_LEDS_ON_STRIP = 256;
static const unsigned MAX_BRIGHTNESS_VALUE = 10;  


// Platform specific implementation
void init_renderer()
{
  renderer_data = (RendererData *)malloc(sizeof(RendererData));
  renderer_data->width  = 16;
  renderer_data->height = 16;

  // Load led renderer dll
  void *dll_handle = dlopen("./led_renderer.dll", RTLD_LAZY);
  if(!dll_handle)
  {
    fprintf(stderr, "Could not load led renderer dll\n");
    assert(0);
  }

  renderer_data->init_led = (init_led_fn)dlsym(dll_handle, "init_led");
  renderer_data->render_to_led = (render_to_led_fn)dlsym(dll_handle, "render_to_led");
  renderer_data->clear_led = (clear_led_fn)dlsym(dll_handle, "clear_led");
  renderer_data->shutdown_led = (shutdown_led_fn)dlsym(dll_handle, "shutdown_led");



  renderer_data->init_led(NUM_LEDS_ON_STRIP, &(renderer_data->light_data));
}

void render()
{
  renderer_data->render_to_led();
}

void swap_frame()
{
  renderer_data->clear_led();
}

void shutdown_renderer()
{
  renderer_data->shutdown_led();
  free(renderer_data);
}

v2 window_to_world_space(v2 window_position)
{
  return v2();
}





// Game rendering implementation
void draw_cell(v2i position, Color color)
{
  int column = position.x;
  int row = position.y;

  if(column < 0) return;
  if(row    < 0) return;
  if(column >= renderer_data->width)  return;
  if(row    >= renderer_data->height) return;

  if(row % 2 == 1) column = (renderer_data->width - 1) - column;

  float alpha = color.a;
  int r = MAX_BRIGHTNESS_VALUE * color.r * alpha;
  int g = MAX_BRIGHTNESS_VALUE * color.g * alpha; 
  int b = MAX_BRIGHTNESS_VALUE * color.b * alpha; 

  unsigned value = (b << 16) | (g << 8) | (r << 0);

  renderer_data->light_data[row * renderer_data->width + column] = value;
}

void draw_cell_in_left_bar(v2i position, Color color)
{
}

void draw_cell_in_right_bar(v2i position, Color color)
{
}


