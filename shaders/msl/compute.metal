#include <metal_stdlib>

using namespace metal;

kernel void ComputeMain(device int *Data [[buffer(0)]],
				   		uint ThreadPositionInGrid [[thread_position_in_grid]])
{
    if (ThreadPositionInGrid % 2 == 0)
	{
        Data[ThreadPositionInGrid] = ThreadPositionInGrid * 2;
    }
	else
	{
        Data[ThreadPositionInGrid] = ThreadPositionInGrid % 3;
    }
}
