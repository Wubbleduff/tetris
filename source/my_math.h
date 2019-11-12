#pragma once

#include <math.h> // sqrt, cos, sin, atan2f
#include <assert.h>

static const float PI = 3.14159265f;

///////////////////////////////////////////////////////////////////////////////
// common operations
///////////////////////////////////////////////////////////////////////////////

static float squared(float a)
{
  return a * a;
}

static int min(int a, int b)
{
  return (a < b) ? a : b; 
}
static int min(int a, int b, int c)
{
  return min(a, min(b, c)); 
}
static float min(float a, float b)
{
  return (a < b) ? a : b; 
}
static float min(float a, float b, float c)
{
  return min(a, min(b, c)); 
}

static int max(int a, int b)
{
  return (a > b) ? a : b; 
}
static int max(int a, int b, int c)
{
  return max(a, max(b, c)); 
}
static float max(float a, float b)
{
  return (a > b) ? a : b; 
}
static float max(float a, float b, float c)
{
  return max(a, max(b, c)); 
}

static int clamp(int a, int min, int max)
{
  if(a < min) return min;
  if(a > max) return max;
  return a;
}
static float clamp(float a, float min, float max)
{
  if(a < min) return min;
  if(a > max) return max;
  return a;
}

static float absf(float a)
{
  return (a < 0.0f) ? -a : a;
}

static float deg_to_rad(float a)
{
  return a * (PI / 180.0f);
}

static float rad_to_deg(float a)
{
  return a * (180.0f / PI);
}



///////////////////////////////////////////////////////////////////////////////
// vector structs
///////////////////////////////////////////////////////////////////////////////
struct v2
{
  float x;
  float y;

  // Default constructor
  v2() : x(0.0f), y(0.0f) { }

  v2(float in_x, float in_y) : x(in_x), y(in_y) { }
};

struct v3
{
  float x;
  float y;
  float z;

  // Default constructor
  v3() : x(0.0f), y(0.0f), z(0.0f) { }

  // Non default constructor
  v3(float in_x, float in_y, float in_z) : x(in_x), y(in_y), z(in_z) { }

  v3(v2 v, float a) : x(v.x), y(v.y), z(a) { }
  v3(float a, v2 v) : x(a),   y(v.x), z(v.y) { }
};

struct v4
{
  float x;
  float y;
  float z;
  float w;

  // Default constructor
  v4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) { }

  // Non default constructor
  v4(float in_x, float in_y, float in_z, float in_w) : x(in_x), y(in_y), z(in_z), w(in_w) { }

  v4(v2 v, float a, float b) : x(v.x), y(v.y), z(a),   w(b)   { }
  v4(float a, v2 v, float b) : x(a),   y(v.x), z(v.y), w(b)   { }
  v4(float a, float b, v2 v) : x(a),   y(b),   z(v.x), w(v.y) { }
  v4(v3 v, float a) : x(v.x), y(v.y), z(v.z), w(a)   { }
  v4(float a, v3 v) : x(a),   y(v.x), z(v.y), w(v.z) { }
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

///////////////////////////////////////////////////////////////////////////////
// vector operations
///////////////////////////////////////////////////////////////////////////////

static v2 operator+(v2 a, v2 b) { return v2(a.x + b.x, a.y + b.y); }
static v3 operator+(v3 a, v3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static v4 operator+(v4 a, v4 b) { return v4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }

static v2 operator-(v2 a, v2 b) { return v2(a.x - b.x, a.y - b.y); }
static v3 operator-(v3 a, v3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static v4 operator-(v4 a, v4 b) { return v4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }

// Unary negation
static v2 operator-(v2 a) { return v2(-a.x, -a.y); }
static v3 operator-(v3 a) { return v3(-a.x, -a.y, -a.z); }
static v4 operator-(v4 a) { return v4(-a.x, -a.y, -a.z, -a.w); }

// Dot product
static float operator*(v2 a, v2 b) { return (a.x * b.x) + (a.y * b.y); }
static float operator*(v3 a, v3 b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }
static float operator*(v4 a, v4 b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w); }
static float dot(v2 a, v2 b) { return a * b; }
static float dot(v3 a, v3 b) { return a * b; }
static float dot(v4 a, v4 b) { return a * b; }

static v2 operator*(v2 a, float scalar) { return v2(a.x * scalar, a.y * scalar); }
static v3 operator*(v3 a, float scalar) { return v3(a.x * scalar, a.y * scalar, a.z * scalar); }
static v4 operator*(v4 a, float scalar) { return v4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar); }
static v2 operator*(float scalar, v2 a) { return v2(a.x * scalar, a.y * scalar); }
static v3 operator*(float scalar, v3 a) { return v3(a.x * scalar, a.y * scalar, a.z * scalar); }
static v4 operator*(float scalar, v4 a) { return v4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar); }

