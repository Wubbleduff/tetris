#include "tetris.h"
#include "renderer2d.h"

#include <chrono> // For seeding random
#include <random>

enum PieceType
{
  I_PIECE,
  J_PIECE,
  L_PIECE,
  O_PIECE,
  S_PIECE,
  T_PIECE,
  Z_PIECE
};

enum RotationState
{
  RS_0,
  RS_R,
  RS_2,
  RS_L,
};

struct v2i
{
  int x, y;

  v2i() {}
  v2i(int inX, int inY) : x(inX), y(inY) {}

  v2i operator+(const v2i &rhs) { return v2i(x + rhs.x, y + rhs.y); }
  v2i operator-(const v2i &rhs) { return v2i(x - rhs.x, y - rhs.y); }

  v2i &operator+=(const v2i &rhs) { x += rhs.x; y += rhs.y; return *this; }
  v2i &operator-=(const v2i &rhs) { x -= rhs.x; y -= rhs.y; return *this; }
};

struct Piece
{
  PieceType type;

  v2i position;
  RotationState rotation;

  v2i points[4];
};

struct Cell
{
  bool filled = false;
  Color color = Color(0.0f, 0.0f, 0.0f, 1.0f);
};

struct Grid
{
  int rows, columns;

  Cell *cells;

  Cell &operator[](v2i point) { return cells[point.y * columns + point.x]; }
};

struct GameState
{
  Grid grid;
  Piece falling_piece;
  Piece held_piece;

  std::default_random_engine *generator;
  std::uniform_int_distribution<int> *distribution;
};

static GameState game_state;


static const int NUM_KICK_TESTS = 5;
static v2i default_offset_data[20] =
{
  // 1         2         3         4         5
  { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}, // 0
  { 0,  0}, { 1,  0}, { 1, -1}, { 0,  2}, { 1,  2}, // R
  { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}, // 2
  { 0,  0}, {-1,  0}, {-1, -1}, { 0,  2}, {-1,  2}  // L
};
static v2i i_piece_offset_data[20] =
{
  // 1         2         3         4         5
  { 0,  0}, {-1,  0}, { 2,  0}, {-1,  0}, { 2,  0}, // 0
  {-1,  0}, { 0,  0}, { 0,  0}, { 0,  1}, { 0, -2}, // R
  {-1,  1}, { 1,  1}, {-2,  1}, { 1,  0}, {-2,  0}, // 2
  { 0,  1}, { 0,  1}, { 0,  1}, { 0, -1}, { 0,  2}  // L
};

static const int NUM_O_KICK_TESTS = 1;
static v2i o_piece_offset_data[4] =
{
  { 0,  0},
  { 0, -1},
  {-1, -1},
  {-1,  0}
};




static v2 mouse_world_position()
{
  return window_to_world_space(MouseWindowPosition());
}

static v2i mouse_grid_position()
{
  v2 world_pos = mouse_world_position();
  return v2i((int)(world_pos.x), (int)(world_pos.y));
}

static void make_i_piece(Piece *p)
{
  p->type = I_PIECE;
  p->points[0] = v2i(-1, 0);
  p->points[1] = v2i( 0, 0);
  p->points[2] = v2i( 1, 0);
  p->points[3] = v2i( 2, 0);
}

static void make_j_piece(Piece *p)
{
  p->type = J_PIECE;
  p->points[0] = v2i(-1, 1);
  p->points[1] = v2i(-1, 0);
  p->points[2] = v2i( 0, 0);
  p->points[3] = v2i( 1, 0);
}

static void make_l_piece(Piece *p)
{
  p->type = L_PIECE;
  p->points[0] = v2i(-1, 0);
  p->points[1] = v2i( 0, 0);
  p->points[2] = v2i( 1, 0);
  p->points[3] = v2i( 1, 1);
}

static void make_o_piece(Piece *p)
{
  p->type = O_PIECE;
  p->points[0] = v2i( 0,  0);
  p->points[1] = v2i( 1,  0);
  p->points[2] = v2i( 1,  1);
  p->points[3] = v2i( 0,  1);
}

