#include "tetris.h"

#include "game_presentation.h"
#include "input.h"
#include "game_timer.h"

#include <chrono> // For seeding random
#include <random>
#include <cstring> // memset

static const int NUM_NEXT_PIECES = 6;
static const float FALL_INTERVAL = 200.0f;
static const float SPEED_UP_MODIFIER = 5.0f;
static const float LOCK_TIME = 500.0f;
static const float LOCK_TOLERANCE = 2000.0f;

enum PieceType
{
  I_PIECE,
  J_PIECE,
  L_PIECE,
  O_PIECE,
  S_PIECE,
  T_PIECE,
  Z_PIECE,

  NO_PIECE
};

enum RotationState
{
  RS_0,
  RS_R,
  RS_2,
  RS_L,
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
  // Game grid
  Grid grid;


  // Piece generation
  std::default_random_engine *generator;
  std::uniform_int_distribution<int> *distribution;
  int next_piece_index = 0;
  PieceType next_pieces[NUM_NEXT_PIECES];


  // Swap piece
  PieceType held_piece;
  bool swapped_piece_this_turn = false;


  // Falling piece
  Piece falling_piece;
  float lock_delay_timer = LOCK_TIME;
  float lock_tolerance_timer = LOCK_TOLERANCE;


  // Grid cells to clear
  int num_rows_to_clear = 0;
  int rows_to_clear[4] = {};

  
  bool freeze = false;

  unsigned score = 0;
};




// GLOBALS
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













/*
static v2 mouse_world_position()
{
  return window_to_world_space(MouseWindowPosition());
}

static v2i mouse_grid_position()
{
  v2 world_pos = mouse_world_position();
  return v2i((int)(world_pos.x), (int)(world_pos.y));
}
*/




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

  return Color();
}

static void draw_piece(PieceType type, v2i position, float opaqueness, int screen_position = 0)
{
  Piece piece;
  switch(type)
  {
    case 0: {make_i_piece(&piece); break;}
    case 1: {make_j_piece(&piece); break;}
    case 2: {make_l_piece(&piece); break;}
    case 3: {make_o_piece(&piece); break;}
    case 4: {make_s_piece(&piece); break;}
    case 5: {make_t_piece(&piece); break;}
    case 6: {make_z_piece(&piece); break;}

    default: return;
  }

  for(int i = 0; i < 4; i++)
  {
    v2i pos = piece.points[i] + position;
    Color color = piece_color(type);
    color.a = opaqueness;

    if(screen_position == -1)     draw_cell_in_left_bar(pos, color);
    else if(screen_position == 0) draw_cell(pos, color);
    else if(screen_position == 1) draw_cell_in_right_bar(pos, color);
  }
}

static void draw_piece(Piece *piece, v2i position, float opaqueness, int screen_position = 0)
{
  if(piece->type == NO_PIECE) return;

  for(int i = 0; i < 4; i++)
  {
    v2i pos = piece->points[i] + position;
    Color color = piece_color(piece->type);
    color.a = opaqueness;

    if(screen_position == -1)     draw_cell_in_left_bar(pos, color);
    else if(screen_position == 0) draw_cell(pos, color);
    else if(screen_position == 1) draw_cell_in_right_bar(pos, color);
  }
}

static bool animate_filled_rows()
{
  static float timer = 0.0f;
  static int current_column = 0;

  static const float animation_interval = 40.0f;
  float animation_threshold = animation_interval;

  timer += get_dt();

  Grid *grid = &game_state.grid;
  if(timer >= animation_threshold)
  {
    for(int i = 0; i < game_state.num_rows_to_clear; i++)
    {
      v2i cell = v2i(current_column, game_state.rows_to_clear[i]);
      (*grid)[cell].color = Color(0.0f, 0.0f, 0.0f, 1.0f);
    }

    current_column++;

    timer -= animation_threshold;
  }

  if(current_column >= grid->columns)
  {
    current_column = 0;
    return true;
  }

  return false;
}

static void spawn_piece(PieceType type)
{
  Piece *falling_piece = &game_state.falling_piece;

  falling_piece->position = v2i(4, 16);
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

  spawn_piece((PieceType)game_state.next_pieces[game_state.next_piece_index]);
  game_state.next_pieces[game_state.next_piece_index] = (PieceType)num;
  game_state.next_piece_index++;
  game_state.next_piece_index %= NUM_NEXT_PIECES;
}

