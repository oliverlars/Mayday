#include <metal_stdlib>

using namespace metal;


struct per_tile_data
{
	int At;
	int Count;
};

// Just doing to do points for now and see how that goes
struct data
{
	float2 Position;
	float Radius;
};

struct uniforms
{
	int NumberOfTilesInX;
	int NumberOfTilesInY;
};

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return length(Position - Centre) - Radius; 
}

kernel void ComputeMain(texture2d<float, access::read_write> Texture [[ texture(0) ]],
						device per_tile_data *DataView [[buffer(0)]],
						device data *Data [[buffer(1)]],
						constant uniforms &Uniforms [[buffer(2)]],
						uint2 ThreadPositionInGrid [[thread_position_in_grid]],
						uint2 ThreadGroupPositionInGrid [[threadgroup_position_in_grid]],
						uint2 ThreadGroupSize [[threads_per_threadgroup]])
{
	int TileIndex = ThreadGroupPositionInGrid.x + ThreadGroupPositionInGrid.y * Uniforms.NumberOfTilesInX;
	int Count = DataView[TileIndex].Count;


	// https://blog.pkh.me/p/44-perfecting-anti-aliasing-on-signed-distance-functions.html
	int SmallestDim = min(Texture.get_width(), Texture.get_height());
	float UnitSize = 1.0/(float)SmallestDim;

	for(int I = 0; I < Count; I++)
	{
		int DataIndex = DataView[TileIndex].At + I;
		float2 Position = float2(ThreadPositionInGrid.x, ThreadPositionInGrid.y);
		float Distance = DistanceToCircle(Position, Data[DataIndex].Position, Data[DataIndex].Radius);
		float AA = smoothstep(-UnitSize, UnitSize, -Distance);
		float4 CurrentColour = Texture.read(ThreadPositionInGrid);
		float3 Colour = mix(CurrentColour.rgb, float3(1, 1, 0), AA);
		Texture.write(float4(Colour, 1.0), ThreadPositionInGrid);
	}
}
