#include <metal_stdlib>

using namespace metal;


struct data_view 
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

kernel void ComputeMain(texture2d<float, access::write> Texture [[ texture(0) ]],
				   		uint2 ThreadPositionInGrid [[thread_position_in_grid]])
{
	if (ThreadPositionInGrid.x >= Texture.get_width() || ThreadPositionInGrid.y >= Texture.get_height())
	{
		return;
	}
	uint2 TileCoordinate = ThreadPositionInGrid / 32;
	float2 LocalCoordinate = float2(ThreadPositionInGrid % 32) / 32.0;
	float X = fmod(TileCoordinate.x/32.0, 1.0);
	float Y = fmod(TileCoordinate.y/32.0, 1.0);
	float4 Colour = float4(X * LocalCoordinate.x, Y * LocalCoordinate.y, 0.0, 1.0);
	Texture.write(Colour, ThreadPositionInGrid);
}