static v2 operator/(v2 a, float scalar) { return v2(a.x / scalar, a.y / scalar); }
static v3 operator/(v3 a, float scalar) { return v3(a.x / scalar, a.y / scalar, a.z / scalar); }
static v4 operator/(v4 a, float scalar) { return v4(a.x / scalar, a.y / scalar, a.z / scalar, a.w / scalar); }

static v2 &operator+=(v2 &a, v2 b) { a.x += b.x; a.y += b.y; return a; }
static v3 &operator+=(v3 &a, v3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
static v4 &operator+=(v4 &a, v4 b) { a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; return a; }

static v2 &operator-=(v2 &a, v2 b) { a.x -= b.x; a.y -= b.y; return a; }
static v3 &operator-=(v3 &a, v3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
static v4 &operator-=(v4 &a, v4 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; return a; }

static v2 &operator*=(v2 &a, float b) { a.x -= b; a.y -= b; return a; }
static v3 &operator*=(v3 &a, float b) { a.x -= b; a.y -= b; a.z -= b; return a; }
static v4 &operator*=(v4 &a, float b) { a.x -= b; a.y -= b; a.z -= b; a.w -= b; return a; }

static v2 &operator/=(v2 &a, float b) { a.x /= b; a.y /= b; return a; }
static v3 &operator/=(v3 &a, float b) { a.x /= b; a.y /= b; a.z /= b; return a; }
static v4 &operator/=(v4 &a, float b) { a.x /= b; a.y /= b; a.z /= b; a.w /= b; return a; }

// Gets the length of the vector
static float length(v2 v)
{
  return (float)sqrt(squared(v.x) + squared(v.y));
}
static float length(v3 v)
{
  return (float)sqrt(squared(v.x) + squared(v.y) + squared(v.z));
}
static float length(v4 v)
{
  return (float)sqrt(squared(v.x) + squared(v.y) + squared(v.z) + squared(v.w));
}

// Gets the squared length of this vector
static float length_squared(v2 v)
{
  return squared(v.x) + squared(v.y);
}
static float length_squared(v3 v)
{
  return squared(v.x) + squared(v.y) + squared(v.z);
}
static float length_squared(v4 v)
{
  return squared(v.x) + squared(v.y) + squared(v.z) + squared(v.w);
}

// Returns a unit vector from this vector.
static v2 unit(v2 v)
{
  return v / length(v);
}
static v3 unit(v3 v)
{
  return v / length(v);
}
static v4 unit(v4 v)
{
  return v / length(v);
}

// Returns this vector clamped by max length
static v2 clamp_length(v2 v, float max_length)
{
  float len = length(v);
  if(len > max_length)
  {
    v = v / len;
    v = v * max_length;
  }

  return v;
}
static v3 clamp_length(v3 v, float max_length)
{
  float len = length(v);
  if(len > max_length)
  {
    v = v / len;
    v = v * max_length;
  }

  return v;
}
static v4 clamp_length(v4 v, float max_length)
{
  float len = length(v);
  if(len > max_length)
  {
    v = v / len;
    v = v * max_length;
  }

  return v;
}


///////////////////////////////////////////////////////////////////////////////
// v2 specific operations
///////////////////////////////////////////////////////////////////////////////

// Returns a vector that is perpendicular to this vector. This specific
// normal will be rotated 90 degrees clockwise.
static v2 find_normal(v2 a)
{
  return v2(a.y, -a.x);
}

// Returns this vector rotated by the angle in radians
static v2 rotated(v2 a, float angle)
{
  v2 v;

  v.x = a.x * (float)cos(angle) - a.y * (float)sin(angle);
  v.y = a.x * (float)sin(angle) + a.y * (float)cos(angle);

  return v;
}

// Returns the angle this vector is pointing at in radians. If the vector is
// pointing straight right, the angle is 0. If left, the angle is PI / 2.0f
// etc.
//      PI/2
//       |
// PI <-- --> 0
//       |
//     -PI/2
static float angle(v2 a)
{
  return atan2f(a.y, a.x);
}


///////////////////////////////////////////////////////////////////////////////
// v3 specific operations
///////////////////////////////////////////////////////////////////////////////

static v3 cross(v3 a, v3 b)
{
  v3 v;
  v.x = (a.y * b.z) - (a.z * b.y);
  v.y = (a.z * b.x) - (a.x * b.z);
  v.z = (a.x * b.y) - (a.y * b.x);
  return v;
}







///////////////////////////////////////////////////////////////////////////////
// matrix structs
///////////////////////////////////////////////////////////////////////////////

struct mat4
{
  float m[4][4];

  mat4() : m { {1.0f, 0.0f, 0.0f, 0.0f},
               {0.0f, 1.0f, 0.0f, 0.0f},
               {0.0f, 0.0f, 1.0f, 0.0f},
               {0.0f, 0.0f, 0.0f, 1.0f} }
  {}
  
  mat4(float aa, float ab, float ac, float ad,
       float ba, float bb, float bc, float bd,
       float ca, float cb, float cc, float cd,
       float da, float db, float dc, float dd) :
    m { {aa, ab, ac, ad},
        {ba, bb, bc, bd},
        {ca, cb, cc, cd},
        {da, db, dc, dd} }
  {}

  const float *operator[](unsigned i) const
  {
    return m[i];
  }
  float *operator[](unsigned i)
  {
    return m[i];
  }
};

///////////////////////////////////////////////////////////////////////////////
// matrix operations
///////////////////////////////////////////////////////////////////////////////

// This funciton was made only for the matrix-vector multiplication
static float dot4v(const float *a, v4 b)
{
  return (a[0] * b.x) + (a[1] * b.y) + (a[2] * b.z) + (a[3] * b.w);
}
static v4 operator*(const mat4 &lhs, v4 rhs)
{
  v4 result;

  result.x = dot4v(lhs[0], rhs);
  result.y = dot4v(lhs[1], rhs);
  result.z = dot4v(lhs[2], rhs);
  result.w = dot4v(lhs[3], rhs);

  return result;
}

static mat4 operator*(const mat4 &lhs, const mat4 &rhs)
{
  mat4 product;

  // Loop through each spot in the resulting matrix
  for(unsigned row = 0; row < 4; row++)
  {
    for(unsigned col = 0; col < 4; col++)
    {
      // Dot the row and column for the given slot
      float dot = 0.0f;
      for(unsigned i = 0; i < 4; i++)
      {
        dot += lhs.m[row][i] * rhs.m[i][col];
      }

      product[row][col] = dot;
    }
  }

  return product;
}

static mat4 make_translation_matrix(v3 offset)
{
  mat4 result = 
  {
    1.0f, 0.0f, 0.0f, offset.x,
    0.0f, 1.0f, 0.0f, offset.y,
    0.0f, 0.0f, 1.0f, offset.z,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  return result;
}

static mat4 make_scale_matrix(v3 scale)
{
  mat4 result = 
  {
    scale.x, 0.0f, 0.0f, 0.0f,
    0.0f, scale.y, 0.0f, 0.0f,
    0.0f, 0.0f, scale.z, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  return result;
}

static mat4 make_x_axis_rotation_matrix(float radians)
{
  mat4 result = 
  {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, (float)cos(radians), (float)-sin(radians), 0.0f,
    0.0f, (float)sin(radians), (float)cos(radians), 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  return result;
}

static mat4 make_y_axis_rotation_matrix(float radians)
{
  mat4 result = 
  {
    (float)cos(radians), 0.0f, (float)sin(radians), 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    (float)-sin(radians), 0.0f, (float)cos(radians), 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  return result;
}

static mat4 make_z_axis_rotation_matrix(float radians)
{
  mat4 result = 
  {
    (float)cos(radians), (float)-sin(radians), 0.0f, 0.0f,
    (float)sin(radians), (float)cos(radians), 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  return result;
}
