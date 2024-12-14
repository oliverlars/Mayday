#include "maths.h"


/*

V2

*/

v2f AddV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X += B.X;
	Result.Y += B.X;
	return Result;
}

v2f SubV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X -= B.X;
	Result.Y -= B.X;
	return Result;
}

v2f MulV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X *= B.X;
	Result.Y *= B.X;
	return Result;
}

v2f DivV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X /= B.X;
	Result.Y /= B.X;
	return Result;
}

v2d AddV2D(v2d A, v2d B)
{
	v2d Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	return Result;
}

v2d SubV2D(v2d A, v2d B)
{
	v2d Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	return Result;
}

v2d MulV2D(v2d A, v2d B)
{
	v2d Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	return Result;
}

v2d DivV2D(v2d A, v2d B)
{
	v2d Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	return Result;
}

v2i AddV2I(v2i A, v2i B)
{
	v2i Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	return Result;
}

v2i SubV2I(v2i A, v2i B)
{
	v2i Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	return Result;
}

v2i MulV2I(v2i A, v2i B)
{
	v2i Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	return Result;
}

v2i DivV2I(v2i A, v2i B)
{
	v2i Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	return Result;
}

v2l AddV2L(v2l A, v2l B)
{
	v2l Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	return Result;
}

v2l SubV2L(v2l A, v2l B)
{
	v2l Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	return Result;
}

v2l MulV2L(v2l A, v2l B)
{
	v2l Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	return Result;
}

v2l DivV2L(v2l A, v2l B)
{
	v2l Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	return Result;
}

v2f V2FromF32(f32 X) { return (v2f){ X, X }; }
v2d V2FromF64(f64 X) { return (v2d){ X, X }; }
v2i V2FromS32(s32 X) { return (v2i){ X, X }; }
v2l V2FromS64(s64 X) { return (v2l){ X, X }; }


/*

V3

*/

v3f AddV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X += B.X;
	Result.Y += B.X;
	Result.Z += B.Z;
	return Result;
}

v3f SubV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X -= B.X;
	Result.Y -= B.X;
	Result.Z -= B.Z;
	return Result;
}

v3f MulV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X *= B.X;
	Result.Y *= B.X;
	Result.Z *= B.Z;
	return Result;
}

v3f DivV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X /= B.X;
	Result.Y /= B.X;
	Result.Z /= B.Z;
	return Result;
}

v3d AddV3D(v3d A, v3d B)
{
	v3d Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	return Result;
}

v3d SubV3D(v3d A, v3d B)
{
	v3d Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	return Result;
}

v3d MulV3D(v3d A, v3d B)
{
	v3d Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	return Result;
}

v3d DivV3D(v3d A, v3d B)
{
	v3d Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	return Result;
}

v3i AddV3I(v3i A, v3i B)
{
	v3i Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	return Result;
}

v3i SubV3I(v3i A, v3i B)
{
	v3i Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	return Result;
}

v3i MulV3I(v3i A, v3i B)
{
	v3i Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	return Result;
}

v3i DivV3I(v3i A, v3i B)
{
	v3i Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	return Result;
}

v3l AddV3L(v3l A, v3l B)
{
	v3l Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	return Result;
}

v3l SubV3L(v3l A, v3l B)
{
	v3l Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	return Result;
}

v3l MulV3L(v3l A, v3l B)
{
	v3l Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	return Result;
}

v3l DivV3L(v3l A, v3l B)
{
	v3l Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	return Result;
}

v3f V3FromF32(f32 X) { return (v3f){ X, X, X }; }
v3d V3FromF64(f64 X) { return (v3d){ X, X, X }; }
v3i V3FromS32(s32 X) { return (v3i){ X, X, X }; }
v3l V3FromS64(s64 X) { return (v3l){ X, X, X }; }

/*

V4

*/

v4f AddV4F(v4f A, v4f B)
{
	v4f Result = A;
	Result.X += B.X;
	Result.Y += B.X;
	Result.Z += B.Z;
	Result.W += B.W;
	return Result;
}

v4f SubV4F(v4f A, v4f B)
{
	v4f Result = A;
	Result.X -= B.X;
	Result.Y -= B.X;
	Result.Z -= B.Z;
	Result.W -= B.W;
	return Result;
}

v4f MulV4F(v4f A, v4f B)
{
	v4f Result = A;
	Result.X *= B.X;
	Result.Y *= B.X;
	Result.Z *= B.Z;
	Result.W *= B.W;
	return Result;
}

v4f DivV4F(v4f A, v4f B)
{
	v4f Result = A;
	Result.X /= B.X;
	Result.Y /= B.X;
	Result.Z /= B.Z;
	Result.W /= B.W;
	return Result;
}

v4d AddV4D(v4d A, v4d B)
{
	v4d Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	Result.W += B.W;
	return Result;
}

