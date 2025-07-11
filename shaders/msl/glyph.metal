#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	uint CurveStart;
	uint CurveCount;
	float4 Colour;
	float2 UV;
};

struct vertex_input
{
	float4 Position [[attribute(0)]];
	float4 Colour   [[attribute(1)]];
	float4 UV       [[attribute(2)]];
	uint CurveStart [[attribute(3)]];
	uint CurveCount [[attribute(4)]];
	float  PointSize;
};

struct uniforms
{
	float4x4 PVMMatrix;
};

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.Colour = Vertices[VertexID].Colour;
	Output.UV = Vertices[VertexID].UV.xy;
	Output.CurveStart = Vertices[VertexID].CurveStart;
	Output.CurveCount = Vertices[VertexID].CurveCount;
	return Output;
}

// https://github.com/GreenLightning/gpu-font-rendering/tree/master
float ComputeCoverage(float InverseDiameter, float2 p0, float2 p1, float2 p2) {
	if (p0.y > 0 && p1.y > 0 && p2.y > 0) return 0.0;
	if (p0.y < 0 && p1.y < 0 && p2.y < 0) return 0.0;

	// Note: Simplified from abc formula by extracting a factor of (-2) from b.
	float2 a = p0 - 2*p1 + p2;
	float2 b = p0 - p1;
	float2 c = p0;

	float t0, t1;
	if (abs(a.y) >= 1e-5) {
		// Quadratic segment, solve abc formula to find roots.
		float radicand = b.y*b.y - a.y*c.y;
		if (radicand <= 0) return 0.0;
	
		float s = sqrt(radicand);
		t0 = (b.y - s) / a.y;
		t1 = (b.y + s) / a.y;
	} else {
		// Linear segment, avoid division by a.y, which is near zero.
		// There is only one root, so we have to decide which variable to
		// assign it to based on the direction of the segment, to ensure that
		// the ray always exits the shape at t0 and enters at t1. For a
		// quadratic segment this works 'automatically', see readme.
		float t = p0.y / (p0.y - p2.y);
		if (p0.y < p2.y) {
			t0 = -1.0;
			t1 = t;
		} else {
			t0 = t;
			t1 = -1.0;
		}
	}

	float alpha = 0;
	
	if (t0 >= 0 && t0 < 1) {
		float x = (a.x*t0 - 2.0*b.x)*t0 + c.x;
		alpha += clamp(x * InverseDiameter + 0.5, 0.0f, 1.0f);
	}

	if (t1 >= 0 && t1 < 1) {
		float x = (a.x*t1 - 2.0*b.x)*t1 + c.x;
		alpha -= clamp(x * InverseDiameter + 0.5, 0.0f, 1.0f);
	}

	return alpha;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
							 texture2d<float> FontAtlas [[texture(0)]],
							 device float2 *Curves [[buffer(0)]],
							 sampler Sampler [[sampler(0)]])
{

	float Alpha = 0.0;

	float2 Size = Input.UV;

	// HACK(Oliver): This flip could happen in the CPU too, either in the curves place
	// or the uv coordinate generation, but whatever its fine here i guess.
	Size.y = 1.0 - Size.y;
	float2 InverseDiameter = 1.0 / (abs(dfdx(Size)) + abs(dfdy(Size)));
	for(int offset = -1; offset <= 1; offset += 1) {
		Size = Size + float2(0, offset) * dfdx(Size) / 3.0;
		for (int i = 0; i < Input.CurveCount; i += 3) {
			float2 p0 = Curves[(Input.CurveStart + i) + 0];
			float2 p1 = Curves[(Input.CurveStart + i) + 1];
			float2 p2 = Curves[(Input.CurveStart + i) + 2];

			p0 -= Size;
			p1 -= Size;
			p2 -= Size;

			Alpha += ComputeCoverage(InverseDiameter.y, p0, p1, p2);
		}
	}

	if(0){
		// Visualize control points.
		float2 fw = abs(dfdx(Size)) + abs(dfdy(Size));
		float r = 4.0 * 0.5 * (fw.x + fw.y);
		for (int i = 0; i < Input.CurveCount; i += 3) {
			float2 p0 = Curves[(Input.CurveStart + i) + 0];
			float2 p1 = Curves[(Input.CurveStart + i) + 1];
			float2 p2 = Curves[(Input.CurveStart + i) + 2];
			
			p0 -= Size;
			p1 -= Size;
			p2 -= Size;

			if (dot(p0, p0) < r*r || dot(p2, p2) < r*r) {
				return float4(0, 1, 0, 1);
			}

			if (dot(p1, p1) < r*r) {
				return float4(1, 0, 1, 1);
			}
		}
	}

	return float4(Input.Colour.rgb, Input.Colour.a * Alpha / 3.0);
}
