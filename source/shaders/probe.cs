struct probe
{
	float3		Position;
	float		Transmittance;
};

cbuffer model_params : register(b0)
{
	float4x4		World,
					View,
					Proj;
};

cbuffer raymarch_params : register(b1)
{
	uint		ScreenWidth;
	uint		ScreenHeight;
	float		MinVal;
	float		MaxVal;
	float3		LightPos;
	float 		Absorption;
	float 		DensityScale;
	float 		Ambient;
};

cbuffer grid_params : register(b2)
{
	int3		GridDims;
	uint		ProbeCount;
	float3		GridMin;
	float3		GridMax;
	float3		GridExtents;
	float3		GridExtentsRcp;
	float3		CellSize;
};

static const uint		MaxIterations = 64;

Texture3D<float>				Volume : register(t0);
SamplerState					LinearSampler : register(s0);
RWStructuredBuffer<probe>		Probes : register(u0);

float4x4 	inverse(float4x4 m);
float		Lightmarch(float3 Pos);

[numthreads(1, 1, 1)]
void
main(uint3 ThreadID : SV_DispatchThreadID)
{
	float		Transmittance;
	float3		Pos;
	uint		ProbeIndex;


	Pos.x = float(ThreadID.x) * CellSize.x + GridMin.x;
	Pos.y = float(ThreadID.y) * CellSize.y + GridMin.y;
	Pos.z = float(ThreadID.z) * CellSize.z + GridMin.z;

	ProbeIndex = (ThreadID.z * GridDims.x * GridDims.y) + ThreadID.y * GridDims.x + ThreadID.x;
	
	Probes[ProbeIndex].Position = Pos;
	Probes[ProbeIndex].Transmittance = Lightmarch(Pos);
}

float
Lightmarch(float3 Pos)
{
	float3		LightDir = normalize(LightPos - Pos);
	float		StepSize = length(LightPos - Pos) / float(MaxIterations);
	float		TotalDensity = 0;
	float4x4 	InvWorld = inverse(World);


	for (uint i = 0; i < MaxIterations; i++)
	{
		float3 InvPos = mul(InvWorld, float4(Pos, 1)).xyz;
		float Density = DensityScale * Volume.SampleLevel(LinearSampler, InvPos, 0);
		/* float NormalizedDensity = (Density - MinVal) / (MaxVal - MinVal); */

		TotalDensity += Density * StepSize;

		Pos += StepSize * LightDir;
	}
	
	float Transmittance = exp(-TotalDensity * Absorption);

	return (Transmittance);
}

float4x4 inverse(float4x4 m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

