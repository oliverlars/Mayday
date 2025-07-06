

Texture2D    FontAtlas: register(t0);
SamplerState Sampler:   register(s0);

struct vertex_input
{
	float4 Position: POS;
	float4 Colour:   COL;
	float4 UV:       TEX;
	int    CurveStart: CURVESTART;
	int    CurveCount: CURVECOUNT;
};

struct vertex_output
{
	float4 Position: SV_POSITION;
	float4 Colour:  COL;
	float2 UV:      TEXCOORD0;
	int    CurveStart: CURVESTART;
	int    CurveCount: CURVECOUNT;
};

cbuffer uniforms : register(b0)
{
	row_major float4x4 PVMMatrix;
};

StructuredBuffer<float2> Curves : register(t1);

vertex_output VertexMain(vertex_input Vertex)
{
	vertex_output Output;
	Output.Position = mul(Vertex.Position, PVMMatrix);
	Output.Colour = Vertex.Colour;
	Output.UV = Vertex.UV.xy;
	Output.CurveStart = Vertex.CurveStart;
	Output.CurveCount = Vertex.CurveCount;
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
		alpha += clamp(x * InverseDiameter + 0.5, 0, 1);
	}

	if (t1 >= 0 && t1 < 1) {
		float x = (a.x*t1 - 2.0*b.x)*t1 + c.x;
		alpha -= clamp(x * InverseDiameter + 0.5, 0, 1);
	}

	return alpha;
}


float4 PixelMain(vertex_output Input) : SV_TARGET
{
	float Alpha = 0.0;

	float2 Size = Input.UV;
	float2 InverseDiameter = 1.0 / ddx(Size);
	for(int offset = -1; offset <= 1; offset += 1) {
		Size = Size + float2(0, offset) * ddx(Size) / 3.0;
		for (int i = 0; i < Input.CurveCount; i++) {
			float2 p0 = Curves[(Input.CurveStart + i)*3 + 0];
			float2 p1 = Curves[(Input.CurveStart + i)*3 + 1];
			float2 p2 = Curves[(Input.CurveStart + i)*3 + 2];

			p0 -= Size;
			p1 -= Size;
			p2 -= Size;

			Alpha += ComputeCoverage(InverseDiameter.y, p0, p1, p2);
		}
	}

	// {
	// 	// Visualize control points.
	// 	float2 fw = ddx(Size);
	// 	float r = 4.0 * 0.5 * (fw.x + fw.y);
	// 	for (int i = 0; i < Input.CurveCount; i++) {
	// 		// float2 p0 = Curves[(Input.CurveStart + i)*3 + 0];
	// 		// float2 p1 = Curves[(Input.CurveStart + i)*3 + 1];
	// 		// float2 p2 = Curves[(Input.CurveStart + i)*3 + 2];

	// 		float2 p0 = float2(0.0, 0.0);
	// 		float2 p1 = float2(0.5, 0.5);
	// 		float2 p2 = float2(1.0, 1.0);
			
	// 		// p0 -= Size;
	// 		// p1 -= Size;
	// 		// p2 -= Size;

	// 		if (dot(p0, p0) < r*r || dot(p2, p2) < r*r) {
	// 			return float4(0, 1, 0, 1);
	// 		}

	// 		if (dot(p1, p1) < r*r) {
	// 			return float4(1, 0, 1, 1);
	// 		}
	// 	}
	// }

	//float4 Colour = float4(Input.Colour.rgb, float(Input.CurveCount)/200.0);
	float4 Colour = float4(Input.Colour.rgb, Alpha);
	return Colour;
}
