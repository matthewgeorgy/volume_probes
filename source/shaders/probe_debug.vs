struct vertex
{
	float3		Pos;
};

struct probe
{
	float3		Pos;
	float		Transmittance;
};

struct ps_in
{
	float4		Pos : SV_Position;
	float		Transmittance : COLOR;
};

StructuredBuffer<vertex>		Vertices : register(t0);
StructuredBuffer<probe>			Probes : register(t1);

cbuffer model_params : register(b0)
{
	float4x4		World,
					View,
					Proj;
};

cbuffer grid_params : register(b1)
{
	int3		GridDims;
	uint		ProbeCount;
	float3		GridMin;
	float3		GridMax;
	float3		GridExtents;
	float3		GridExtentsRcp;
	float3		CellSize;
};

ps_in
main(uint VertexID : SV_VertexID,
	 uint InstanceID : SV_InstanceID)
{
	ps_in			Output;
	vertex			Input;
	probe			Probe;
	float3			Scale = 0.5f * CellSize;


	Input = Vertices[VertexID];
	Probe = Probes[InstanceID];

	Input.Pos *= Scale;
	Output.Pos = float4(Input.Pos - (Scale * float3(0.5f, 0.5f, 0.5f) - Probe.Pos), 1.0f);

	Output.Pos = mul(View, Output.Pos);
	Output.Pos = mul(Proj, Output.Pos);
	Output.Transmittance = Probe.Transmittance;

	return (Output);
}

