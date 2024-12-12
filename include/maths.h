#include "misc.h"

/*

V2

*/

typedef struct v2f { f32 X; f32 Y; } v2f;
#define v2f(...) (v2f){ VA_ARGS }
v2f AddV2F(v2f A, v2f B);
v2f SubV2F(v2f A, v2f B);
v2f MulV2F(v2f A, v2f B);
v2f DivV2F(v2f A, v2f B);

typedef struct v2d { f64 X; f64 Y; } v2d;
#define v2d(...) (v2d){ VA_ARGS }
v2d AddV2D(v2d A, v2d B);
v2d SubV2D(v2d A, v2d B);
v2d MulV2D(v2d A, v2d B);
v2d DivV2D(v2d A, v2d B);

typedef struct v2i { s32 X; s32 Y; } v2i;
#define v2i(...) (v2i){ VA_ARGS }
v2i AddV2I(v2i A, v2i B);
v2i SubV2I(v2i A, v2i B);
v2i MulV2I(v2i A, v2i B);
v2i DivV2I(v2i A, v2i B);

typedef struct v2l { s64 X; s64 Y; } v2l;
#define v2l(...) (v2l){ VA_ARGS }
v2l AddV2L(v2l A, v2l B);
v2l SubV2L(v2l A, v2l B);
v2l MulV2L(v2l A, v2l B);
v2l DivV2L(v2l A, v2l B);

v2f V2FromF32(f32 X) { return (v2f){ X, X }; }
v2d V2FromF64(f64 X) { return (v2d){ X, X }; }
v2i V2FromS32(s32 X) { return (v2i){ X, X }; }
v2l V2FromS64(s64 X) { return (v2l){ X, X }; }

/*

V3

*/

typedef struct v3f { f32 X; f32 Y; f32 Z; } v3f;
#define v3f(...) (v3f){ VA_ARGS }
v3f AddV3F(v3f A, v3f B);
v3f SubV3F(v3f A, v3f B);
v3f MulV3F(v3f A, v3f B);
v3f DivV3F(v3f A, v3f B);

typedef struct v3d { f64 X; f64 Y; f32 Z; } v3d;
#define v3d(...) (v3d){ VA_ARGS }
v3d AddV3D(v3d A, v3d B);
v3d SubV3D(v3d A, v3d B);
v3d MulV3D(v3d A, v3d B);
v3d DivV3D(v3d A, v3d B);

typedef struct v3i { s32 X; s32 Y; f32 Z; } v3i;
#define v3i(...) (v3i){ VA_ARGS }
v3i AddV3I(v3i A, v3i B);
v3i SubV3I(v3i A, v3i B);
v3i MulV3I(v3i A, v3i B);
v3i DivV3I(v3i A, v3i B);

typedef struct v3l { s64 X; s64 Y; f32 Z; } v3l;
#define v3l(...) (v3l){ VA_ARGS }
v3l AddV3L(v3l A, v3l B);
v3l SubV3L(v3l A, v3l B);
v3l MulV3L(v3l A, v3l B);
v3l DivV3L(v3l A, v3l B);

v3f V3FromF32(f32 X) { return (v3f){ X, X, X }; }
v3d V3FromF64(f64 X) { return (v3d){ X, X, X }; }
v3i V3FromS32(s32 X) { return (v3i){ X, X, X }; }
v3l V3FromS64(s64 X) { return (v3l){ X, X, X }; }


/*

V4

*/


typedef struct v4f { f32 X; f32 Y; f32 Z; f32 W; } v4f;
#define v4f(...) (v4f){ VA_ARGS }
v4f AddV4F(v4f A, v4f B);
v4f SubV4F(v4f A, v4f B);
v4f MulV4F(v4f A, v4f B);
v4f DivV4F(v4f A, v4f B);

typedef struct v4d { f64 X; f64 Y; f32 Z; f32 W; } v4d;
#define v4d(...) (v4d){ VA_ARGS }
v4d AddV4D(v4d A, v4d B);
v4d SubV4D(v4d A, v4d B);
v4d MulV4D(v4d A, v4d B);
v4d DivV4D(v4d A, v4d B);

typedef struct v4i { s32 X; s32 Y; f32 Z; f32 W; } v4i;
#define v4i(...) (v3i){ VA_ARGS }
v4i AddV4I(v4i A, v4i B);
v4i SubV4I(v4i A, v4i B);
v4i MulV4I(v4i A, v4i B);
v4i DivV4I(v4i A, v4i B);

typedef struct v4l { s64 X; s64 Y; f32 Z; f32 W; } v4l;
#define v4l(...) (v4l){ VA_ARGS }
v4l AddV4L(v4l A, v4l B);
v4l SubV4L(v4l A, v4l B);
v4l MulV4L(v4l A, v4l B);
v4l DivV4L(v4l A, v4l B);

v4f V4FromF32(f32 X) { return (v4f){ X, X, X, X }; }
v4d V4FromF64(f64 X) { return (v4d){ X, X, X, X }; }
v4i V4FromS32(s32 X) { return (v4i){ X, X, X, X }; }
v4l V4FromS64(s64 X) { return (v4l){ X, X, X, X }; }


/*

M4

*/

typedef struct m44f { f32 E[4][4]; } m44f;
#define v3f(...) (v3f){ VA_ARGS }
m44f MulM44FV3F(m44f A, v3f B);
m44f MulM44FM44F(m44f A, m44f B);

typedef union m44d { f64 E[4][4]; } m44d;
#define v3d(...) (v3d){ VA_ARGS }
m44d MulM44DV3D(m44d A, v3d B);
m44d MulM44DM44D(m44d A, m44d B);