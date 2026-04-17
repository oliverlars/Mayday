#include "maths.h"
#include "strings.h"

#define _USE_MATH_DEFINES
#include <math.h>

random32_state XorShiftInit()
{
	return (random32_state){ .State = XORSHIFT_STATE };
}

u32 XorShift32(random32_state *State)
{
	State->State ^= State->State << 13;
	State->State ^= State->State >> 17;
	State->State ^= State->State << 5;
	return State->State;
}

float RandomBilateral(random32_state *State)
{
	float Result = ((float)XorShift32(State) / (float)UINT32_MAX);

	return Result * 2.0f - 1.0f;
}

float RandomUnilateral(random32_state *State)
{
	float Result = ((float)XorShift32(State) / (float)UINT32_MAX);

	return Result;
}

u64 M_FNVHashString(u64 Hash, str8 String)
{
	for (s64 Index = 0; Index < String.Size; Index++)
	{

		Hash *= M_FNV_64_PRIME;
		Hash ^= (u64)String.Bytes[Index];
	}
	return Hash;
}

u64 M_FNVHashBytes(u64 Hash, u8 *Bytes, int ByteCount)
{
	str8 BytesAsString = (str8){ByteCount, Bytes};
	return M_FNVHashString(Hash, BytesAsString);
}

f32 ClampF32(f32 Value, f32 Low, f32 High)
{
	f32 Result = Value;
	if(Value < Low)
	{
		Result = Low;
	}
	if(Value > High)
	{
		Result = High;
	}
	return Result;
}

s32 ClampS32(s32 Value, s32 Low, s32 High)
{
	s32 Result = Value;
	if(Value < Low)
	{
		Result = Low;
	}
	if(Value > High)
	{
		Result = High;
	}
	return Result;
}

f32 LerpF32(f32 Source, f32 Target, float T)
{
	return Source + T * (Target - Source);
}

/*

V2

*/

v2f AddV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	return Result;
}

v2f SubV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	return Result;
}

v2f MulV2F(v2f A, v2f B)
{
	v2f Result;
	Result.X = A.X * B.X;
	Result.Y = A.Y * B.Y;
	return Result;
}

v2f DivV2F(v2f A, v2f B)
{
	v2f Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	return Result;
}

v2f LerpV2F(v2f Source, v2f Target, float T)
{
	v2f Result;
	Result.X = Source.X + T * (Target.X - Source.X);
	Result.Y = Source.Y + T * (Target.Y - Source.Y);
	return Result;
}

f32 LengthV2F(v2f V)
{
	return sqrtf(DotV2F(V, V));
}

v2f NormalizeV2F(v2f V)
{
	f32 L = LengthV2F(V);
	return DivV2F(V, v2f(L, L));
}

f32 DotV2F(v2f A, v2f B)
{
	return A.X*B.X + A.Y*B.Y;
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

v2f V2FromF32(f32 X) { return (v2f){ X, X }; }
v2i V2FromS32(s32 X) { return (v2i){ X, X }; }

/*

V3

*/

v3f AddV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X += B.X;
	Result.Y += B.Y;
	Result.Z += B.Z;
	return Result;
}

v3f SubV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X -= B.X;
	Result.Y -= B.Y;
	Result.Z -= B.Z;
	return Result;
}

v3f MulV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X *= B.X;
	Result.Y *= B.Y;
	Result.Z *= B.Z;
	return Result;
}

v3f DivV3F(v3f A, v3f B)
{
	v3f Result = A;
	Result.X /= B.X;
	Result.Y /= B.Y;
	Result.Z /= B.Z;
	return Result;
}

v3f LerpV3F(v3f Source, v3f Target, float T)
{
	v3f Result;
	Result.X = Source.X + T * (Target.X - Source.X);
	Result.Y = Source.Y + T * (Target.Y - Source.Y);
	Result.Z = Source.Z + T * (Target.Z - Source.Z);
	return Result;
}

f32 LengthV3F(v3f V)
{
	return sqrtf(DotV3F(V, V));
}

v3f NormalizeV3F(v3f V)
{
	f32 L = LengthV3F(V);
	return DivV3F(V, v3f(L, L, L));
}

v3f CrossV3F(v3f A, v3f B)
{
	v3f Result = {0};
	Result.X = A.Y * B.Z - A.Z * B.Y;
	Result.Y = A.Z * B.X - A.X * B.Z;
	Result.Z = A.X * B.Y - A.Y * B.X;
	return Result;
}

