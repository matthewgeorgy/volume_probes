struct vertex
{
	float3 Position;
};

cbuffer model_params : register(b0)
{
	float4x4 World, View, Proj;
};

StructuredBuffer<vertex> Vertices;

struct ps_in
{
	float4 Position : SV_Position;
};

ps_in
main(uint VertexID : SV_VertexID)
{
	ps_in		Output;
	vertex		Input = Vertices[VertexID];

	Output.Position = float4(Input.Position, 1.0);
	Output.Position = mul(World, Output.Position);
	Output.Position = mul(View, Output.Position);
	Output.Position = mul(Proj, Output.Position);

	return (Output);
}