v4d SubV4D(v4d A, v4d B)
{
	v4d Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	Result.W -= B.W;
	return Result;
}

v4d MulV4D(v4d A, v4d B)
{
	v4d Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	Result.W *= B.W;
	return Result;
}

v4d DivV4D(v4d A, v4d B)
{
	v4d Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	Result.W /= B.W;
	return Result;
}

v4i AddV4I(v4i A, v4i B)
{
	v4i Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	Result.W += B.W;
	return Result;
}

v4i SubV4I(v4i A, v4i B)
{
	v4i Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	Result.W -= B.W;
	return Result;
}

v4i MulV4I(v4i A, v4i B)
{
	v4i Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	Result.W *= B.W;
	return Result;
}

v4i DivV4I(v4i A, v4i B)
{
	v4i Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	Result.W /= B.W;
	return Result;
}

v4l AddV4L(v4l A, v4l B)
{
	v4l Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	Result.W += B.W;
	return Result;
}

v4l SubV4L(v4l A, v4l B)
{
	v4l Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	Result.W -= B.W;
	return Result;
}

v4l MulV4L(v4l A, v4l B)
{
	v4l Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	Result.W *= B.W;
	return Result;
}

v4l DivV4L(v4l A, v4l B)
{
	v4l Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	Result.W /= B.W;
	return Result;
}

v4f V4FromF32(f32 X) { return (v4f){ X, X, X, X }; }
v4d V4FromF64(f64 X) { return (v4d){ X, X, X, X }; }
v4i V4FromS32(s32 X) { return (v4i){ X, X, X, X }; }
v4l V4FromS64(s64 X) { return (v4l){ X, X, X, X }; }

/*

M44

*/

m44f MulM44FV4F(m44f A, v4f B)
{
	m44f Result;

	Result.E[0][0] = A.E[0][0]*1.0f + A.E[0][1]*0.0f + A.E[0][2]*0.0f + A.E[0][3]*0.0f;
	Result.E[0][1] = A.E[0][0]*0.0f + A.E[0][1]*1.0f + A.E[0][2]*0.0f + A.E[0][3]*0.0f;
	Result.E[0][2] = A.E[0][0]*0.0f + A.E[0][1]*0.0f + A.E[0][2]*1.0f + A.E[0][3]*0.0f;
	Result.E[0][3] = A.E[0][0]*B.X  + A.E[0][1]*B.Y  + A.E[0][2]*B.Z  + A.E[0][3]*B.W;

	Result.E[1][0] = A.E[1][0]*1.0f + A.E[1][1]*0.0f + A.E[1][2]*0.0f + A.E[1][3]*0.0f;
	Result.E[1][1] = A.E[1][0]*0.0f + A.E[1][1]*1.0f + A.E[1][2]*0.0f + A.E[1][3]*0.0f;
	Result.E[1][2] = A.E[1][0]*0.0f + A.E[1][1]*0.0f + A.E[1][2]*1.0f + A.E[1][3]*0.0f;
	Result.E[1][3] = A.E[1][0]*B.X  + A.E[1][1]*B.Y  + A.E[1][2]*B.Z  + A.E[1][3]*B.W;

	Result.E[2][0] = A.E[2][0]*1.0f + A.E[2][1]*0.0f + A.E[2][2]*0.0f + A.E[2][3]*0.0f;
	Result.E[2][1] = A.E[2][0]*0.0f + A.E[2][1]*1.0f + A.E[2][2]*0.0f + A.E[2][3]*0.0f;
	Result.E[2][2] = A.E[2][0]*0.0f + A.E[2][1]*0.0f + A.E[2][2]*1.0f + A.E[2][3]*0.0f;
	Result.E[2][3] = A.E[2][0]*B.X  + A.E[2][1]*B.Y  + A.E[2][2]*B.Z  + A.E[2][3]*B.W;

	Result.E[3][0] = A.E[3][0]*1.0f + A.E[3][1]*0.0f + A.E[3][2]*0.0f + A.E[3][3]*0.0f;
	Result.E[3][1] = A.E[3][0]*0.0f + A.E[3][1]*1.0f + A.E[3][2]*0.0f + A.E[3][3]*0.0f;
	Result.E[3][2] = A.E[3][0]*0.0f + A.E[3][1]*0.0f + A.E[3][2]*1.0f + A.E[3][3]*0.0f;
	Result.E[3][3] = A.E[3][0]*B.X  + A.E[3][1]*B.Y  + A.E[3][2]*B.Z  + A.E[3][3]*B.W;

	return Result;
}

