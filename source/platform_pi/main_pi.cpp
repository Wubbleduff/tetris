
#include "pi_renderer.h"

#include "../input.h"
#include "../tetris.h"

#include "stdlib.h" // malloc

// Input
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)

// Time
#include <chrono>


#define MAX_KEYS 256

struct PlatformState
{
  bool game_running = false;

  int keyboard_file;
  bool keys_down[MAX_KEYS];

  std::chrono::high_resolution_clock::time_point last_time;
  float dt = 0.0f;
};

PlatformState *state;


// Input implementation
void init_input()
{
  int keyboard_file;
  int version;
  unsigned short id[4];
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
  
  //----- OPEN THE INPUT DEVICE -----
  if((keyboard_file = open("/dev/input/event0", O_RDONLY)) < 0)    //<<<<SET THE INPUT DEVICE PATH HERE
  {
    perror("KeyboardMonitor can't open input device");
    close(keyboard_file);
    return;
  }
  
  //----- GET DEVICE VERSION -----
  if(ioctl(keyboard_file, EVIOCGVERSION, &version))
  {
    printf("KeyboardMonitor can't get version\n");
    close(keyboard_file);
    return;
  }
  printf("Input driver version is %d.%d.%d\n", version >> 16, (version >> 8) & 0xff, version & 0xff);

  //----- GET DEVICE INFO -----
  ioctl(keyboard_file, EVIOCGID, id);
  printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n", id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);
  
  memset(bit, 0, sizeof(bit));
  ioctl(keyboard_file, EVIOCGBIT(0, EV_MAX), bit[0]);


  // Set non blocking
  int flags = fcntl(keyboard_file, F_GETFL, 0) | O_NONBLOCK;
  fcntl(keyboard_file, F_SETFL, flags);

  state->keyboard_file = keyboard_file;
}

bool button_toggled_down(unsigned char key)
{
}

bool button_toggled_up(unsigned char key)
{
}

bool button_state(unsigned char key)
{
  if(key < 0)         return false;
  if(key >= MAX_KEYS) return false;

  // Q : 16
  // A : 30

  switch(key)
  {
    case 'W': return state->keys_down[17];
    case 'A': return state->keys_down[30];
    case 'S': return state->keys_down[31];
    case 'D': return state->keys_down[32];

    case 'I': return state->keys_down[23];
    case 'J': return state->keys_down[36];
    case 'K': return state->keys_down[37];
    case 'L': return state->keys_down[38];

    case 'R': return state->keys_down[19];

    case ' ': return state->keys_down[57];


    default:  return false;
  }
}

static void read_input()
{
  struct input_event read_input_event[64];
  int bytes_read = read(state->keyboard_file, read_input_event, sizeof(struct input_event) * 64);

  if(bytes_read < 0)
  {
    if(errno != EAGAIN) fprintf(stderr, "Abnormal error when reading input: %i\n", errno);
    return;
  }
  else
  {
    for(int i = 0; i < bytes_read / sizeof(struct input_event); i++)
    {
      //We have:
      //read_input_event[i].time  timeval: 16 bytes (8 bytes for seconds, 8 bytes for microseconds)
      //read_input_event[i].type  See input-event-codes.h
      //read_input_event[i].code  See input-event-codes.h
      //  read_input_event[i].value    01 for keypress, 00 for release, 02 for autorepeat
          
      if(read_input_event[i].type == EV_KEY)
      {
        if(read_input_event[i].value == 2)
        {
          //This is an auto repeat of a held down key
          //std::cout << (int)(read_input_event[i].code) << " Auto Repeat";
          //std::cout << std::endl;
        }
        else if(read_input_event[i].value == 1)
        {
          //----- KEY DOWN -----
          //std::cout << (int)(read_input_event[i].code) << " Key Down";    //input-event-codes.h
          //std::cout << std::endl;
          //printf("Key down: %i\n", (int)(read_input_event[i].code));
          
          // TODO: I don't really know how this works. Maybe this is wrong?
          state->keys_down[read_input_event[i].code] = true;
        }
        else if(read_input_event[i].value == 0)
        {
          //----- KEY UP -----
          //std::cout << (int)(read_input_event[i].code) << " Key Up";    //input-event-codes.h
          //std::cout << std::endl;
          //printf("Key up: %i\n", (int)(read_input_event[i].code));

          // TODO: I don't really know how this works. Maybe this is wrong?
          state->keys_down[read_input_event[i].code] = false;
        }
      }
    }
  }
}
void shutdown_input()
{
  close(state->keyboard_file);
}


// Time implementation
// In ms
float get_dt()
{
  if(state->dt >= 33.3f)
  {
    printf("dt: %f\n", state->dt);
    return 33.3f;
  }

  return state->dt;
}


int main(int argc, char **argv)
{
  state = (PlatformState *)malloc(sizeof(PlatformState));

  // Initializtion
  init_input();
  init_renderer();
  init_tetris();

  // Main loop
  state->game_running = true;
  while(state->game_running)
  {
    // Input
    read_input();
    if(state->keys_down[1])
    {
      state->game_running = false;
      break; 
    }


    // timing stuff
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

    double time_span = std::chrono::duration_cast<std::chrono::nanoseconds>(now - state->last_time).count();
    time_span *= 1e-6;

    state->dt = time_span;
    state->last_time = now;


    update_tetris();

    render();
    swap_frame();

    for(int i = 0; i < MAX_KEYS; i++) state->keys_down[i] = false;
  }


  shutdown_input();
  shutdown_renderer();
  free(state);
  return 0;
}

