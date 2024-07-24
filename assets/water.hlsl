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
	return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
	return float4(0.0f, 0.0f, 1.0f, 1.0f);
}