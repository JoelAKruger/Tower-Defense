struct VS_Input
{
    float3 pos    : POS;
	float3 normal : NORMAL;
    float4 color  : COL;
	float2 uv     : UV;
};

struct VS_Output_Default
{
	float4 pos : SV_POSITION;
	float3 pos_world : POS0;
	float4 pos_light_space : POS1;
	float4 pos_clip : TEXCOORD0;
	float4 color : COL;
	float3 normal : NORMAL;
	float2 uv : UV;
};

cbuffer Constants : register(b5)
{
	float4x4 world_to_clip;
	float4x4 model_to_world;
	float4x4 world_to_light;
	float4 color;
	float time;

	float4 clip_plane;
};

Texture2D shadow_map : register(t1);
SamplerComparisonState shadow_sampler : register(s1);

// Normal shaders
VS_Output_Default MyVertexShader(VS_Input input) 
{
	float4 world_pos = mul(float4(input.pos, 1.0f), model_to_world);
	float4 pos_clip = mul(world_pos, world_to_clip);

	VS_Output_Default output;
	output.pos = pos_clip;
	output.pos_world = world_pos.xyz / world_pos.w;
	output.pos_light_space = mul(world_pos, world_to_light);
	output.pos_clip = pos_clip;
	output.color = input.color + color;
	output.normal = (float3) mul(float4(input.normal, 0.0f), model_to_world);
	output.uv = input.uv;
	
	return output;
}

float4 PixelShader_Color(VS_Output_Default input) : SV_TARGET
{
	float clip_plane_distance = dot(clip_plane.xyz, input.pos_world) + clip_plane.w;
	if (clip_plane_distance < 0)
	{
		discard;
	}

	float2 shadow_uv;
	shadow_uv.x = 0.5f + (input.pos_light_space.x / input.pos_light_space.w * 0.5f);
	shadow_uv.y = 0.5f - (input.pos_light_space.y / input.pos_light_space.w * 0.5f);
	float pixel_depth = input.pos_light_space.z / input.pos_light_space.w;
   
	float epsilon = 0.0001f;
	float lighting = float(shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, pixel_depth - epsilon));

	if (lighting == 0.0f)
	{
		input.color.rgb *= 0.2f;
	}

	return input.color;
}

float4 PixelShader_Model(VS_Output_Default input) : SV_TARGET
{
	float clip_plane_distance = dot(clip_plane.xyz, input.pos_world) + clip_plane.w;
	if (clip_plane_distance < 0)
	{
		discard;
	}

	float2 shadow_uv;
	shadow_uv.x = 0.5f + (input.pos_light_space.x / input.pos_light_space.w * 0.5f);
	shadow_uv.y = 0.5f - (input.pos_light_space.y / input.pos_light_space.w * 0.5f);
	float pixel_depth = input.pos_light_space.z / input.pos_light_space.w;

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

Texture2D reflection_texture : register(t2);
SamplerState reflection_sampler : register(s2);

Texture2D refraction_texture : register(t3);
SamplerState refraction_sampler : register(s3);

Texture2D water_dudv_texture : register(t4);
SamplerState water_dudv_sampler : register(s4);

float4 PixelShader_Water(VS_Output_Default input) : SV_Target
{
	float4 water_color = float4(0.0f, 0.2f, 0.05f, 1.0f);
	float distortion_strength = 0.02f;
	float distortion_scaling = 6.0f;
	float wave_speed = 0.03f;

	float2 uv0 = frac(input.uv * distortion_scaling + float2(0.5f * wave_speed * time, 0.0f));
	float2 uv1 = frac(input.uv * distortion_scaling + float2(-wave_speed * time, -wave_speed * time));
	float2 screen_uv = 0.5f * (input.pos_clip.xy / input.pos_clip.w) + float2(0.5f, 0.5f);

	float2 reflect_coords = screen_uv;
	float2 refract_coords = float2(screen_uv.x, 1.0f - screen_uv.y);

	float2 distortion0 = float2(0.0f, 0.0f); //water_dudv_texture.Sample(water_dudv_sampler, uv0).rg * 2.0f - 1.0f;
	float2 distortion1 = water_dudv_texture.Sample(water_dudv_sampler, uv1).rg * 2.0f - 1.0f;

	float2 distortion = distortion0 + distortion1;

	reflect_coords += distortion * distortion_strength;
	refract_coords += distortion * distortion_strength;
	reflect_coords = clamp(reflect_coords, 0.001f, 0.999f);
	refract_coords = clamp(refract_coords, 0.001f, 0.999f);

	float4 reflection_color = reflection_texture.Sample(reflection_sampler, reflect_coords);
	float4 refraction_color = refraction_texture.Sample(refraction_sampler, refract_coords);

	float4 color = lerp(reflection_color, refraction_color, 0.5f);
	color = lerp(color, water_color, 0.12f);

	return color;
}