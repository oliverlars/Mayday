struct vertex_input
{
    float4 Position: POS;
    float4 Colour:   COL;
	float4 UV:       TEX;
	float  PointSize: PSIZE;
};

struct vertex_output
{
	float4 Position: SV_POSITION;
	float4 Colour:  COL;
};

cbuffer uniforms : register(b0)
{
	row_major float4x4 PVMMatrix;
};

float4 ToSRGB(float4 LinearRGB)
{
	float3 Higher = float3(1.055, 1.055, 1.055)*pow(LinearRGB.rgb, float3(1.0/2.4, 1.0/2.4, 1.0/2.4)) - float3(0.055, 0.055, 0.055);
	float3 Lower = LinearRGB.rgb * float3(12.92, 12.92, 12.92);

	float3 RGB = float3(0.0, 0.0, 0.0);
	if(all(LinearRGB.rgb < float3(0.0031308, 0.0031308, 0.0031308)))
	{
		RGB = Lower;
	}
	else
	{
		RGB = Higher;
	}

	return float4(RGB, LinearRGB.a);
}

float3 TonemapToACESFilm(float3 X)
{
	float A = 2.51;
	float B = 0.03;
	float C = 2.43;
	float D = 0.59;
	float E = 0.14;

	return clamp((X * (A*X + B)) / (X * (C*X + D) + E), 0.0, 1.0);
}


vertex_output VertexMain(vertex_input Vertex)
{
	vertex_output Output;
	Output.Position = mul(Vertex.Position, PVMMatrix);
	Output.Colour = Vertex.Colour;

	return Output;
}

float4 PixelMain(vertex_output Input) : SV_TARGET
{
	float3 ACES = TonemapToACESFilm(Input.Colour.rgb);
	return ToSRGB(float4(ACES.rgb, Input.Colour.a));
}