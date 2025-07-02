struct vertex_input
{
	float4 Position:  POS;
	float4 Colour:    COL;
	float2 Start:     START;
	float2 Control:   CONTROL;
	float2 End:       END;
	float  Thickness: THICKNESS;
};

struct vertex_output
{
	float4 Position: SV_POSITION;
	float4 Colour:  COL;

	nointerpolation float2 Start: START;
	nointerpolation float2 Control: CONTROL;
	nointerpolation float2 End: END;
	float Thickness: THICKNESS;
	nointerpolation bool EvaluateAsLine: EvaluateAsLine;
};

cbuffer uniforms : register(b0)
{
	row_major float4x4 PVMMatrix;
};

float dot2(float Value)
{
	return dot(Value, Value);
}

float dot2(float2 Value)
{
	return dot(Value, Value);
}

float dot2(float3 Value)
{
	return dot(Value, Value);
}

float dot2(float4 Value)
{
	return dot(Value, Value);
}

// https://iquilezles.org/articles/distfunctions2d/
float DistanceToSegment(float2 Position, float2 Start, float2 End)
{
	float2 EndToStart = End - Start;
	float2 PositionToStart = Position - Start;
	float H = clamp( dot(PositionToStart, EndToStart)/dot(EndToStart,EndToStart), 0.0, 1.0);
	return length(PositionToStart - H*EndToStart);
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
		float2 uv = sign(x)*pow(abs(x), float2(1.0/3.0, 1.0/3.0));
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

bool AreColinear(float2 A, float2 B, float2 C) {
	float2 AB = B - A;
	float2 AC = C - A;
	float Cross = AB.x * AC.y - AB.y * AC.x;
	// Since they're in pixel space we can afford a larger tolerance.
	return abs(Cross) < 1.0;
}

vertex_output VertexMain(vertex_input Vertex)
{
	vertex_output Output;
	Output.Position = mul(Vertex.Position, PVMMatrix);
	Output.Colour = Vertex.Colour;
	Output.Start = Vertex.Start;
	Output.Control = Vertex.Control;
	Output.End = Vertex.End;
	Output.Thickness = Vertex.Thickness;
	Output.EvaluateAsLine = AreColinear(Output.Start, Output.Control, Output.End);
	return Output;
}

float4 PixelMain(vertex_output Input) : SV_TARGET
{
	float HalfThickness = 0.5 * Input.Thickness;
	float Distance;
	if(Input.EvaluateAsLine)
	{
		
		Distance = DistanceToSegment(Input.Position.xy, Input.Start, Input.End) - HalfThickness;
	}
	else
	{
		Distance = DistanceToBezier(Input.Position.xy, Input.Start, Input.Control, Input.End) - HalfThickness;
	}

	float AA = smoothstep(HalfThickness + HalfThickness, HalfThickness - HalfThickness, Distance);

	return float4(Input.Colour.rgb, float(AA));
}