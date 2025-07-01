#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float4 Colour;
	[[flat]] float2 Start;
	[[flat]] float2 Control;
	[[flat]] float2 End;
	[[flat]] float Thickness;
	[[flat]] int   Type;
};

struct vertex_input
{
	float4 Position  [[attribute(0)]];
	float4 Colour    [[attribute(1)]];
	float2 Start     [[attribute(2)]];
	float2 Control   [[attribute(3)]];
	float2 End       [[attribute(4)]];
	float  Thickness [[attribute(4)]];
};

struct uniforms
{
	float4x4 PVMMatrix;
};

template<typename T>
float dot2(T Value)
{
	return dot(Value, Value);
}

// https://iquilezles.org/articles/distfunctions2d/
float DistanceToBezier(float2 Pos, float2 A, float2 B, float2 C)
{
	float2 a = B - A;
	float2 b = A - 2.0*B + C;
	float2 c = a * 2.0;
	float2 d = A - Pos;
	float kk = 1.0/dot(b,b);
	float kx = kk * dot(a,b);
	float ky = kk * (2.0*dot(a,a)+dot(d,b)) / 3.0;
	float kz = kk * dot(d,a);      
	float res = 0.0;
	float p = ky - kx*kx;
	float p3 = p*p*p;
	float q = kx*(2.0*kx*kx-3.0*ky) + kz;
	float h = q*q + 4.0*p3;
	if( h >= 0.0) 
	{ 
		h = sqrt(h);
		float2 x = (float2(h,-h)-q)/2.0;
		float2 uv = sign(x)*pow(abs(x), float2(1.0/3.0));
		float t = clamp( uv.x+uv.y-kx, 0.0, 1.0 );
		res = dot2(d + (c + b*t)*t);
	}
	else
	{
		float z = sqrt(-p);
		float v = acos( q/(p*z*2.0) ) / 3.0;
		float m = cos(v);
		float n = sin(v)*1.732050808;
		float3  t = clamp(float3(m+m,-n-m,n-m)*z-kx,0.0,1.0);
		res = min( dot2(d+(c+b*t.x)*t.x),
				   dot2(d+(c+b*t.y)*t.y) );
		// the third root cannot be the closest
		// res = min(res,dot2(d+(c+b*t.z)*t.z));
	}
	return sqrt(res);
}

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.Colour = Vertices[VertexID].Colour;
	Output.Start = Vertices[VertexID].Start;
	Output.Control = Vertices[VertexID].Control;
	Output.End = Vertices[VertexID].End;
	Output.Thickness = Vertices[VertexID].Thickness;
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	float Distance = DistanceToBezier(Input.Position.xy, Input.Start, Input.Control, Input.End);
	float HalfThickness = 0.5 * Input.Thickness;
	float AA = smoothstep(HalfThickness + HalfThickness*fwidth(Distance), HalfThickness - HalfThickness*fwidth(Distance), Distance);

	return float4(Input.Colour.rgb, float(AA));
}