static void reset_next_pieces()
{
  static int last_num;
  for(int i = 0; i < NUM_NEXT_PIECES; i++)
  {
    int num = (*game_state.distribution)(*game_state.generator);

    // If repeated piece, roll again
    if(num == last_num) num = (*game_state.distribution)(*game_state.generator);
    last_num = num;

    game_state.next_pieces[i] = (PieceType)num;
  }

  game_state.next_piece_index = 0;
}

static void restart_game()
{
  Grid *grid = &game_state.grid;
  for(int i = 0; i < grid->rows * grid->columns; i++)
  {
    grid->cells[i].filled = false;
  }

  game_state.held_piece = NO_PIECE;

  reset_next_pieces();

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

static void mark_filled_rows()
{
  Grid *grid = &game_state.grid;

  int num_marked_rows = 0;
  int rows_to_clear[4] = {};

  // Go through cells and find filled row
  for(int row = 0; row < grid->rows; row++)
  {
    bool found_gap = false;
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
      rows_to_clear[num_marked_rows] = row;
      num_marked_rows++;
    }
  }

  game_state.num_rows_to_clear = num_marked_rows;
  for(int i = 0; i < num_marked_rows; i++) game_state.rows_to_clear[i] = rows_to_clear[i];

  int score_increase = num_marked_rows * 10;
  //if(num_marked_rows == 4) score_increase *= 4;
  game_state.score += score_increase;
}

static void clear_marked_rows()
{
  Grid *grid = &game_state.grid;
  int num_rows = game_state.num_rows_to_clear;

  while(num_rows)
  {
    // NOTE:
    // The array of rows to clear is assumed to be ordered bottom-up
    int target = game_state.rows_to_clear[num_rows - 1];
    int to_copy = target + 1;

    // Clear the top-most row
    memset(&(grid->cells[(grid->rows - 1) * grid->columns]), 0, sizeof(Cell) * grid->columns);

    // Move all rows from the target row down one
    while(target != grid->rows - 1)
    {
      Cell *dest = &(grid->cells[target * grid->columns]);
      Cell *source = &(grid->cells[to_copy * grid->columns]);

      memcpy(dest, source, sizeof(Cell) * grid->columns);

      target++;
      to_copy++;
    }

    num_rows--;
  }

  game_state.num_rows_to_clear = 0;
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

  // Check for rows to mark
  mark_filled_rows();

  // Reset falling piece state
  game_state.swapped_piece_this_turn = false;
  game_state.lock_delay_timer = LOCK_TIME;
  game_state.lock_tolerance_timer = LOCK_TOLERANCE;
  game_state.falling_piece.type = NO_PIECE;
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
  game_state.grid.columns = 10;
  game_state.grid.rows = 24;
  game_state.grid.cells = new Cell[game_state.grid.rows * game_state.grid.columns];

  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  game_state.generator = new std::default_random_engine(seed);
  game_state.distribution = new std::uniform_int_distribution<int>(0, 6);

  restart_game();
}

