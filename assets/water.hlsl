struct VS_Input
{
	float3 pos : POS;
	float3 normal : NORMAL;
	float4 color : COL;
	float2 uv : UV;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
	float4 world_pos : POS0;
	float4 light_space_pos : POS1;
	float3 normal : NORMAL;
	float4 color : COLOR;
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

	float4 pos = float4(input.pos, 1.0f);

	output.pos = mul(pos, transform);
	
	output.world_pos = pos;
	output.color = input.color;

	return output;
}

float4 ps_main(VS_Output Input) : SV_Target
{
	float4 Color = float4(0.0f, 0.0f, 1.0f, 1.0f);
	return Color;
}