static void make_s_piece(Piece *p)
{
  p->type = S_PIECE;
  p->points[0] = v2i(-1,  0);
  p->points[1] = v2i( 0,  0);
  p->points[2] = v2i( 0,  1);
  p->points[3] = v2i( 1,  1);
}

static void make_t_piece(Piece *p)
{
  p->type = T_PIECE;
  p->points[0] = v2i( 0, 0);
  p->points[1] = v2i(-1, 0);
  p->points[2] = v2i( 1, 0);
  p->points[3] = v2i( 0, 1);
}

static void make_z_piece(Piece *p)
{
  p->type = Z_PIECE;
  p->points[0] = v2i(-1,  1);
  p->points[1] = v2i( 0,  1);
  p->points[2] = v2i( 0,  0);
  p->points[3] = v2i( 1,  0);
}

static Color piece_color(PieceType type)
{
  switch(type)
  {
    case I_PIECE: { return Color(0.0f, 1.0f, 1.0f, 1.0f); }
    case J_PIECE: { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
    case L_PIECE: { return Color(1.0f, 0.5f, 0.0f, 1.0f); }
    case O_PIECE: { return Color(1.0f, 1.0f, 0.0f, 1.0f); }
    case S_PIECE: { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
    case T_PIECE: { return Color(1.0f, 0.0f, 1.0f, 1.0f); }
    case Z_PIECE: { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
  }
}

static void spawn_piece(PieceType type)
{
  Piece *falling_piece = &game_state.falling_piece;

  falling_piece->position = v2i(4, 22);
  falling_piece->rotation = RS_0;

  switch(type)
  {
    case 0: {make_i_piece(falling_piece); break;}
    case 1: {make_j_piece(falling_piece); break;}
    case 2: {make_l_piece(falling_piece); break;}
    case 3: {make_o_piece(falling_piece); break;}
    case 4: {make_s_piece(falling_piece); break;}
    case 5: {make_t_piece(falling_piece); break;}
    case 6: {make_z_piece(falling_piece); break;}
  }
}

static void spawn_next_piece()
{

  static int last_num;
  int num = (*game_state.distribution)(*game_state.generator);

  // If repeated piece, roll again
  if(num == last_num) num = (*game_state.distribution)(*game_state.generator);
  last_num = num;

  spawn_piece((PieceType)num);
}

static void restart_game()
{
  Grid *grid = &game_state.grid;
  for(int i = 0; i < grid->rows * grid->columns; i++)
  {
    grid->cells[i].filled = false;
  }

  spawn_next_piece();
}





static void swap(int &a, int &b)
{
  a ^= b;
  b ^= a;
  a ^= b;
}

static bool valid_point(v2i p)
{
  if(p.x < 0 || p.x >= game_state.grid.columns ||
     p.y < 0 || p.y >= game_state.grid.rows) return false;
  return true;
}

static void lock_piece(Piece *piece)
{
  Grid *grid = &game_state.grid;

  // Lock grid pieces
  for(int i = 0; i < 4; i++)
  {
    v2i p = piece->position + piece->points[i];
    if(!valid_point(p)) continue;

    (*grid)[p].filled = true;
    (*grid)[p].color = piece_color(piece->type);
  }

  // Kill filled rows
  // Go through cells and find filled row
  for(int row = 0; row < grid->rows; row++)
  {
    bool found_gap = false;
    int full_row_index = -1;
    for(int column = 0; column < grid->columns; column++)
    {
      if((*grid)[v2i(column, row)].filled == false)
      {
        found_gap = true;
        break;
      }
    }

    if(!found_gap)
    {
      // Go through each row starting at the current row and copy row down
      // Clear the top-most row
      for(int i = 0; i < grid->columns; i++) (*grid)[v2i(i, grid->rows - 1)].filled = false;
      // Move the rest down one row
      for(int breaking_row = row; breaking_row < grid->rows - 1; breaking_row++)
      {
        for(int column = 0; column < grid->columns; column++)
        {
          (*grid)[v2i(column, breaking_row)] = (*grid)[v2i(column, breaking_row + 1)];
        }
      }

      // Check this row again
      row--;
    }
  }


  // Now make the next piece
  spawn_next_piece();
}

static bool hit_side(Piece *p)
{
  Grid *grid = &game_state.grid;

  for(int i = 0; i < 4; i++)
  {
    v2i point = p->position + p->points[i];
    if(point.x < 0 || point.x >= grid->columns) return true;
  }

  return false;
}

static bool below_bottom(Piece *p)
{
  Grid *grid = &game_state.grid;

  for(int i = 0; i < 4; i++)
  {
    v2i point = p->position + p->points[i];
    if(point.y < 0) return true;
  }

  return false;
}

static bool hit_block(Piece *p)
{
  Grid *grid = &game_state.grid;

  for(int i = 0; i < 4; i++)
  {
    v2i point = p->position + p->points[i];
    if(!valid_point(point)) continue;
    if((*grid)[point].filled) return true;
  }

  return false;
}

static void rotate_left(Piece *p)
{
  if(p->rotation == RS_0) p->rotation = RS_L;
  else p->rotation = (RotationState)(p->rotation - 1);

  for(int i = 0; i < 4; i++)
  {
    v2i &point = p->points[i];
    swap(point.x, point.y);
    point.x = -point.x;
  }
}

static void rotate_right(Piece *p)
{
  p->rotation = (RotationState)((p->rotation + 1) % 4);
  for(int i = 0; i < 4; i++)
  {
    v2i &point = p->points[i];
    swap(point.x, point.y);
    point.y = -point.y;
  }
}

// -1 is counter-clockwise, 1 is clockwise
static void rotate(Piece *p, int direction)
{
  if(direction == -1)     rotate_left(p);
  else if(direction == 1) rotate_right(p);
}

// Returns true if successfully kicked piece into valid position, false otherwise
static bool try_kick(Piece *piece, RotationState prev_rotation)
{
  RotationState curr_rotation = piece->rotation;

  int num_tests = NUM_KICK_TESTS;
  v2i *offset_data;
  if(piece->type == O_PIECE)
  {
    num_tests = NUM_O_KICK_TESTS;
    offset_data = o_piece_offset_data;
  }
  else if(piece->type == I_PIECE)
  {
    offset_data = i_piece_offset_data;
  }
  else
  {
    offset_data = default_offset_data;
  }

  bool success = false;
  for(int i = 0; i < num_tests; i++)
  {
    v2i prev_state_offset = offset_data[prev_rotation * num_tests + i];
    v2i curr_state_offset = offset_data[curr_rotation * num_tests + i];

    v2i test_offset = prev_state_offset - curr_state_offset;

    piece->position += test_offset;
    if(hit_block(piece) || hit_side(piece) || below_bottom(piece))
    {
      piece->position -= test_offset;
      continue;
    }
    else
    {
      success = true;
      break;
    }
  }

  return success;
}







void init_tetris()
{
  set_camera_position(v2(5.0f, 12.0f));
  set_camera_width(10.0f);

  game_state.grid.columns = 10;
  game_state.grid.rows = 24;
  game_state.grid.cells = new Cell[game_state.grid.rows * game_state.grid.columns];

  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  game_state.generator = new std::default_random_engine(seed);
  game_state.distribution = new std::uniform_int_distribution<int>(0, 6);

  spawn_next_piece();
}

void update_tetris()
{
  Grid *grid = &game_state.grid;
  Piece *falling_piece = &game_state.falling_piece;

  // Record input
  bool w_toggled = false;
  bool a_toggled = false;
  bool s_toggled = false;
  bool d_toggled = false;
  bool j_toggled = false;
  bool l_toggled = false;
  bool r_toggled = false;
  bool space_toggled = false;
  {
    static bool w_down = false;
    static bool a_down = false;
    static bool s_down = false;
    static bool d_down = false;
    static bool j_down = false;
    static bool l_down = false;
    static bool r_down = false;
    static bool space_down = false;

    if(ButtonDown('W') && !w_down) w_toggled = true;
    if(ButtonDown('A') && !a_down) a_toggled = true; 
    if(ButtonDown('S') && !s_down) s_toggled = true;
    if(ButtonDown('D') && !d_down) d_toggled = true;
    if(ButtonDown('J') && !j_down) j_toggled = true; 
    if(ButtonDown('L') && !l_down) l_toggled = true;
    if(ButtonDown('R') && !r_down) r_toggled = true;
    if(ButtonDown(' ') && !space_down) space_toggled = true;

    w_down = ButtonDown('W');
    a_down = ButtonDown('A');
    s_down = ButtonDown('S');
    d_down = ButtonDown('D');
    j_down = ButtonDown('J');
    l_down = ButtonDown('L');
    r_down = ButtonDown('R');
    space_down = ButtonDown(' ');
  }

  if(r_toggled)
  {
    restart_game();
  }

  float dt = get_dt();


  int want_to_move = 0;
  if(a_toggled) want_to_move = -1;
  if(d_toggled) want_to_move =  1;
  if(a_toggled && d_toggled) want_to_move = 0;

  static float delay_counter = 0;
  static float move_counter = 0;
  const static float move_interval = 50.0f;
  const static float delay_time = 125.0f;

  if(ButtonDown('A') && ButtonDown('D'))
  {
    move_counter = 0;
    delay_counter = 0;
  }
  if(ButtonDown('A'))
  {
    delay_counter -= dt;
  }
  else if(ButtonDown('D'))
  {
    delay_counter += dt;
  }
  else
  {
    move_counter = 0;
    delay_counter = 0;
  }



  if(delay_counter <= -delay_time)
  {
    move_counter += dt;

    if(move_counter >= move_interval)
    {
      want_to_move = -1;
      move_counter -= move_interval;
    }
  }

  if(delay_counter >= delay_time)
  {
    move_counter += dt;

    if(move_counter >= move_interval)
    {
      want_to_move = 1;
      move_counter -= move_interval;
    }
  }



  /*
  if(move_counter <= -move_threshold && move_counter % move_interval == 0)
  {
    want_to_move = -1;
    move_counter += move_interval;
  }
  if(move_counter >= move_threshold && move_counter % move_interval == 0)
  {
    want_to_move = 1;
    move_counter -= move_interval;
  }
  */



  int want_to_rotate = 0;;
  if(j_toggled) want_to_rotate = -1;
  if(l_toggled) want_to_rotate = 1;
  if(j_toggled && l_toggled) want_to_rotate = 0;

  int going_to_move = 0;
  int going_to_rotate = 0;
  v2i kick_offset = v2i(0, 0);
  bool want_to_lock_piece = false;
  bool want_to_fall_faster = ButtonDown('S');
  bool want_to_hard_drop = w_toggled;



  // Swap piece
  {
    if(space_toggled)
    {
      PieceType type = game_state.held_piece.type;
      game_state.held_piece = *falling_piece;
      spawn_piece(type);
    }
  }


  // Collision checks
  {
    Piece future_piece = *falling_piece;

    // Check if collision when trying to rotate
    RotationState prev_rotation = future_piece.rotation;
    rotate(&future_piece, want_to_rotate);
    bool kicked = try_kick(&future_piece, prev_rotation);
    if(kicked)
    {
      going_to_rotate = want_to_rotate;
      kick_offset = future_piece.position - falling_piece->position;
    }

    // Check if collision when moving horizontally
    future_piece.position.x += want_to_move;
    bool outside = hit_side(&future_piece);
    bool inside_block = hit_block(&future_piece);
    if(!outside && !inside_block)
    {
      going_to_move = want_to_move;
    }
    else
    {
      // Otherwise, reset the future piece
      future_piece = *falling_piece;
    }

    // After horizontal checks, check vertical with new results
    future_piece = *falling_piece;
    future_piece.position.x += going_to_move;

    // Check if collision when moving vertically
    future_piece.position.y -= 1;
    bool at_bottom = below_bottom(&future_piece);
    bool just_above_block = hit_block(&future_piece);
    if(at_bottom || just_above_block)
    {
      want_to_lock_piece = true;
    }
  }



  // Move falling piece
  Piece ghost_piece;
  {
    // Piece moves down after time interval

    static const float fall_interval = 200.0f;
    static const float speed_up_modifier = 5.0f;
    static const float lock_time = 500.0f;
    static float fall_counter = 0.0f;
    static float lock_delay = lock_time;

    // Move based on input
    if(going_to_move)
    {
      falling_piece->position.x += going_to_move;
      lock_delay = lock_time;
    }

    if(going_to_rotate)
    {
      rotate(falling_piece, going_to_rotate);
      lock_delay = lock_time;
    }

    falling_piece->position += kick_offset;

    if(want_to_lock_piece)
    {
      lock_delay -= dt;

      if(lock_delay <= 0.0f)
      {
        // Lock piece
        lock_piece(falling_piece);
        lock_delay = lock_time;
      }
    }
    else
    {
      if(want_to_fall_faster) fall_counter += dt * speed_up_modifier;
      else fall_counter += dt;

      if(fall_counter >= fall_interval)
      {
        falling_piece->position.y -= 1;
        fall_counter -= fall_interval;
      }
    }


    // Hard drop
    ghost_piece = *falling_piece;
    {
      bool hit_somthing = false;
      while(!hit_somthing)
      {
        ghost_piece.position.y -= 1;

        bool at_bottom = below_bottom(&ghost_piece);
        bool just_above_block = hit_block(&ghost_piece);
        if(at_bottom || just_above_block)
        {
          hit_somthing = true;
        }
      }
      ghost_piece.position.y += 1;
      

      if(want_to_hard_drop)
      {
        falling_piece->position = ghost_piece.position;
        lock_piece(falling_piece);
        lock_delay = 0;
      }
    }
  }




  // Debugging
  if(MouseDown(0))
  {
    v2i mouse_pos = mouse_grid_position();
    (*grid)[mouse_pos].filled = true;
    (*grid)[mouse_pos].color = Color(0.5f, 0.5f, 0.5f, 1.0f);
  }
  if(MouseDown(1))
  {
    v2i mouse_pos = mouse_grid_position();
    (*grid)[mouse_pos].filled = false;
  }

  //draw_rect(v3(mouse_world_position(), 0.0f), v2(1.0f, 1.0f), 0.0f, Color(1, 1, 1, 1));






  // Draw calls

  // Draw falling piece
  for(int i = 0; i < 4; i++)
  {
    v2i pos = falling_piece->position + falling_piece->points[i];
    Color color = piece_color(falling_piece->type);
    draw_rect(v3(pos.x, pos.y, 0.0f), v2(1.0f, 1.0f), 0.0f, color);
  }

  // Draw ghost piece
  for(int i = 0; i < 4; i++)
  {
    v2i pos = ghost_piece.position + ghost_piece.points[i];
    Color color = piece_color(ghost_piece.type);
    color.a = 0.25f;
    draw_rect(v3(pos.x, pos.y, 0.0f), v2(1.0f, 1.0f), 0.0f, color);
  }

  // Draw held piece
  for(int i = 0; i < 4; i++)
  {
    v2i pos = game_state.held_piece.points[i];
    Color color = piece_color(game_state.held_piece.type);
    color.a = 0.25f;
    draw_rect(v3(pos.x + 2.0f, pos.y + 20.0f, 0.0f), v2(1.0f, 1.0f), 0.0f, color);
  }

  for(int row = 0; row < grid->rows; row++)
  {
    for(int column = 0; column < grid->columns; column++)
    {
      Cell *cell = &(*grid)[v2i(column, row)];
      if(cell->filled)
      {
        draw_rect(v3(column, row, 0.0f), v2(1.0f, 1.0f), 0.0f, cell->color);
      }
    }
  }
}

