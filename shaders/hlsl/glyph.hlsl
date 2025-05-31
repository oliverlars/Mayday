

Texture2D    FontAtlas: register(t0);
SamplerState Sampler:   register(s0);

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
	float2 UV:      TEXCOORD0;
};

cbuffer uniforms : register(b0)
{
	row_major float4x4 PVMMatrix;
}


vertex_output VertexMain(vertex_input Vertex)
{
	vertex_output Output;
	Output.Position = mul(Vertex.Position, PVMMatrix);
	Output.Colour = Vertex.Colour;
	Output.UV = Vertex.UV.xy;
	return Output;
}

float4 PixelMain(vertex_output Input) : SV_TARGET
{
	float4 Colour = float4(Input.Colour.rgb, FontAtlas.Sample(Sampler, Input.UV).r*Input.Colour.a);
	return Colour;
}
