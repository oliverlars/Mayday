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
	float2 UV       [[attribute(2)]];
	uint CurveStart [[attribute(3)]];
	uint CurveCount [[attribute(4)]];
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

float2 CalculateQuadraticRoots(float a, float b, float c)
{
	const float epsilon = 1e-5;
	float2 roots = float2(-99999.0);

	if (abs(a) < epsilon)
	{
		if (b != 0.0) roots[0] = -c / b;
	}
	else
	{
		float discriminant = b * b - 4.0 * a * c;
		if (discriminant > -epsilon)
		{
			float s = sqrt(max(0.0, discriminant));
			roots[0] = (-b + s) / (2.0 * a);
			roots[1] = (-b - s) / (2.0 * a);
		}
	}
	return roots;
}


// https://github.com/GreenLightning/gpu-font-rendering/tree/master
//https://github.com/SebLague/Text-Rendering/blob/main/Assets/Scripts/SebText/Renderer/Resources/TextShader.shader
float ComputeCoverage(float InverseDiameter, float2 P0, float2 P1, float2 P2, float2 PixelSize)
{

	float InvPixelSize = (1.0 / PixelSize.x);
	bool IsDownwardCurve = P0.y > 0 || P2.y < 0;

	if (IsDownwardCurve)
	{
		if (P0.y < 0 && P2.y <= 0) return -1.0;
		if (P0.y > 0 && P2.y >= 0) return -1.0;
	}
	else
	{
		if (P0.y <= 0 && P2.y < 0) return -1.0;
		if (P0.y >= 0 && P2.y > 0) return -1.0;
	}

	float2 A = P0 - 2 * P1 + P2;
	float2 B = 2 * (P1 - P0);
	float2 C = P0;
	
	float Epsilon = 1e-4;
	float2 Roots = CalculateQuadraticRoots(A.y, B.y, C.y);
	bool OnSeg0 = Roots[0] >= -Epsilon && Roots[0] <= 1 + Epsilon;
	bool OnSeg1 = Roots[1] >= -Epsilon && Roots[1] <= 1 + Epsilon;

	float T0 = saturate(Roots[0]);
	float T1 = saturate(Roots[1]);
	float Intersect0 = A.x * T0 * T0 + B.x * T0 + C.x;
	float Intersect1 = A.x * T1 * T1 + B.x * T1 + C.x;

	float Coverage = float(0.0);
	int Sign = IsDownwardCurve ? 1 : -1;
	if (OnSeg0) Coverage += saturate(0.5 + Intersect0 * InvPixelSize) * Sign;
	if (OnSeg1) Coverage += saturate(0.5 + Intersect1 * InvPixelSize) * Sign;

	return saturate(Coverage);
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
							 texture2d<float> FontAtlas [[texture(0)]],
							 device float2 *Curves [[buffer(0)]],
							 sampler Sampler [[sampler(0)]])
{

	float Alpha = 0.0;

	float2 Size = Input.UV;

	//Size.y = 1.0 - Size.y;

	float2 PixelSize = abs(dfdx(Input.UV)) + abs(dfdy(Input.UV));
	
	float2 InverseDiameter = 1.0 / (abs(dfdx(Size)) + abs(dfdy(Size)));
	for(int Offset = -1; Offset <= 1; Offset += 1)
	{
		float2 SampleUV = Size + float2(0.0, Offset) * (dfdy(Size) / 3.0);
		float SampleCov = 0.0;
		for (int I = 0; I < Input.CurveCount; I += 3) {
			float2 P0 = Curves[(Input.CurveStart + I) + 0];
			float2 P1 = Curves[(Input.CurveStart + I) + 1];
			float2 P2 = Curves[(Input.CurveStart + I) + 2];

			P0 -= Size;
			P1 -= Size;
			P2 -= Size;

			float Coverage = ComputeCoverage(InverseDiameter.y, P0, P1, P2, PixelSize);
			if(Coverage < 0.0) continue;
			SampleCov += Coverage;
		}
		Alpha += saturate(SampleCov);
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
