struct VS_Input
{
    float3 pos    : POS;
	float3 normal : NORMAL;
	float2 uv     : TEX;
    float4 color  : COL;
};

struct VS_Output_Default
{
	float4 pos : SV_POSITION;
	//float4 pos_world : POS0;
	float4 pos_light_space : POS1;
	float4 color : COL;
	float3 normal : NORMAL;
};

cbuffer Transform : register(b0)
{
	float4x4 world_transform;
};

cbuffer ModelTransform : register(b2)
{
	float4x4 model_transform;
};

cbuffer LightTransform : register(b4)
{
	float4x4 light_transform;
};

Texture2D shadow_map : register(t1);
SamplerComparisonState shadow_sampler : register(s1);

// Normal shaders
VS_Output_Default VertexShader(VS_Input input)
{
	float4 world_pos = mul(float4(input.pos, 1.0f), model_transform);

	VS_Output_Normal output;
	output.pos = mul(world_pos, world_transform);
	output.normal = mul(float4(input.normal, 0.0f), model_transform);
	output.pos_light_space = mul(world_pos, light_transform);
	output.color = input.color;
	return output;
}

float4 PixelShader_Color(VS_Output_Default input) : SV_TARGET
{
	float2 shadow_uv;
	shadow_uv.x = 0.5f + (input.light_space_pos.x / input.light_space_pos.w * 0.5f);
	shadow_uv.y = 0.5f - (input.light_space_pos.y / input.light_space_pos.w * 0.5f);
	float pixel_depth = input.light_space_pos.z / input.light_space_pos.w;
   
	float epsilon = 0.0001f;
	float lighting = float(shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, pixel_depth - epsilon));

	if (lighting == 0.0f)
	{
		input.color.rgb *= 0.2f;
	}

	return input.color;
}