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
	int NumberOfTilesinY;
};

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return length(Position - Centre) - Radius; 
}

kernel void ComputeMain(texture2d<float, access::write> Texture [[ texture(0) ]],
						device per_tile_data *DataView [[buffer(0)]],
						device data *Data [[buffer(1)]],
						constant uniforms &Uniforms [[buffer(2)]],
				   		uint2 ThreadPositionInGrid [[thread_position_in_grid]])
{
	if (ThreadPositionInGrid.x >= Texture.get_width() || ThreadPositionInGrid.y >= Texture.get_height())
	{
		return;
	}
	uint2 TileCoordinate = ThreadPositionInGrid / 32;
	int TileIndex = TileCoordinate.x + TileCoordinate.y*Uniforms.NumberOfTilesInX;
	int Count = DataView[TileIndex].Count;
	for(int I = 0; I < Count; I++)
	{
		int DataIndex = DataView[TileIndex].At + I;

		float2 Position = float2(ThreadPositionInGrid.x, ThreadPositionInGrid.y);
		float Distance = DistanceToCircle(Position, Data[DataIndex].Position, Data[DataIndex].Radius);
		if(Distance < 0.0)
		{
			Texture.write(float4(1, 1, 0, 1), ThreadPositionInGrid);
		}
	}
}
