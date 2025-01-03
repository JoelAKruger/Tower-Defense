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
};

float GetShadowValue(float2 shadow_uv, float pixel_depth);
float GetShadowValueBetter(float2 shadow_uv, float pixel_depth, float3 normal);

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
   
	float epsilon = 0.0001f;
	float lighting = float(shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, pixel_depth - epsilon));

	if (lighting == 0.0f)
	{
		input.color.rgb *= 0.2f;
	}

	return input.color;
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

	float lit = (1.0f - 0.8f * shadow);
	
	float3 light_vec = -1.0f * light_direction;
	float3 view_vec = normalize(camera_pos - input.pos_world);
	float3 half_vec = normalize(light_vec + view_vec);

	float3 reflect_vec = -1.0f * reflect(view_vec, input.normal);

	float n_dot_l = max(dot(input.normal, light_vec), 0.0f);
	float n_dot_h = max(dot(input.normal, half_vec), 0.0f);
	float h_dot_v = max(dot(half_vec, view_vec), 0.0f);
	float n_dot_v = max(dot(input.normal, view_vec), 0.0f);

	float3 albedo = input.color;

	//Hard code these for now
	float roughness = 0.9f;
	float metallic = 0.0f;
	float occlusion = 1.0f;
	float3 light_color = float3(1.0f, 1.0f, 1.0f);
	float3 fresnel_color = float3(1.0f, 1.0f, 1.0f);

	float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), fresnel_color * albedo, metallic);

	float d = trowbridge_reitz_normal_distribution(n_dot_h, roughness);
	float3 f = fresnel(f0, n_dot_v, roughness);
	float g = schlick_beckmann_geometry_attenuation_function(n_dot_v, roughness) * schlick_beckmann_geometry_attenuation_function(n_dot_l, roughness);

	float lambert_direct = max(dot(input.normal, light_vec), 0.0f);
	float3 direct_radiance = light_color * occlusion;

	float3 diffuse_direct_term = lambert_diffuse(albedo) * (1.0f - f) * (1.0f - metallic) * input.color;
	float3 specular_direct_term = g * d * f / (4.0f * n_dot_v * n_dot_l + 0.00001f);

	float3 brdf_direct_output  = (diffuse_direct_term + specular_direct_term) * lambert_direct * direct_radiance * lit;

	return float4(brdf_direct_output, 1.0f);

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

Texture2D reflection_texture : register(t2);
SamplerState reflection_sampler : register(s2);

Texture2D refraction_texture : register(t3);
SamplerState refraction_sampler : register(s3);

Texture2D water_dudv_texture : register(t4);
SamplerState water_dudv_sampler : register(s4);

Texture2D water_flow_texture : register(t5);
SamplerState water_flow_sampler : register(s5);

Texture2D water_normal_texture : register(t6);
SamplerState water_normal_sampler : register(s6);

