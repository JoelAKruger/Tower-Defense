struct VS_Input
{
	float2 pos : POS;
	float2 uv : TEX;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
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
	output.pos = mul(float4(input.pos, 0.0f, 1.0f), transform);;
	output.uv = input.uv;
	return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
	float4 sample = mytexture.Sample(mysampler, input.uv);
	return sample;
}