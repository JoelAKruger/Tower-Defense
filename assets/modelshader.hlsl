struct VS_Output
{
	float4 pos : SV_POSITION;
	float4 world_pos : POS0;
	float4 light_space_pos : POS1;
	float3 normal : NORMAL;
	float4 color : COLOR;
};

struct VS_Input
{
	float3 pos : POS;
	float3 normal : NORMAL;
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

//Lighting
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

	float4 pos = mul(float4(input.pos, 1.0f), model_transform);

	output.pos = mul(pos, world_transform);
	
	output.normal = mul(float4(input.normal, 0.0f), model_transform);
	//output.world_pos = pos;
	output.light_space_pos = mul(pos, light_transform);

	output.color = color;
	return output;
}

float4 ps_main(VS_Output input) : SV_Target
{
	float2 shadow_uv;
	shadow_uv.x = 0.5f + (input.light_space_pos.x / input.light_space_pos.w * 0.5f);
	shadow_uv.y = 0.5f - (input.light_space_pos.y / input.light_space_pos.w * 0.5f);
	float pixel_depth = input.light_space_pos.z / input.light_space_pos.w;

	float ambient = 0.3f;

	float3 light_dir = 1.0f * normalize(float3(1.0f, 1.0f, 1.0f));
	float diffuse = 0.5f + 0.5f * dot(normalize(input.normal), -1.0f * light_dir);

	//float3 view_dir = normalize(camera_pos - input.pos);
	//float3 reflect_dir = reflect(-light_dir, input.normal);

	//float specular = 0.5f * pow(max(dot(view_dir, reflect_dir), 0.0), 32);

	float epsilon = 0.001f;
	float is_lit = float(shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, pixel_depth - epsilon));

	if (is_lit == 0.0f)
	{
		diffuse *= 0.2f;
	}

	return float4((ambient + diffuse) * input.color.rgb, input.color.a);
}