float4 PixelShader_Water(VS_Output_Default input) : SV_Target
{
	float4 water_color = float4(0.0f, 0.2f, 0.05f, 1.0f);
	float distortion_strength = 0.01f;
	float distortion_scaling = 8.0f;
	float wave_speed = 0.03f;

	float2 flow_map = water_flow_texture.Sample(water_flow_sampler, input.uv).rg * 2.0f + float2(-1.0f, -1.0f);
	float phase0 = frac((time / 5.0f));
	float phase1 = frac((time / 5.0f) + 0.5f);
	float t = abs(2.0 * frac(time / 5.0f) - 1.0);

	float2 uv0 = frac(input.uv * distortion_scaling + 5.0f * wave_speed * phase0 * flow_map);
	float2 uv1 = frac(input.uv * distortion_scaling + 5.0f * wave_speed * phase1 * flow_map + float2(0.5f, 0.5f));

	float2 screen_uv = 0.5f * (input.pos_clip.xy / input.pos_clip.w) + float2(0.5f, 0.5f);

	float2 reflect_coords = screen_uv;
	float2 refract_coords = float2(screen_uv.x, 1.0f - screen_uv.y);

	float2 distortion0 = water_dudv_texture.Sample(water_dudv_sampler, uv0).rg * 2.0f - 1.0f;
	float2 distortion1 = water_dudv_texture.Sample(water_dudv_sampler, uv1).rg * 2.0f - 1.0f;

	float3 normal0 = water_normal_texture.Sample(water_normal_sampler, uv0).rgb * 2.0f - 1.0f;
	float3 normal1 = water_normal_texture.Sample(water_normal_sampler, uv1).rgb * 2.0f - 1.0f;
	normal0 = normalize(normal0);
	normal1 = normalize(normal1);

	float3 light_dir = normalize(float3(1.0f, 1.0f, 1.0f));
	float3 view_dir = normalize(camera_pos - input.pos_world);
	float3 reflect_dir = reflect(light_dir, normal0);
	float specular0 = pow(max(dot(view_dir, reflect_dir), 0.0), 32);	

	reflect_dir = reflect(light_dir, normal1);
	float specular1 = pow(max(dot(view_dir, reflect_dir), 0.0), 32);	

	float2 distortion = distortion0 * (1.0f - t) + distortion1 * t;
	float specular = specular0 * (1.0f - t) + specular1 * t;

	reflect_coords += distortion * distortion_strength;
	refract_coords += distortion * distortion_strength;
	reflect_coords = clamp(reflect_coords, 0.001f, 0.999f);
	refract_coords = clamp(refract_coords, 0.001f, 0.999f);

	float4 reflection_color = reflection_texture.Sample(reflection_sampler, reflect_coords);
	float4 refraction_color = refraction_texture.Sample(refraction_sampler, refract_coords);

	float4 color = lerp(reflection_color, refraction_color, 0.5f);
	color = lerp(color, water_color, 0.12f) + 0.2f * specular * float4(1.0f, 1.0f, 1.0f, 1.0f);
	
	return color;
}

Texture2D ambient_texture : register(t7);
SamplerState ambient_sampler : register(s7);

Texture2D diffuse_texture : register(t8);
SamplerState diffuse_sampler : register(s8);

Texture2D normal_texture : register(t9);
SamplerState normal_sampler : register(s9);

Texture2D specular_texture : register(t10);
SamplerState specular_sampler : register(s10);

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

	float ambient = (float)ambient_texture.Sample(ambient_sampler, input.uv);

	float3 normal = normal_texture.Sample(normal_sampler, input.uv);
	normal = 2.0f * normal - float3(1.0f, 1.0f, 1.0f);

	float3 light_dir = 1.0f * normalize(float3(1.0f, -1.0f, 1.0f)); //TODO: make this a constant!
	float diffuse = 0.5f + 0.5f * dot(normal, light_dir);

	float3 view_dir = normalize(camera_pos - input.pos_world);	

	float3 reflect_dir = reflect(light_dir, normal);

	float shinyness = 8.0f;
	float specular = specular_texture.Sample(specular_sampler, input.uv) * pow(max(dot(view_dir, reflect_dir), 0.0), shinyness);

	float shadow = GetShadowValue(shadow_uv, pixel_depth);

	float light = ambient + shadow * (diffuse + specular);
	float3 color = diffuse_texture.Sample(diffuse_sampler, input.uv);

	return float4((ambient + (1.0f - shadow) * (diffuse + specular)) * color, 1.0f);
}

Texture2D default_texture : register(t0);
SamplerState default_sampler : register(s0);

float4 PixelShader_Texture(VS_Output_Default input) : SV_Target
{
	float4 sample = default_texture.Sample(default_sampler, input.uv);
	return sample;
}

//Helper functions
float GetShadowValue(float2 shadow_uv, float pixel_depth)
{
	float light = 0.0f;
	
	float lit = shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, pixel_depth - 0.000f);

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

	float lit = shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_uv, pixel_depth - 0.001f);

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
