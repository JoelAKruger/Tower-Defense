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
	float3 camera_pos;
	float3 light_direction;
	float3 light_color;

	float3 Albedo;
    float Roughness;
    float Metallic;
    float Occlusion;
    float3 FresnelColor;
};

float GetShadowValue(float2 shadow_uv, float pixel_depth);
float GetShadowValueBetter(float2 shadow_uv, float pixel_depth, float3 normal);

Texture2D default_texture : register(t0);
SamplerState default_sampler : register(s0);

Texture2D shadow_map : register(t1);
SamplerComparisonState shadow_comparer : register(s1);
SamplerState shadow_sampler : register(s2);

Texture2D reflection_texture   : register(t2);
Texture2D refraction_texture   : register(t3);
Texture2D water_dudv_texture   : register(t4);
Texture2D water_flow_texture   : register(t5);
Texture2D water_normal_texture : register(t6);

Texture2D ambient_texture  : register(t7);
Texture2D diffuse_texture  : register(t8);
Texture2D normal_texture   : register(t9);
Texture2D specular_texture : register(t10);

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
	output.color = input.color + color + float4(Albedo, 1);
	output.normal = normalize((float3)mul(float4(input.normal, 0.0f), model_to_world));
	output.uv = input.uv;
	
	return output;
}

//Only depth
float4 VertexShader_OnlyDepth(VS_Input input) : SV_POSITION
{
	float4 world_pos = mul(float4(input.pos, 1.0f), model_to_world);
	float4 pos_clip = mul(world_pos, world_to_clip);
	return pos_clip;
}

void PixelShader_OnlyDepth() {}

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
	float shadow = GetShadowValueBetter(shadow_uv, pixel_depth, input.normal);

	return float4(input.color.rgb * (1.0f - 0.25 * shadow), 1.0f);
}

float trowbridge_reitz_normal_distribution(float n_dot_h, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH2 = n_dot_h * n_dot_h;
    float denominator = 3.141592f * pow((alpha2 - 1) * NdotH2 + 1, 2);
    return alpha2 / denominator;
}

float3 fresnel(float3 F0, float NdotV, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(1.0 - NdotV, 5.0);
}

float schlick_beckmann_geometry_attenuation_function(float dotProduct, float roughness)
{
    float alpha = roughness * roughness;
    float k = alpha * 0.797884560803;  // sqrt(2 / PI)
    return dotProduct / (dotProduct * (1 - k) + k);
}

float3 lambert_diffuse(float3 albedo)
{
    return albedo / 3.141592f;
}

float3 brdf_unity(float3 albedo, float3 world_pos, float3 light_dir, float3 normal)
{
	float3 light_vec = -1.0f * light_dir;
	float3 view_vec = normalize(camera_pos - world_pos);
	float3 half_vec = normalize(light_vec + view_vec);

	float3 reflect_vec = -1.0f * reflect(view_vec, normal);

	float n_dot_l = max(dot(normal, light_vec), 0.0f);
	float n_dot_h = max(dot(normal, half_vec), 0.0f);
	float h_dot_v = max(dot(half_vec, view_vec), 0.0f);
	float n_dot_v = max(dot(normal, view_vec), 0.0f);

	float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), FresnelColor * albedo, Metallic);

	float d = trowbridge_reitz_normal_distribution(n_dot_h, Roughness);
	float3 f = fresnel(f0, n_dot_v, Roughness);
	float g = schlick_beckmann_geometry_attenuation_function(n_dot_v, Roughness) * schlick_beckmann_geometry_attenuation_function(n_dot_l, Roughness);

	float lambert_direct = max(dot(normal, light_vec), 0.0f);
	float3 direct_radiance = light_color * Occlusion;

	float3 diffuse_direct_term = lambert_diffuse(albedo) * (1.0f - f) * (1.0f - Metallic) * albedo;
	float3 specular_direct_term = g * d * f / (4.0f * n_dot_v * n_dot_l + 0.00001f);

	float3 brdf_direct_output  = (diffuse_direct_term + specular_direct_term) * lambert_direct * direct_radiance;
	return brdf_direct_output;
}

