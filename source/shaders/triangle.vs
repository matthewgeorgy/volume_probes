struct vertex
{
	float3 Position;
	float3 Color;
};

StructuredBuffer<vertex> Vertices;

struct ps_in
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

ps_in
main(uint VertexID : SV_VertexID)
{
	ps_in		Output;
	vertex		Input = Vertices[VertexID];

	Output.Position = float4(Input.Position, 1.0);
	Output.Color = float4(Input.Color, 1.0);

	return (Output);
}

