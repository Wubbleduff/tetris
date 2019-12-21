#include "renderer.h"
#include "network_client.h"
#include "input.h"
#include "tetris.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

//#include <windows.h>
#include <stdio.h>
#include <time.h>



static HWND window_handle;


static LARGE_INTEGER last_time;
static float dt;

static bool running;

static const float NETWORK_FREQUENCY = 33.33f;
static float network_timer = 0.0f;


#define MAX_BUTTONS 256
static bool keyStates[MAX_BUTTONS] = {};
static bool mouseStates[8] = {};

static v2 mouseWindowPosition;

static void init_imgui()
{
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;

  ImGui_ImplWin32_Init(window_handle);
  ImGui_ImplDX11_Init(get_d3d_device(), get_d3d_device_context());
}
static void imgui_frame()
{
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}
static void imgui_endframe()
{
  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}


bool button_state(unsigned char button)
{
  if(button < 0 || button > MAX_BUTTONS) return false;

  return keyStates[button];
}

bool MouseDown(unsigned button)
{
  if(button < 0 || button > 8) return false;

  return mouseStates[button];
}

v2 MouseWindowPosition()
{
  return mouseWindowPosition;
}

float get_dt()
{
  if(dt > 33.33f) return 33.33f;
  return dt;
}

static void initialize(unsigned client_width, unsigned client_height, bool is_fullscreen, bool is_vsync)
{
  init_renderer(window_handle, client_width, client_height, is_fullscreen, is_vsync);

  init_network_client("192.168.0.42", 4242, 16, 16);

  init_imgui();

  init_tetris();
}


static void shutdown()
{
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  shutdown_network_client();
  DestroyWindow(window_handle);
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = 0;

  if(ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return true;

  switch (message)
  {
    case WM_DESTROY:
    {
      running = false;
      PostQuitMessage(0);
      return 0;
    }
    break;

    case WM_CLOSE: 
    {
      running = false;
      return 0;
    }  
    break;

    case WM_KEYDOWN: 
    {
      if(wParam >= 0 && wParam < MAX_BUTTONS) keyStates[wParam] = true;
    }
    break;

    case WM_KEYUP:
    {
      if(wParam >= 0 && wParam < MAX_BUTTONS) keyStates[wParam] = false;
    }
    break;

    case WM_LBUTTONDOWN: { mouseStates[0] = true;  break; }
    case WM_LBUTTONUP:   { mouseStates[0] = false; break; }

    case WM_RBUTTONDOWN: { mouseStates[1] = true;  break; }
    case WM_RBUTTONUP:   { mouseStates[1] = false; break; }
    
    default:
    {
      result = DefWindowProc(window, message, wParam, lParam);
    }
    break;
  }
  
  return result;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  // Create the window class
  WNDCLASS window_class = {};
  window_class.style = CS_HREDRAW|CS_VREDRAW;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = hInstance;
  window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  window_class.lpszClassName = "Windows Program Class";
  if(!RegisterClass(&window_class)) { return 1; }

  
#if 0
  unsigned window_width = GetSystemMetrics(SM_CXSCREEN);
  unsigned window_height = GetSystemMetrics(SM_CYSCREEN);
#else
  unsigned window_width = 10 * 50 + 400;
  unsigned window_height = 24 * 50;
#endif

  window_handle = CreateWindowEx(0, window_class.lpszClassName, "Tetris", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, window_width, window_height, 0, 0, hInstance, 0);                                
  //window_handle = CreateWindowEx(0, window_class.lpszClassName, "Tetris", WS_POPUP | WS_VISIBLE, 0, 0, window_width, window_height, 0, 0, hInstance, 0);                                
  if(!window_handle) { return 1; }


  // Initialize
  RECT client_rect;
  GetClientRect(window_handle, &client_rect);
  initialize(client_rect.right, client_rect.bottom, false, false);


  // Main loop
  running = true;
  while(running)
  {
    MSG message;
    while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
      if(message.message == WM_QUIT)
      {
        running = false;
      }
      TranslateMessage(&message);
      DispatchMessage(&message);
    }

    if(keyStates[VK_ESCAPE])
    {
      PostQuitMessage(0);
      running = false;
    }

    // Mouse position
    POINT pos;
    GetCursorPos(&pos);
    ScreenToClient(window_handle, &pos);
    mouseWindowPosition = v2(pos.x, pos.y);


    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);

    // compute the elapsed time in millisec
    dt = (t.QuadPart - last_time.QuadPart) * 1000.0 / frequency.QuadPart;
    last_time = t;


    update_tetris();


    render();
    swap_frame();

    network_timer += get_dt();
    if(network_timer >= NETWORK_FREQUENCY)
    {
      printf("Sending at network time = %f\n", network_timer);
      send_network_data();
      network_timer -= NETWORK_FREQUENCY;
    }
  }

  shutdown();

  return 0;
}

