#include <metal_stdlib>


struct per_tile_data
{
	int At;
	int Count;
	int Type;
};

struct circle
{
	metal::float2 Position;
	metal::float4 Colour;
	float Radius;
};

struct line
{
	metal::float2 Start;
	metal::float2 End;
	float Thickness;
	float4 Colour;
};

struct bezier
{
	metal::float2 Start;
	metal::float2 Control;
	metal::float2 End;
	float Thickness;
	float4 Colour;
};

struct glyph
{
	metal::float2 Position;
	metal::float2 Colour;
	uint CurveStart;
	uint CurveCount;
};

struct uniforms
{
	int NumberOfTilesInX;
	int NumberOfTilesInY;
	metal::float4 Scissor;
};

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return metal::length(Position - Centre) - Radius; 
}

template<typename T>
float dot2(T Value)
{
	return metal::dot(Value, Value);
}

float DistanceToLine(float2 Position, float2 Start, float2 End)
{
	float2 EndToStart = End - Start;
	float2 PositionToStart = Position - Start;
	float H = metal::clamp(metal::dot(PositionToStart, EndToStart)/metal::dot(EndToStart,EndToStart), 0.0, 1.0);
	return metal::length(PositionToStart - H*EndToStart);
}

// https://iquilezles.org/articles/distfunctions2d/
float DistanceToBezier(float2 Pos, float2 A, float2 B, float2 C)
{
	float2 a = B - A;
	float2 b = A - 2.0*B + C;
	float2 c = a * 2.0;
	float2 d = A - Pos;
	float kk = 1.0/metal::dot(b,b);
	float kx = kk * metal::dot(a,b);
	float ky = kk * (2.0*metal::dot(a,a)+metal::dot(d,b)) / 3.0;
	float kz = kk * metal::dot(d,a);
	float res = 0.0;
	float p = ky - kx*kx;
	float p3 = p*p*p;
	float q = kx*(2.0*kx*kx-3.0*ky) + kz;
	float h = q*q + 4.0*p3;
	if( h >= 0.0) 
	{ 
		h = metal::sqrt(h);
		float2 x = (float2(h,-h)-q)/2.0;
		float2 uv = metal::sign(x)*metal::pow(metal::abs(x), float2(1.0/3.0));
		float t = metal::clamp( uv.x+uv.y-kx, 0.0, 1.0 );
		res = dot2(d + (c + b*t)*t);
	}
	else
	{
		float z = metal::sqrt(-p);
		float v = metal::acos( q/(p*z*2.0) ) / 3.0;
		float m = metal::cos(v);
		float n = metal::sin(v)*1.732050808;
		float3  t = metal::clamp(float3(m+m,-n-m,n-m)*z-kx,0.0,1.0);
		res = metal::min( dot2(d+(c+b*t.x)*t.x),
				   dot2(d+(c+b*t.y)*t.y) );
		// the third root cannot be the closest
		// res = min(res,dot2(d+(c+b*t.z)*t.z));
	}
	return metal::sqrt(res);
}

bool AreColinear(float2 A, float2 B, float2 C) {
	float2 AB = B - A;
	float2 AC = C - A;
	float Cross = AB.x * AC.y - AB.y * AC.x;
	// Since they're in pixel space we can afford a larger tolerance.
	return metal::fabs(Cross) < 1.0;
}

kernel void ComputeMain(metal::texture2d<float, metal::access::read_write> Texture [[ texture(0) ]],
						device per_tile_data *DataView [[buffer(0)]],
						device circle *Circles [[buffer(1)]],
						device line *Lines [[buffer(2)]],
						device bezier *Beziers [[buffer(3)]],
						device glyph *Glyphs [[buffer(4)]],
						constant uniforms &Uniforms [[buffer(5)]],
						uint2 ThreadPositionInGrid [[thread_position_in_grid]],
						uint2 ThreadGroupPositionInGrid [[threadgroup_position_in_grid]],
						uint2 ThreadGroupSize [[threads_per_threadgroup]])
{
	int TileIndex = ThreadGroupPositionInGrid.x + ThreadGroupPositionInGrid.y * Uniforms.NumberOfTilesInX;
	int Count = DataView[TileIndex].Count;


	metal::float3 ColourSoFar = Texture.read(ThreadPositionInGrid).rgb;
	float AlphaSoFar = 0.0;


	// HACK(Oliver): I just made this kind of work but too tired to fix it now
	if(ThreadPositionInGrid.x < Uniforms.Scissor.x ||
	   ThreadPositionInGrid.x > Uniforms.Scissor.x + Uniforms.Scissor.z ||
	   ThreadPositionInGrid.y > Uniforms.Scissor.y ||
	   ThreadPositionInGrid.y < Uniforms.Scissor.y - Uniforms.Scissor.w)
	{
		//FIXME(Oliver): No clue why this isn't working now

		return;
	}
	
	for(int I = 0; I < Count; I++)
	{
		int DataIndex = DataView[TileIndex].At + I;
		int Type = DataView[TileIndex].Type;
		if(0) {}
		else if(Type == 0)
		{

			metal::float2 Position = float2(ThreadPositionInGrid.x, ThreadPositionInGrid.y);
			float Distance = DistanceToCircle(Position, Circles[DataIndex].Position, Circles[DataIndex].Radius);
			float AA = metal::smoothstep(-1, 1, -Distance) * Circles[DataIndex].Colour.a;
	
			// Equivalent to ONE_MINUS_SRC_ALPHA
			// Framebuffer stores premultiplied alpha
			ColourSoFar += (1.0 - AlphaSoFar) * Circles[DataIndex].Colour.rgb*AA;
			AlphaSoFar += AA*(1.0 - AlphaSoFar);
		}
		else if(Type == 1)
		{
			metal::float2 Position = float2(ThreadPositionInGrid.x, ThreadPositionInGrid.y);
			float Thickness = Lines[DataIndex].Thickness;
			float Distance = DistanceToLine(Position, Lines[DataIndex].Start, Lines[DataIndex].End) - Thickness*0.5;
			float AA = metal::smoothstep(-1, 1, -Distance) * Lines[DataIndex].Colour.a;

			// Equivalent to ONE_MINUS_SRC_ALPHA
			// Framebuffer stores premultiplied alpha
			ColourSoFar += (1.0 - AlphaSoFar) * Lines[DataIndex].Colour.rgb*AA;
			AlphaSoFar += AA*(1.0 - AlphaSoFar);
		}
		else if(Type == 2)
		{
			metal::float2 Position = float2(ThreadPositionInGrid.x, ThreadPositionInGrid.y);
			float Thickness = Beziers[DataIndex].Thickness;

			float Distance;
			if(AreColinear(Beziers[DataIndex].Start, Beziers[DataIndex].Control, Beziers[DataIndex].End))
			{
				Distance = DistanceToLine(Position, Beziers[DataIndex].Start, Beziers[DataIndex].End) - Thickness*0.5;
			}
			else
			{
				Distance = DistanceToBezier(Position, Beziers[DataIndex].Start, Beziers[DataIndex].Control, Beziers[DataIndex].End) - Thickness*0.5;
			}

			float AA = metal::smoothstep(-1, 1, -Distance) * Beziers[DataIndex].Colour.a;
			
			ColourSoFar += (1.0 - AlphaSoFar) * Beziers[DataIndex].Colour.rgb*AA;
			AlphaSoFar += AA*(1.0 - AlphaSoFar);
		}
	}
	Texture.write(float4(ColourSoFar, 1.0), ThreadPositionInGrid);
}
