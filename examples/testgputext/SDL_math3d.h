/**
 * Define SDL_MATH_3D_IMPLEMENTATION before including this header in any one source (.c) files.
 * Angles are in radians unless specified otherwise.
**/

#ifndef SDL_MATH_3D_H
#define SDL_MATH_3D_H

#include <SDL3/SDL_stdinc.h>

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float x;
    float y;
} SDL_Vec2;

typedef struct
{
    float x;
    float y;
    float z;
} SDL_Vec3;

typedef struct
{
    float x;
    float y;
    float z;
    float w;
} SDL_Vec4;

/**
 * The matrix is stored in column major format
 **/
typedef struct
{
    union
    {
        struct
        {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        } v;
        float m[4][4];
    };
} SDL_Mat4X4;


// SDL Vector functions
static inline SDL_Vec3 SDL_Vector3(float x, float y, float z);
static inline float    SDL_Vec3Magnitude(SDL_Vec3 vec);
static inline SDL_Vec3 SDL_Vec3Normalize(SDL_Vec3 vec);
static inline SDL_Vec3 SDL_Vec3Add(SDL_Vec3 vec1, SDL_Vec3 vec2);
static inline SDL_Vec3 SDL_Vec3Sub(SDL_Vec3 vec1, SDL_Vec3 vec2);
static inline SDL_Vec3 SDL_Vec3MultiplyFloat(SDL_Vec3 vec, float val);
static inline float    SDL_Vec3Dot(SDL_Vec3 vec1, SDL_Vec3 vec2);
static inline SDL_Vec3 SDL_Vec3Cross(SDL_Vec3 vec1, SDL_Vec3 vec2);

// SDL Matrix functions
static inline SDL_Mat4X4 SDL_Matrix4X4(
    float m00, float m10, float m20, float m30,
    float m01, float m11, float m21, float m31,
    float m02, float m12, float m22, float m32,
    float m03, float m13, float m23, float m33
);
static inline SDL_Mat4X4 SDL_MatrixIdentity(void);
static inline SDL_Mat4X4 SDL_MatrixTranspose(SDL_Mat4X4 mat);
static inline SDL_Mat4X4 SDL_MatrixMultiply(SDL_Mat4X4 mat1, SDL_Mat4X4 mat2);
static inline SDL_Mat4X4 SDL_MatrixScaling(SDL_Vec3 scale);
static inline SDL_Mat4X4 SDL_MatrixTranslation(SDL_Vec3 offset);
static inline SDL_Mat4X4 SDL_MatrixRotationX(float angle);
static inline SDL_Mat4X4 SDL_MatrixRotationY(float angle);
static inline SDL_Mat4X4 SDL_MatrixRotationZ(float angle);

static inline SDL_Mat4X4 SDL_MatrixOrtho(float left, float right, float bottom, float top, float back, float front);
static inline SDL_Mat4X4 SDL_MatrixPerspective(float fovy, float aspect_ratio, float near, float far);
static inline SDL_Mat4X4 SDL_MatrixLookAt(SDL_Vec3 pos, SDL_Vec3 target, SDL_Vec3 up);


// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif
#endif /* SDL_MATH_3D_H */


#ifdef SDL_MATH_3D_IMPLEMENTATION

static inline SDL_Vec3 SDL_Vector3(float x, float y, float z) {
    return (SDL_Vec3) {.x = x, .y = y, .z = z};
}

static inline float SDL_Vec3Magnitude(SDL_Vec3 vec)
{
    return SDL_sqrtf(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);
}


static inline SDL_Vec3 SDL_Vec3Normalize(SDL_Vec3 vec)
{
    float mag = SDL_Vec3Magnitude(vec);

    if (mag == 0) {
        return (SDL_Vec3) {0, 0, 0};
    } else if (mag == 1){
        return vec;
    } else {
        return (SDL_Vec3) {vec.x/mag, vec.y/mag, vec.z/mag};
    }
}

static inline SDL_Vec3 SDL_Vec3Add(SDL_Vec3 vec1, SDL_Vec3 vec2)
{
    return SDL_Vector3(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z);
}

static inline SDL_Vec3 SDL_Vec3Sub(SDL_Vec3 vec1, SDL_Vec3 vec2)
{
    return SDL_Vector3(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z);
}

static inline SDL_Vec3 SDL_Vec3MultiplyFloat(SDL_Vec3 vec, float val)
{
    return SDL_Vector3(vec.x * val, vec.y * val, vec.z * val);
}

static inline float SDL_Vec3Dot(SDL_Vec3 vec1, SDL_Vec3 vec2)
{
    return (vec1.x * vec2.x + vec1.y * vec2.y + vec1.y * vec2.y);
}

static inline SDL_Vec3 SDL_Vec3Cross(SDL_Vec3 vec1, SDL_Vec3 vec2) {
    return SDL_Vector3(
        vec1.y * vec2.z - vec1.z * vec2.y,
        vec1.z * vec2.x - vec1.x * vec2.z,
        vec1.x * vec2.y - vec1.y * vec2.x
    );
}