float4 PixelShader_PBR(VS_Output_Default input) : SV_TARGET
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
	float shadow = GetShadowValueBetter(shadow_uv, pixel_depth, input.normal);
	
	float ambient = 0.08f;

	float3 result = float3(0, 0, 0);
	result += ambient * input.color * light_color;
	result += 0.9f * (brdf_unity(input.color, input.pos_world, light_direction, input.normal) * (1.0f - 0.8f * shadow));

	return float4(result, 1.0f);
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

	float diffuse = 0.5f + 0.5f * dot(input.normal, -1.0f * light_direction);

	float3 view_dir = normalize(camera_pos - input.pos_world);
	float3 reflect_dir = reflect(light_direction, input.normal);

	float specular = pow(max(dot(view_dir, reflect_dir), 0.0), 32);

	float shadow = GetShadowValueBetter(shadow_uv, pixel_depth, input.normal);

	float light = ambient + diffuse * (1.0f - 0.8f * shadow) + specular * (1.0f - shadow);

	return float4(light * input.color.rgb, input.color.a);
	//return float4(float3(0.5f, 0.5f, 0.5f) + 0.5f * input.normal, 1.0f);
}

float4 PixelShader_Water(VS_Output_Default input) : SV_Target
{
	float2 shadow_uv;
	shadow_uv.x = 0.5f + (input.pos_light_space.x / input.pos_light_space.w * 0.5f);
	shadow_uv.y = 0.5f - (input.pos_light_space.y / input.pos_light_space.w * 0.5f);
	float pixel_depth = input.pos_light_space.z / input.pos_light_space.w;
	float shadow = GetShadowValueBetter(shadow_uv, pixel_depth, input.normal);

	float4 water_color = float4(float3(0.0f, 0.2f, 0.05f) * (1.0f - 0.1f * shadow), 1.0f);
	float distortion_strength = 0.01f;
	float distortion_scaling = 800.0f;
	float wave_speed = 0.03f;

	float2 flow_map = water_flow_texture.Sample(default_sampler, input.uv).rg * 2.0f + float2(-1.0f, -1.0f);
	float phase0 = frac((time / 5.0f));
	float phase1 = frac((time / 5.0f) + 0.5f);
	float t = abs(2.0 * frac(time / 5.0f) - 1.0);

	float2 uv0 = frac(input.uv * distortion_scaling + 5.0f * wave_speed * phase0 * flow_map);
	float2 uv1 = frac(input.uv * distortion_scaling + 5.0f * wave_speed * phase1 * flow_map + float2(0.5f, 0.5f));

	float2 screen_uv = 0.5f * (input.pos_clip.xy / input.pos_clip.w) + float2(0.5f, 0.5f);

	float2 reflect_coords = screen_uv;
	float2 refract_coords = float2(screen_uv.x, 1.0f - screen_uv.y);

	float2 distortion0 = water_dudv_texture.Sample(default_sampler, uv0).rg * 2.0f - 1.0f;
	float2 distortion1 = water_dudv_texture.Sample(default_sampler, uv1).rg * 2.0f - 1.0f;

	float3 normal0 = water_normal_texture.Sample(default_sampler, uv0).rgb * 2.0f - 1.0f;
	float3 normal1 = water_normal_texture.Sample(default_sampler, uv1).rgb * 2.0f - 1.0f;
	normal0 = normalize(normal0);
	normal1 = normalize(normal1);

	float3 light_dir = normalize(light_direction);
	float3 view_dir = normalize(camera_pos - input.pos_world);

	float refractive_factor = dot(view_dir, input.normal);
	refractive_factor = pow(refractive_factor, 0.5f);

	float3 reflect_dir = reflect(light_dir, normal0);
	float specular0 = pow(max(dot(view_dir, reflect_dir), 0.0), 32);	

	reflect_dir = reflect(light_dir, normal1);
	float specular1 = pow(max(dot(view_dir, reflect_dir), 0.0), 32);	

	float2 distortion = distortion0 * (1.0f - t) + distortion1 * t;
	float specular = specular0 * (1.0f - t) + specular1 * t;

	float distortion_amount = refractive_factor;

	reflect_coords += distortion_amount * distortion * distortion_strength;
	refract_coords += distortion_amount * distortion * distortion_strength;
	reflect_coords = clamp(reflect_coords, 0.001f, 0.999f);
	refract_coords = clamp(refract_coords, 0.001f, 0.999f);

	float4 reflection_color = reflection_texture.Sample(default_sampler, reflect_coords);
	float4 refraction_color = refraction_texture.Sample(default_sampler, refract_coords);

	float4 color = lerp(reflection_color, refraction_color, refractive_factor);
	color = lerp(color, water_color, 0.1f) + 0.2f * specular * float4(1.0f, 1.0f, 1.0f, 1.0f) * (1.0f - shadow);
	
	return color;
}


