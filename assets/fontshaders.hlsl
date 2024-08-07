struct VS_Input
{
	float3 pos : POS;
	float2 uv : TEX;
	float4 color : COL;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
	float4 color : COL;
	float2 uv : TEXCOORD;
};

cbuffer Transform
{
	float4x4 transform;
};

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

VS_Output vs_main(VS_Input input)
{
	VS_Output output;
	output.pos = mul(float4(input.pos, 1.0f), transform);
	output.uv = input.uv;
	output.color = input.color;
	return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
	float sample = mytexture.Sample(mysampler, input.uv);
	return float4(input.color.rgb, sample);
	//return input.color * sample;
	//return input.color;
}