void update_tetris()
{
  /*
  static v2i pos = v2i();
  pos.x++;
  if(pos.x >= 16)
  {
    pos.x = 0;
    pos.y++;
  }
  if(pos.y >= 16)
  {
    pos.y = 0;
  }
  draw_cell(pos, Color(0, 0, 1, 1));
  */


#if 1
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

    if(button_state('W') && !w_down) w_toggled = true;
    if(button_state('A') && !a_down) a_toggled = true; 
    if(button_state('S') && !s_down) s_toggled = true;
    if(button_state('D') && !d_down) d_toggled = true;
    if(button_state('J') && !j_down) j_toggled = true; 
    if(button_state('L') && !l_down) l_toggled = true;
    if(button_state('R') && !r_down) r_toggled = true;
    if(button_state(' ') && !space_down) space_toggled = true;

    w_down = button_state('W');
    a_down = button_state('A');
    s_down = button_state('S');
    d_down = button_state('D');
    j_down = button_state('J');
    l_down = button_state('L');
    r_down = button_state('R');
    space_down = button_state(' ');
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

  if(button_state('A') && button_state('D'))
  {
    move_counter = 0;
    delay_counter = 0;
  }
  if(button_state('A'))
  {
    delay_counter -= dt;
  }
  else if(button_state('D'))
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



  // Swap piece
  if(space_toggled && !game_state.swapped_piece_this_turn && !game_state.freeze)
  {
    PieceType type = game_state.held_piece;
    game_state.held_piece = falling_piece->type;

    if(type == NO_PIECE) spawn_next_piece();
    else spawn_piece(type);

    game_state.swapped_piece_this_turn = true;
  }



  int want_to_rotate = 0;;
  if(j_toggled) want_to_rotate = -1;
  if(l_toggled) want_to_rotate = 1;
  if(j_toggled && l_toggled) want_to_rotate = 0;

  int going_to_move = 0;
  int going_to_rotate = 0;
  v2i kick_offset = v2i(0, 0);
  bool want_to_lock_piece = false;
  bool want_to_fall_faster = button_state('S');
  bool want_to_hard_drop = w_toggled;

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
  bool locked_piece = false;
  {
    // Piece moves down after time interval

    static float fall_counter = 0.0f;

    // Move based on input
    if(going_to_move && !game_state.freeze)
    {
      falling_piece->position.x += going_to_move;
      if(game_state.lock_tolerance_timer > 0.0f) game_state.lock_delay_timer = LOCK_TIME;
    }

    if(going_to_rotate && !game_state.freeze)
    {
      rotate(falling_piece, going_to_rotate);
      if(game_state.lock_tolerance_timer > 0.0f) game_state.lock_delay_timer = LOCK_TIME;
    }

    falling_piece->position += kick_offset;

    if(want_to_lock_piece)
    {
      game_state.lock_delay_timer -= dt;
      game_state.lock_tolerance_timer -= dt;

      if(game_state.lock_delay_timer <= 0.0f)
      {
        // Lock piece
        lock_piece(falling_piece);
        locked_piece = true;
      }
    }
    else
    {
      if(!game_state.freeze)
      {
        if(want_to_fall_faster) fall_counter += dt * SPEED_UP_MODIFIER;
        else fall_counter += dt;
      }
      //if(s_toggled) falling_piece->position.y -= 1;

      if(fall_counter >= FALL_INTERVAL)
      {
        falling_piece->position.y -= 1;
        fall_counter -= FALL_INTERVAL;
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
      

      if(want_to_hard_drop && !game_state.freeze)
      {
        falling_piece->position = ghost_piece.position;
        lock_piece(falling_piece);
        locked_piece = true;
      }
    }
  }



  // Clearing filled rows
  if(game_state.num_rows_to_clear > 0)
  {
    game_state.freeze = true;
    bool done = animate_filled_rows();

    // Just got done clearing rows
    if(done)
    {
      clear_marked_rows();
      spawn_next_piece();
    }
  }
  else
  {
    game_state.freeze = false;
    if(locked_piece) spawn_next_piece();
  }




  // Debugging
  /*
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
  */

  //draw_rect(v3(mouse_world_position(), 0.0f), v2(1.0f, 1.0f), 0.0f, Color(1, 1, 1, 1));






  // Draw calls

  // Draw ghost piece
  draw_piece(&ghost_piece, ghost_piece.position, 0.25f);

  for(int row = 0; row < grid->rows; row++)
  {
    for(int column = 0; column < grid->columns; column++)
    {
      Cell *cell = &(*grid)[v2i(column, row)];
      if(cell->filled)
      {
        draw_cell(v2i(column, row), cell->color);
      }
    }
  }

  // Draw falling piece
  draw_piece(falling_piece, falling_piece->position, 1.0f);


  const int spacing = 2;
  const int begin_height = (spacing * NUM_NEXT_PIECES) / 2;

  // Draw held piece
  if(game_state.swapped_piece_this_turn) draw_piece(game_state.held_piece, v2i(0, begin_height), 0.1f, -1);
  else draw_piece(game_state.held_piece, v2i(0, begin_height), 1.0f, -1);

  for(int piece = 0; piece < NUM_NEXT_PIECES; piece++)
  {
    int index = (game_state.next_piece_index + piece) % NUM_NEXT_PIECES;
    draw_piece(game_state.next_pieces[index], v2i(0, -piece * 2 * spacing + begin_height), 1.0f, 1);
  }
#endif
}