float4 PixelShader_TexturedModel(VS_Output_Default input) : SV_Target
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

	float ambient = (float)ambient_texture.Sample(default_sampler, input.uv);

	float3 normal = normal_texture.Sample(default_sampler, input.uv);
	normal = 2.0f * normal - float3(1.0f, 1.0f, 1.0f);

	float3 light_dir = 1.0f * normalize(float3(1.0f, -1.0f, 1.0f)); //TODO: make this a constant!
	float diffuse = 0.5f + 0.5f * dot(normal, light_dir);

	float3 view_dir = normalize(camera_pos - input.pos_world);	

	float3 reflect_dir = reflect(light_dir, normal);

	float shinyness = 8.0f;
	float specular = specular_texture.Sample(default_sampler, input.uv) * pow(max(dot(view_dir, reflect_dir), 0.0), shinyness);

	float shadow = GetShadowValue(shadow_uv, pixel_depth);

	float light = ambient + shadow * (diffuse + specular);
	float3 color = diffuse_texture.Sample(default_sampler, input.uv);

	return float4((ambient + (1.0f - shadow) * (diffuse + specular)) * color, 1.0f);
}

float sdr_to_hdr(float value)
{
	if (value < 0.95f) return value;
	return pow(value, 4) + 0.14f;
}

float4 PixelShader_Texture(VS_Output_Default input) : SV_Target
{
	float clip_plane_distance = dot(clip_plane.xyz, input.pos_world) + clip_plane.w;
	if (clip_plane_distance < 0)
	{
		discard;
	}

	float4 sample = color * default_texture.Sample(default_sampler, input.uv);
	if (sample.a == 0.0f)
	{
		discard;
	}

	return sample;
}

//Helper functions
float GetShadowValue(float2 shadow_uv, float pixel_depth)
{
	float light = 0.0f;
	
	float lit = shadow_map.SampleCmpLevelZero(shadow_comparer, shadow_uv, pixel_depth - 0.000f);

	return 1.0f - lit;
}

float GetShadowValueBetter(float2 shadow_uv, float pixel_depth, float3 normal)
{
	float light = 0.0f;

	float cos_angle = dot(normal, light_direction);
	if (cos_angle > 0.0f)
	{
		return 1.0f;
	}

	float lit = (float)shadow_map.SampleCmpLevelZero(shadow_comparer, shadow_uv, pixel_depth - 0.001f);

	float sample = shadow_map.Sample(shadow_sampler, shadow_uv);
	if (sample == 1.0f)
	{
		lit = 1.0f;
	}

	if (lit > 0.0f)
	{
		if (cos_angle > -3.0f/10.0f)
		{
			float shadow = 1.0f + cos_angle * 10.0f/3.0f;

			return shadow;
		}
	}

	return 1.0f - lit;
}
