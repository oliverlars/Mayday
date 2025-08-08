#include <metal_stdlib>1


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
	float4 Colour;
};

struct bezier
{
	metal::float2 Start;
	metal::float2 Middle;
	metal::float2 End;
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
};

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return metal::length(Position - Centre) - Radius; 
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


	metal::float3 Colour = Texture.read(ThreadPositionInGrid).rgb;
	float Normalise = 1.0/(float)Count;
	for(int I = 0; I < Count; I++)
	{
		int DataIndex = DataView[TileIndex].At + I;
		metal::float2 Position = float2(ThreadPositionInGrid.x, ThreadPositionInGrid.y);
		float Distance = DistanceToCircle(Position, Circles[DataIndex].Position, Circles[DataIndex].Radius);
		float AA = metal::smoothstep(-1, 1, -Distance) * Circles[DataIndex].Colour.a;
		Colour = metal::mix(Colour.rgb, Circles[DataIndex].Colour.rgb * Normalise, AA);
		Texture.write(float4(Colour, 1.0), ThreadPositionInGrid);
	}
}
