#pragma once

#include "misc.h"

#include "strings.h"

#define M_FNV_64_PRIME ((u64)0x100000001b3ULL)
#define M_FNV_HASH_START ((u64)0xcbf29ce484222325ULL)
u64 M_FNVHashString(u64 Hash, str8 String);
u64 M_FNVHashBytes(u64 Hash, u8 *Bytes, s32 ByteCount);

/*

V2

*/

typedef struct v2f { f32 X; f32 Y; } v2f;
#define v2f(...) (v2f){ __VA_ARGS__ }
v2f AddV2F(v2f A, v2f B);
v2f SubV2F(v2f A, v2f B);
v2f MulV2F(v2f A, v2f B);
v2f DivV2F(v2f A, v2f B);

typedef struct v2d { f64 X; f64 Y; } v2d;
#define v2d(...) (v2d){ __VA_ARGS__ }
v2d AddV2D(v2d A, v2d B);
v2d SubV2D(v2d A, v2d B);
v2d MulV2D(v2d A, v2d B);
v2d DivV2D(v2d A, v2d B);

typedef struct v2i { s32 X; s32 Y; } v2i;
#define v2i(...) (v2i){ __VA_ARGS__ }
v2i AddV2I(v2i A, v2i B);
v2i SubV2I(v2i A, v2i B);
v2i MulV2I(v2i A, v2i B);
v2i DivV2I(v2i A, v2i B);

typedef struct v2l { s64 X; s64 Y; } v2l;
#define v2l(...) (v2l){ __VA_ARGS__ }
v2l AddV2L(v2l A, v2l B);
v2l SubV2L(v2l A, v2l B);
v2l MulV2L(v2l A, v2l B);
v2l DivV2L(v2l A, v2l B);

v2f V2FromF32(f32 X);
v2d V2FromF64(f64 X);
v2i V2FromS32(s32 X);
v2l V2FromS64(s64 X);

/*

V3

*/

typedef struct v3f { f32 X; f32 Y; f32 Z; } v3f;
#define v3f(...) (v3f){ __VA_ARGS__ }
v3f AddV3F(v3f A, v3f B);
v3f SubV3F(v3f A, v3f B);
v3f MulV3F(v3f A, v3f B);
v3f DivV3F(v3f A, v3f B);
f32 LengthV3F(v3f V);
v3f NormalizeV3F(v3f V);
v3f CrossV3F(v3f A, v3f B);
f32 DotV3F(v3f A, v3f B);

typedef struct v3d { f64 X; f64 Y; f32 Z; } v3d;
#define v3d(...) (v3d){ __VA_ARGS__ }
v3d AddV3D(v3d A, v3d B);
v3d SubV3D(v3d A, v3d B);
v3d MulV3D(v3d A, v3d B);
v3d DivV3D(v3d A, v3d B);
f64 LengthV3D(v3d V);
v3d NormalizeV3D(v3d V);
v3d CrossV3D(v3d A, v3d B);
f32 DotV3D(v3d A, v3d B);

typedef struct v3i { s32 X; s32 Y; f32 Z; } v3i;
#define v3i(...) (v3i){ __VA_ARGS__ }
v3i AddV3I(v3i A, v3i B);
v3i SubV3I(v3i A, v3i B);
v3i MulV3I(v3i A, v3i B);
v3i DivV3I(v3i A, v3i B);

typedef struct v3l { s64 X; s64 Y; f32 Z; } v3l;
#define v3l(...) (v3l){ __VA_ARGS__ }
v3l AddV3L(v3l A, v3l B);
v3l SubV3L(v3l A, v3l B);
v3l MulV3L(v3l A, v3l B);
v3l DivV3L(v3l A, v3l B);

v3f V3FromF32(f32 X);
v3d V3FromF64(f64 X);
v3i V3FromS32(s32 X);
v3l V3FromS64(s64 X);


/*

V4

*/


typedef struct v4f { f32 X; f32 Y; f32 Z; f32 W; } v4f;
#define v4f(...) (v4f){ __VA_ARGS__ }
v4f AddV4F(v4f A, v4f B);
v4f SubV4F(v4f A, v4f B);
v4f MulV4F(v4f A, v4f B);
v4f DivV4F(v4f A, v4f B);
f64 LengthV4F(v4f V);
v4f NormalizeV4F(v4f V);

typedef struct v4d { f64 X; f64 Y; f32 Z; f32 W; } v4d;
#define v4d(...) (v4d){ __VA_ARGS__ }
v4d AddV4D(v4d A, v4d B);
v4d SubV4D(v4d A, v4d B);
v4d MulV4D(v4d A, v4d B);
v4d DivV4D(v4d A, v4d B);
f64 LengthV4D(v4d V);
v4d NormalizeV4D(v4d V);

typedef struct v4i { s32 X; s32 Y; s32 Z; s32 W; } v4i;
#define v4i(...) (v3i){ __VA_ARGS__ }
v4i AddV4I(v4i A, v4i B);
v4i SubV4I(v4i A, v4i B);
v4i MulV4I(v4i A, v4i B);
v4i DivV4I(v4i A, v4i B);

typedef struct v4l { s64 X; s64 Y; f32 Z; f32 W; } v4l;
#define v4l(...) (v4l){ __VA_ARGS__ }
v4l AddV4L(v4l A, v4l B);
v4l SubV4L(v4l A, v4l B);
v4l MulV4L(v4l A, v4l B);
v4l DivV4L(v4l A, v4l B);

v4f V4FromF32(f32 X);
v4d V4FromF64(f64 X);
v4i V4FromS32(s32 X);
v4l V4FromS64(s64 X);


/*

M4

*/

typedef struct m44f { f32 E[4][4]; } m44f;
m44f MulM44FV3F(m44f A, v3f B);
m44f MulM44FM44F(m44f A, m44f B);

m44f M44FTranspose(m44f M);

m44f M44FromXAngle(f32 Angle);
m44f M44FromYAngle(f32 Angle);
m44f M44FromZAngle(f32 Angle);

m44f M44FPerspective(f32 Aspect, f32 FOV, f32 Near, f32 Far);
m44f M44FLookAt(v3f At, v3f Position, v3f Up);

m44f M44FTranslate(v3f Translate);
m44f M44FScale(v3f Scale);

extern m44f M44F_IDENTITY;

typedef union m44d { f64 E[4][4]; } m44d;
m44d MulM44DV3D(m44d A, v3d B);
m44d MulM44DM44D(m44d A, m44d B);