m44f MulM44FM44F(m44f A, m44f B)
{
 m44f Result;
	Result.E[0][0] = A.E[0][0]*B.E[0][0] + A.E[0][1]*B.E[1][0] + A.E[0][2]*B.E[2][0] + A.E[0][3]*B.E[3][0];
	Result.E[0][1] = A.E[0][0]*B.E[0][1] + A.E[0][1]*B.E[1][1] + A.E[0][2]*B.E[2][1] + A.E[0][3]*B.E[3][1];
	Result.E[0][2] = A.E[0][0]*B.E[0][2] + A.E[0][1]*B.E[1][2] + A.E[0][2]*B.E[2][2] + A.E[0][3]*B.E[3][2];
	Result.E[0][3] = A.E[0][0]*B.E[0][3] + A.E[0][1]*B.E[1][3] + A.E[0][2]*B.E[2][3] + A.E[0][3]*B.E[3][3];

	Result.E[1][0] = A.E[1][0]*B.E[0][0] + A.E[1][1]*B.E[1][0] + A.E[1][2]*B.E[2][0] + A.E[1][3]*B.E[3][0];
	Result.E[1][1] = A.E[1][0]*B.E[0][1] + A.E[1][1]*B.E[1][1] + A.E[1][2]*B.E[2][1] + A.E[1][3]*B.E[3][1];
	Result.E[1][2] = A.E[1][0]*B.E[0][2] + A.E[1][1]*B.E[1][2] + A.E[1][2]*B.E[2][2] + A.E[1][3]*B.E[3][2];
	Result.E[1][3] = A.E[1][0]*B.E[0][3] + A.E[1][1]*B.E[1][3] + A.E[1][2]*B.E[2][3] + A.E[1][3]*B.E[3][3];

	Result.E[2][0] = A.E[2][0]*B.E[0][0] + A.E[2][1]*B.E[1][0] + A.E[2][2]*B.E[2][0] + A.E[2][3]*B.E[3][0];
	Result.E[2][1] = A.E[2][0]*B.E[0][1] + A.E[2][1]*B.E[1][1] + A.E[2][2]*B.E[2][1] + A.E[2][3]*B.E[3][1];
	Result.E[2][2] = A.E[2][0]*B.E[0][2] + A.E[2][1]*B.E[1][2] + A.E[2][2]*B.E[2][2] + A.E[2][3]*B.E[3][2];
	Result.E[2][3] = A.E[2][0]*B.E[0][3] + A.E[2][1]*B.E[1][3] + A.E[2][2]*B.E[2][3] + A.E[2][3]*B.E[3][3];

	Result.E[3][0] = A.E[3][0]*B.E[0][0] + A.E[3][1]*B.E[1][0] + A.E[3][2]*B.E[2][0] + A.E[3][3]*B.E[3][0];
	Result.E[3][1] = A.E[3][0]*B.E[0][1] + A.E[3][1]*B.E[1][1] + A.E[3][2]*B.E[2][1] + A.E[3][3]*B.E[3][1];
	Result.E[3][2] = A.E[3][0]*B.E[0][2] + A.E[3][1]*B.E[1][2] + A.E[3][2]*B.E[2][2] + A.E[3][3]*B.E[3][2];
	Result.E[3][3] = A.E[3][0]*B.E[0][3] + A.E[3][1]*B.E[1][3] + A.E[3][2]*B.E[2][3] + A.E[3][3]*B.E[3][3];

	return Result;
}

m44d MulM44DV4D(m44d A, v4d B)
{
	m44d Result;

	Result.E[0][0] = A.E[0][0]*1.0f + A.E[0][1]*0.0f + A.E[0][2]*0.0f + A.E[0][3]*0.0f;
	Result.E[0][1] = A.E[0][0]*0.0f + A.E[0][1]*1.0f + A.E[0][2]*0.0f + A.E[0][3]*0.0f;
	Result.E[0][2] = A.E[0][0]*0.0f + A.E[0][1]*0.0f + A.E[0][2]*1.0f + A.E[0][3]*0.0f;
	Result.E[0][3] = A.E[0][0]*B.X  + A.E[0][1]*B.Y  + A.E[0][2]*B.Z  + A.E[0][3]*B.W;

	Result.E[1][0] = A.E[1][0]*1.0f + A.E[1][1]*0.0f + A.E[1][2]*0.0f + A.E[1][3]*0.0f;
	Result.E[1][1] = A.E[1][0]*0.0f + A.E[1][1]*1.0f + A.E[1][2]*0.0f + A.E[1][3]*0.0f;
	Result.E[1][2] = A.E[1][0]*0.0f + A.E[1][1]*0.0f + A.E[1][2]*1.0f + A.E[1][3]*0.0f;
	Result.E[1][3] = A.E[1][0]*B.X  + A.E[1][1]*B.Y  + A.E[1][2]*B.Z  + A.E[1][3]*B.W;

	Result.E[2][0] = A.E[2][0]*1.0f + A.E[2][1]*0.0f + A.E[2][2]*0.0f + A.E[2][3]*0.0f;
	Result.E[2][1] = A.E[2][0]*0.0f + A.E[2][1]*1.0f + A.E[2][2]*0.0f + A.E[2][3]*0.0f;
	Result.E[2][2] = A.E[2][0]*0.0f + A.E[2][1]*0.0f + A.E[2][2]*1.0f + A.E[2][3]*0.0f;
	Result.E[2][3] = A.E[2][0]*B.X  + A.E[2][1]*B.Y  + A.E[2][2]*B.Z  + A.E[2][3]*B.W;

	Result.E[3][0] = A.E[3][0]*1.0f + A.E[3][1]*0.0f + A.E[3][2]*0.0f + A.E[3][3]*0.0f;
	Result.E[3][1] = A.E[3][0]*0.0f + A.E[3][1]*1.0f + A.E[3][2]*0.0f + A.E[3][3]*0.0f;
	Result.E[3][2] = A.E[3][0]*0.0f + A.E[3][1]*0.0f + A.E[3][2]*1.0f + A.E[3][3]*0.0f;
	Result.E[3][3] = A.E[3][0]*B.X  + A.E[3][1]*B.Y  + A.E[3][2]*B.Z  + A.E[3][3]*B.W;

	return Result;
}