f32 DotV3F(v3f A, v3f B)
{
	return A.X*B.X + A.Y*B.Y + A.Z*B.Z;
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
v3f V3FromF32(f32 X) { return (v3f){ X, X, X }; }
v3i V3FromS32(s32 X) { return (v3i){ X, X, X }; }

/*

V4

*/

v4f ExpandV4F(v4f V, f32 Amount)
{
	v4f Result = V;
	Result.X -= Amount/2.0;
	Result.Y -= Amount/2.0;
	Result.Z += Amount;
	Result.W += Amount;
	return Result;
}

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

v4f V4FromF32(f32 X) { return (v4f){ X, X, X, X }; }
v4i V4FromS32(s32 X) { return (v4i){ X, X, X, X }; }

/*

M44

*/

m44f M44F_IDENTITY = 
{
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

v4f MulM44FV4F(m44f A, v4f B)
{
	v4f Result;

	Result.X = A.E[0][0] * B.X + A.E[0][1] * B.Y + A.E[0][2] * B.Z + A.E[0][3] * B.W;
	Result.Y = A.E[1][0] * B.X + A.E[1][1] * B.Y + A.E[1][2] * B.Z + A.E[1][3] * B.W;
	Result.Z = A.E[2][0] * B.X + A.E[2][1] * B.Y + A.E[2][2] * B.Z + A.E[2][3] * B.W;
	Result.W = A.E[3][0] * B.X + A.E[3][1] * B.Y + A.E[3][2] * B.Z + A.E[3][3] * B.W;

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

m44f M44FTranspose(m44f M)
{
	m44f Result = (m44f)
	{
		M.E[0][0], M.E[1][0], M.E[2][0], M.E[3][0],
		M.E[0][1], M.E[1][1], M.E[2][1], M.E[3][1],
		M.E[0][2], M.E[1][2], M.E[2][2], M.E[3][2],
		M.E[0][3], M.E[1][3], M.E[2][3], M.E[3][3]
	};
	return Result;
}

m44f M44FromXAngle(f32 Angle)
{
	m44f Result = M44F_IDENTITY;

	Result.E[1][1] = cos(Angle);
	Result.E[1][2] = -sin(Angle);
	Result.E[2][1] = sin(Angle);
	Result.E[2][2] = cos(Angle);

	return Result;
}

m44f M44FromYAngle(f32 Angle)
{
	m44f Result = M44F_IDENTITY;

	Result.E[0][0] = cos(Angle);
	Result.E[0][2] = -sin(Angle);
	Result.E[2][0] = sin(Angle);
	Result.E[2][2] = cos(Angle);

	return Result;
}

m44f M44FromZAngle(f32 Angle)
{
	m44f Result = M44F_IDENTITY;

	Result.E[0][0] = cos(Angle);
	Result.E[0][1] = -sin(Angle);
	Result.E[1][0] = sin(Angle);
	Result.E[1][1] = cos(Angle);

	return Result;
}

m44f M44FPerspective(f32 Aspect, f32 FOV, f32 Near, f32 Far)
{
	FOV = (FOV/360.0)*2*M_PI;
	m44f Result = M44F_IDENTITY;
	Result.E[0][0] = 1.0f / (Aspect * tanf(FOV / 2.0f));
	Result.E[1][1] = 1.0f / tanf(FOV / 2.0f);
	Result.E[2][2] = -(Far + Near) / (Far - Near);
	Result.E[2][3] = -1.0f;
	Result.E[3][2] = -2.0f * (Far * Near) / (Far - Near);
	Result.E[3][3] = 0.0f;

	return Result;
}

m44f M44FOrthographic(f32 Left, f32 Right,
					  f32 Top,  f32 Bottom,
					  f32 Near, f32 Far)
{
	m44f Result = M44F_IDENTITY;
	Result.E[0][0] =  2.0/(Right - Left);
	Result.E[1][1] =  2.0/(Top - Bottom);
	Result.E[2][2] = -2.0/(Far - Near);
	Result.E[3][0] = -(Right + Left)/(Right - Left);
	Result.E[3][1] = -(Top + Bottom)/(Top - Bottom);
	Result.E[3][2] = -(Far + Near)/(Far - Near);

	return Result;
}

m44f M44FLookAt(v3f From, v3f To, v3f ArbitraryUp)
{
	v3f Forward = NormalizeV3F(SubV3F(From, To));
	v3f Right = NormalizeV3F(CrossV3F(ArbitraryUp, Forward));
	v3f Up = CrossV3F(Forward, Right);


	v3f Translation = (v3f)
	{
		DotV3F(Right, From),
		DotV3F(Up, From),
		DotV3F(Forward, From),

	};
	m44f Result = (m44f)
	{
		Right.X, Up.X, Forward.X, 0,
		Right.Y, Up.Y, Forward.Y, 0,
		Right.Z, Up.Z, Forward.Z, 0,
		-Translation.X, -Translation.Y, -Translation.Z, 1,
	};

	return Result;
}

m44f M44FTranslate(v3f Translate)
{
	m44f Result = M44F_IDENTITY;

	Result.E[3][0] = Translate.X;
	Result.E[3][1] = Translate.Y;
	Result.E[3][2] = Translate.Z;

	return Result;
}

m44f M44FScale(v3f Scale)
{
	m44f Result = M44F_IDENTITY;

	Result.E[0][0] = Scale.X;
	Result.E[1][1] = Scale.Y;
	Result.E[2][2] = Scale.Z;

	return Result;
}

m44f M44FInverse(m44f Matrix)
{
	m44f Result = M44F_IDENTITY;
	float Inverse[16] = {0};
	float Determinant = 1.0;
	float Flattened[16] =
	{
		Matrix.E[0][0], Matrix.E[0][1], Matrix.E[0][2], Matrix.E[0][3],
		Matrix.E[1][0], Matrix.E[1][1], Matrix.E[1][2], Matrix.E[1][3],
		Matrix.E[2][0], Matrix.E[2][1], Matrix.E[2][2], Matrix.E[2][3],
		Matrix.E[3][0], Matrix.E[3][1], Matrix.E[3][2], Matrix.E[3][3],
	};

	Inverse[0] = Flattened[5]  * Flattened[10] * Flattened[15]
			   - Flattened[5]  * Flattened[11] * Flattened[14]
			   - Flattened[9]  * Flattened[6]  * Flattened[15]
			   + Flattened[9]  * Flattened[7]  * Flattened[14]
			   + Flattened[13] * Flattened[6]  * Flattened[11]
			   - Flattened[13] * Flattened[7]  * Flattened[10];

	Inverse[4] = -Flattened[4] * Flattened[10] * Flattened[15]
			   + Flattened[4]  * Flattened[11] * Flattened[14]
			   + Flattened[8]  * Flattened[6]  * Flattened[15]
			   - Flattened[8]  * Flattened[7]  * Flattened[14]
			   - Flattened[12] * Flattened[6]  * Flattened[11]
			   + Flattened[12] * Flattened[7]  * Flattened[10];

	Inverse[8] = Flattened[4]  * Flattened[9]  * Flattened[15]
			   - Flattened[4]  * Flattened[11] * Flattened[13]
			   - Flattened[8]  * Flattened[5]  * Flattened[15]
			   + Flattened[8]  * Flattened[7]  * Flattened[13]
			   + Flattened[12] * Flattened[5]  * Flattened[11]
			   - Flattened[12] * Flattened[7]  * Flattened[9];

	Inverse[12] = -Flattened[4] * Flattened[9]  * Flattened[14]
				+ Flattened[4]  * Flattened[10] * Flattened[13]
				+ Flattened[8]  * Flattened[5]  * Flattened[14]
				- Flattened[8]  * Flattened[6]  * Flattened[13]
				- Flattened[12] * Flattened[5]  * Flattened[10]
				+ Flattened[12] * Flattened[6]  * Flattened[9];

	Inverse[1] = -Flattened[1] * Flattened[10] * Flattened[15]
			   + Flattened[1]  * Flattened[11] * Flattened[14]
			   + Flattened[9]  * Flattened[2]  * Flattened[15]
			   - Flattened[9]  * Flattened[3]  * Flattened[14]
			   - Flattened[13] * Flattened[2]  * Flattened[11]
			   + Flattened[13] * Flattened[3]  * Flattened[10];

	Inverse[5] =  Flattened[0] * Flattened[10] * Flattened[15]
			   - Flattened[0]  * Flattened[11] * Flattened[14]
			   - Flattened[8]  * Flattened[2]  * Flattened[15]
			   + Flattened[8]  * Flattened[3]  * Flattened[14]
			   + Flattened[12] * Flattened[2]  * Flattened[11]
			   - Flattened[12] * Flattened[3]  * Flattened[10];

	Inverse[9] = -Flattened[0] * Flattened[9]  * Flattened[15]
			   + Flattened[0]  * Flattened[11] * Flattened[13]
			   + Flattened[8]  * Flattened[1]  * Flattened[15]
			   - Flattened[8]  * Flattened[3]  * Flattened[13]
			   - Flattened[12] * Flattened[1]  * Flattened[11]
			   + Flattened[12] * Flattened[3]  * Flattened[9];

	Inverse[13] = Flattened[0]  * Flattened[9]  * Flattened[14]
				- Flattened[0]  * Flattened[10] * Flattened[13]
				- Flattened[8]  * Flattened[1]  * Flattened[14]
				+ Flattened[8]  * Flattened[2]  * Flattened[13]
				+ Flattened[12] * Flattened[1]  * Flattened[10]
				- Flattened[12] * Flattened[2]  * Flattened[9];

	Inverse[2] = Flattened[1]  * Flattened[6] * Flattened[15]
			   - Flattened[1]  * Flattened[7] * Flattened[14]
			   - Flattened[5]  * Flattened[2] * Flattened[15]
			   + Flattened[5]  * Flattened[3] * Flattened[14]
			   + Flattened[13] * Flattened[2] * Flattened[7]
			   - Flattened[13] * Flattened[3] * Flattened[6];

	Inverse[6] = -Flattened[0]  * Flattened[6] * Flattened[15]
			   +  Flattened[0]  * Flattened[7] * Flattened[14]
			   +  Flattened[4]  * Flattened[2] * Flattened[15]
			   -  Flattened[4]  * Flattened[3] * Flattened[14]
			   -  Flattened[12] * Flattened[2] * Flattened[7]
			   +  Flattened[12] * Flattened[3] * Flattened[6];

	Inverse[10] = Flattened[0]  * Flattened[5] * Flattened[15]
				- Flattened[0]  * Flattened[7] * Flattened[13]
				- Flattened[4]  * Flattened[1] * Flattened[15]
				+ Flattened[4]  * Flattened[3] * Flattened[13]
				+ Flattened[12] * Flattened[1] * Flattened[7]
				- Flattened[12] * Flattened[3] * Flattened[5];

	Inverse[14] = -Flattened[0]  * Flattened[5] * Flattened[14]
				+  Flattened[0]  * Flattened[6] * Flattened[13]
				+  Flattened[4]  * Flattened[1] * Flattened[14]
				-  Flattened[4]  * Flattened[2] * Flattened[13]
				-  Flattened[12] * Flattened[1] * Flattened[6]
				+  Flattened[12] * Flattened[2] * Flattened[5];

	Inverse[3]  = -Flattened[1] * Flattened[6] * Flattened[11]
				+  Flattened[1] * Flattened[7] * Flattened[10]
				+  Flattened[5] * Flattened[2] * Flattened[11]
				-  Flattened[5] * Flattened[3] * Flattened[10]
				-  Flattened[9] * Flattened[2] * Flattened[7]
				+  Flattened[9] * Flattened[3] * Flattened[6];

	Inverse[7] = Flattened[0] * Flattened[6] * Flattened[11]
			   - Flattened[0] * Flattened[7] * Flattened[10]
			   - Flattened[4] * Flattened[2] * Flattened[11]
			   + Flattened[4] * Flattened[3] * Flattened[10]
			   + Flattened[8] * Flattened[2] * Flattened[7]
			   - Flattened[8] * Flattened[3] * Flattened[6];

	Inverse[11] = -Flattened[0] * Flattened[5] * Flattened[11]
				+  Flattened[0] * Flattened[7] * Flattened[9]
				+  Flattened[4] * Flattened[1] * Flattened[11]
				-  Flattened[4] * Flattened[3] * Flattened[9]
				-  Flattened[8] * Flattened[1] * Flattened[7]
				+  Flattened[8] * Flattened[3] * Flattened[5];

	Inverse[15] = Flattened[0] * Flattened[5] * Flattened[10]
				- Flattened[0] * Flattened[6] * Flattened[9]
				- Flattened[4] * Flattened[1] * Flattened[10]
				+ Flattened[4] * Flattened[2] * Flattened[9]
				+ Flattened[8] * Flattened[1] * Flattened[6]
				- Flattened[8] * Flattened[2] * Flattened[5];

	Determinant = Flattened[0] * Inverse[0] + Flattened[1] * Inverse[4] + Flattened[2] * Inverse[8] + Flattened[3] * Inverse[12];
	if (Determinant == 0.0f) return M44F_IDENTITY;

	Determinant = 1.0f / Determinant;
	for (int I = 0; I < 16; I++)
	{
		Inverse[I] *= Determinant;
	}

	Result.E[0][0] = Inverse[0];  Result.E[0][1] = Inverse[1];  Result.E[0][2] = Inverse[2];  Result.E[0][3] = Inverse[3];
	Result.E[1][0] = Inverse[4];  Result.E[1][1] = Inverse[5];  Result.E[1][2] = Inverse[6];  Result.E[1][3] = Inverse[7];
	Result.E[2][0] = Inverse[8];  Result.E[2][1] = Inverse[9];  Result.E[2][2] = Inverse[10]; Result.E[2][3] = Inverse[11];
	Result.E[3][0] = Inverse[12]; Result.E[3][1] = Inverse[13]; Result.E[3][2] = Inverse[14]; Result.E[3][3] = Inverse[15];
	return Result;
}
