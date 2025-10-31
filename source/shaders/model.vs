struct vertex
{
	float3		Pos;
};

struct ps_in
{
	float4		Pos : SV_POSITION;
	float4 		Tex : TEXCOORD;
};

StructuredBuffer<vertex>		Vertices : register(t0);

cbuffer model_params : register(b0)
{
	float4x4		World,
					View,
					Proj;
};

ps_in
main(uint VertexID : SV_VertexID)
{
	ps_in			Output;
	vertex			Input;


	Input = Vertices[VertexID];
	Output.Pos = float4(Input, 1.0f);

	Output.Pos = mul(World, Output.Pos);
	Output.Pos = mul(View, Output.Pos);
	Output.Pos = mul(Proj, Output.Pos);

	Output.Tex = float4(0.5f * (Input.Pos + 1), 1.0f);

	return (Output);
}