m44d MulM44DM44D(m44d A, m44d B)
{
 m44d Result;
	Result.E[0][0] = A.E[0][0]*B.E[0][0] + A.E[0][1]*B.E[1][0] + A.E[0][2]*B.E[2][0] + A.E[0][3]*B.E[3][0];
	Result.E[0][1] = A.E[0][0]*B.E[0][1] + A.E[0][1]*B.E[1][1] + A.E[0][2]*B.E[2][1] + A.E[0][3]*B.E[3][1];
	Result.E[0][2] = A.E[0][0]*B.E[0][2] + A.E[0][1]*B.E[1][2] + A.E[0][2]*B.E[2][2] + A.E[0][3]*B.E[3][2];
	Result.E[0][3] = A.E[0][0]*B.E[0][3] + A.E[0][1]*B.E[1][3] + A.E[0][2]*B.E[2][3] + A.E[0][3]*B.E[3][3];

	Result.E[1][0] = A.E[1][0]*B.E[0][0] + A.E[1][1]*B.E[1][0] + A.E[1][2]*B.E[2][0] + A.E[1][3]*B.E[3][0];
	Result.E[1][1] = A.E[1][0]*B.E[0][1] + A.E[1][1]*B.E[1][1] + A.E[1][2]*B.E[2][1] + A.E[1][3]*B.E[3][1];
	Result.E[1][2] = A.E[1][0]*B.E[0][2] + A.E[1][1]*B.E[1][2] + A.E[1][2]*B.E[2][2] + A.E[1][3]*B.E[3][2];
	Result.E[1][3] = A.E[1][0]*B.E[0][3] + A.E[1][1]*B.E[1][3] + A.E[1][2]*B.E[2][3] + A.E[1][3]*B.E[3][3];

	Result.E[2][0] = A.E[2][0]*B.E[0][0] + A.E[2][1]*B.E[1][0] + A.E[2][2]*B.E[2][0] + A.E[2][3]*B.E[3][0];
	Result.E[2][1] = A.E[2][0]*B.E[0][1] + A.E[2][1]*B.E[1][1] + A.E[2][2]*B.E[2][1] + A.E[2][3]*B.E[3][1];
	Result.E[2][2] = A.E[2][0]*B.E[0][2] + A.E[2][1]*B.E[1][2] + A.E[2][2]*B.E[2][2] + A.E[2][3]*B.E[3][2];
	Result.E[2][3] = A.E[2][0]*B.E[0][3] + A.E[2][1]*B.E[1][3] + A.E[2][2]*B.E[2][3] + A.E[2][3]*B.E[3][3];

	Result.E[3][0] = A.E[3][0]*B.E[0][0] + A.E[3][1]*B.E[1][0] + A.E[3][2]*B.E[2][0] + A.E[3][3]*B.E[3][0];
	Result.E[3][1] = A.E[3][0]*B.E[0][1] + A.E[3][1]*B.E[1][1] + A.E[3][2]*B.E[2][1] + A.E[3][3]*B.E[3][1];
	Result.E[3][2] = A.E[3][0]*B.E[0][2] + A.E[3][1]*B.E[1][2] + A.E[3][2]*B.E[2][2] + A.E[3][3]*B.E[3][2];
	Result.E[3][3] = A.E[3][0]*B.E[0][3] + A.E[3][1]*B.E[1][3] + A.E[3][2]*B.E[2][3] + A.E[3][3]*B.E[3][3];

	return Result;
}
