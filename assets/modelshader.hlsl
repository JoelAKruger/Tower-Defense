struct VS_Input
{
	float3 pos : POS;
	float3 normal : NORMAL;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
};

cbuffer WorldTransform : register(b0)
{
	float4x4 world_transform;
};

cbuffer Time : register(b1)
{
	float time;
};

cbuffer ModelTransform : register(b2)
{
	float4x4 model_transform;
};

cbuffer Color : register(b3)
{
	float4 color;
};

VS_Output vs_main(VS_Input input)
{
	VS_Output output;
	output.pos = mul(mul(float4(input.pos, 1.0f), model_transform), world_transform);
	
	output.normal = mul(float4(input.normal, 0.0f), model_transform);

	output.color = color;
	return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
	float ambient = 0.3f;

	float3 light_dir = 1.0f * normalize(float3(-2.0f, 1.0f, 3.0f));
	float diffuse = 0.5f + 0.5f * dot(normalize(input.normal), light_dir);

	return float4((ambient + diffuse) * input.color.rgb, input.color.a);
}