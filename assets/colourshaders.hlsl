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
	float4 light_space_pos : POS;    
	float4 color : COL;
};

cbuffer Transform : register(b0)
{
	float4x4 transform;
};

cbuffer LightTransform : register(b4)
{
	float4x4 light_transform;
	//float4 camera_pos;
};

Texture2D shadow_map : register(t1);
SamplerComparisonState shadow_sampler : register(s1);

VS_Output vs_main(VS_Input input)
{
    VS_Output output;

	float4 pos = float4(input.pos, 1.0f);

	output.pos = mul(pos, transform);
    output.color = input.color;    
	output.light_space_pos = mul(pos, light_transform);

    return output;
}

float4 ps_main(VS_Output input) : SV_TARGET
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