

#include "renderer.h"
//#include "network_client.h"
#include "input.h"
#include "game_timer.h"
#include "tetris.h"

#include <stdio.h>
#include <time.h>


static float dt = 0.0f;

float get_dt()
{
    if(dt > 33.33f) return 33.33f;
    return dt;
}

int main(int argc, char* argv[])
{
    init_graphics();

    init_tetris();

    bool game_running = true;
    timespec t0 = {0};
    timespec t1 = {0};
    while(game_running)
    {
        platform_events();

        if(!window_open())
        {
            game_running = false;
            break;
        }

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
        long diff_in_nanos = (t1.tv_sec - t0.tv_sec) * (long)1e9 + (t1.tv_nsec - t0.tv_nsec);
        long diff_in_millis = diff_in_nanos * 1e-6;
        dt = diff_in_millis;
        t0 = t1;

        update_tetris();

        render();
    }

    shutdown_graphics();
}


