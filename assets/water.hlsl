struct VS_Input
{
	float3 pos : POS;
	float2 uv : TEX;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer Transform : register(b0)
{
	float4x4 transform;
};

cbuffer Time : register(b1)
{
	float time;
};

VS_Output vs_main(VS_Input input)
{
	VS_Output output;
	output.pos = mul(float4(input.pos, 1.0f), transform);
	output.uv = input.uv;
	return output;
}

float Grid(float2 UV, float Period, float HalfWidth)
{
	float2 P = UV;
	float2 Size = float2(HalfWidth, HalfWidth);
	
	float2 A0 = fmod(P - Size, Period);
	float2 A1 = fmod(P + Size, Period);

	float2 A = A1 - A0;

	float G = min(A.x, A.y);

	float Result = clamp(G, 0, 1);
	return Result;
}

float4 ps_main(VS_Output Input) : SV_Target
{
	float4 Color = float4(0.11f, 0.4f, 0.8f, 1.0f);

	float Intensity = 0.7f;

	Color /= clamp(Grid(1000.0f * Input.uv, 20.0f, 1.0f) * Grid(1000.0f * Input.uv, 100.0f, 2.0f) , Intensity, 1.0f);

	return Color;
}