static inline SDL_Mat4X4 SDL_Matrix4X4(
    float m00, float m10, float m20, float m30,
    float m01, float m11, float m21, float m31,
    float m02, float m12, float m22, float m32,
    float m03, float m13, float m23, float m33
) {
    return (SDL_Mat4X4) {
        .m[0][0] = m00, .m[1][0] = m10, .m[2][0] = m20, .m[3][0] = m30,
        .m[0][1] = m01, .m[1][1] = m11, .m[2][1] = m21, .m[3][1] = m31,
        .m[0][2] = m02, .m[1][2] = m12, .m[2][2] = m22, .m[3][2] = m32,
        .m[0][3] = m03, .m[1][3] = m13, .m[2][3] = m23, .m[3][3] = m33
    };
}

static inline SDL_Mat4X4 SDL_MatrixIdentity(void)
{
    return SDL_Matrix4X4(
        1,  0,  0,  0,
        0,  1,  0,  0,
        0,  0,  1,  0,
        0,  0,  0,  1
    );
}

static inline SDL_Mat4X4 SDL_MatrixTranspose(SDL_Mat4X4 mat)
{
    SDL_Mat4X4 res;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            res.m[i][j] = mat.m[i][j];
        }
    }

    return res;
}

static inline SDL_Mat4X4 SDL_MatrixMultiply(SDL_Mat4X4 mat1, SDL_Mat4X4 mat2)
{
    SDL_Mat4X4 res;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int x = 0; x < 4; x++) {
                sum += mat1.m[x][j] * mat2.m[i][x];
            }
            res.m[i][j] = sum;
        }
    }
    
    return res;
}

static inline SDL_Mat4X4 SDL_MatrixScaling(SDL_Vec3 scale)
{
    float x = scale.x, y = scale.y, z = scale.z;
    return SDL_Matrix4X4(
         x,  0,  0,  0,
         0,  y,  0,  0,
         0,  0,  z,  0,
         0,  0,  0,  1
    );
}

static inline SDL_Mat4X4 SDL_MatrixTranslation(SDL_Vec3 offset)
{
    return SDL_Matrix4X4(
         1,  0,  0,  offset.x,
         0,  1,  0,  offset.y,
         0,  0,  1,  offset.z,
         0,  0,  0,         1
    );
}

static inline SDL_Mat4X4 SDL_MatrixRotationX(float angle)
{
    float cos = SDL_cos(angle);
    float sin = SDL_sin(angle);

    return SDL_Matrix4X4(
         1,    0,    0,  0,
         0,  cos, -sin,  0,
         0,  sin,  cos,  0,
         0,    0,    0,  1
    );
}

static inline SDL_Mat4X4 SDL_MatrixRotationY(float angle)
{
    float cos = SDL_cos(angle);
    float sin = SDL_sin(angle);

    return SDL_Matrix4X4(
         cos,  0,  sin,  0,
           0,  1,    0,  0,
        -sin,  0,  cos,  0,
           0,  0,    0,  1
    );
}

static inline SDL_Mat4X4 SDL_MatrixRotationZ(float angle)
{
    float cos = SDL_cos(angle);
    float sin = SDL_sin(angle);

    return SDL_Matrix4X4(
         cos, -sin,  0,  0,
         sin,  cos,  0,  0,
           0,    0,  1,  0,
           0,    0,  0,  1
    );
}

static inline SDL_Mat4X4 SDL_MatrixOrtho(float left, float right, float bottom, float top, float near, float far)
{
    float l = left, r = right, b = bottom, t = top, n = near, f = far;

    float dx = -(r + l) / (r - l);
    float dy = -(t + b) / (t - b);
    float dz = -(f + n) / (f - n);

    return SDL_Matrix4X4(
         2 / (r - l),            0,            0,  dx,
                   0,  2 / (t - b),            0,  dy,
                   0,            0,  2 / (f - n),  dz,
                   0,            0,            0,   1
    );
}

static inline SDL_Mat4X4 SDL_MatrixPerspective(float fovy, float aspect_ratio, float near, float far)
{
    float n = near; float f = far;
    float t = SDL_tanf(fovy/2.0f) * n;
    float b = -t;
    float r = t * aspect_ratio;
    float l = -r;

    return SDL_Matrix4X4(
         (2 * n) / (r - l),                  0,  (r + l) / (r - l),                      0,
                         0,  (2 * n) / (t - b),  (t + b) / (t - b),                      0,
                         0,                  0, -(f + n) / (f - n), -(2 * n * f) / (f - n),
                         0,                  0,                 -1,                      1
    );
}

static inline SDL_Mat4X4 SDL_MatrixLookAt(SDL_Vec3 pos, SDL_Vec3 target, SDL_Vec3 up)
{
    SDL_Vec3 d = SDL_Vec3Normalize(SDL_Vec3Sub(target, pos));
    SDL_Vec3 u = SDL_Vec3Normalize(up);
    SDL_Vec3 r = SDL_Vec3Normalize(SDL_Vec3Cross(u, d));
    u = SDL_Vec3Cross(r, d);

    return SDL_Matrix4X4(
         r.x,  r.y,  r.z, -SDL_Vec3Dot(r, pos),
         u.x,  u.y,  u.z, -SDL_Vec3Dot(u, pos),
        -d.x, -d.y, -d.z,  SDL_Vec3Dot(d, pos),
           0,    0,    0,  1
    );
}

#endif /* SDL_MATH_3D_IMPLEMENTATION */
