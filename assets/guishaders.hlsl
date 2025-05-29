struct GUI_VS_Input
{
	float2 pos : POS;
	float4 color : COL;
	float2 uv : UV;
};

struct VS_Output
{
	float4 pos : SV_POSITION;
	float4 color : COL;
	float2 uv : TEXCOORD;
};

cbuffer GUI_Constants
{
	float4x4 transform;
};

Texture2D gui_texture : register(t0);
SamplerState default_sampler : register(s0);

VS_Output GUI_VertexShader(GUI_VS_Input input)
{
	VS_Output output;
	output.pos = mul(float4(input.pos, 0.0f, 1.0f), transform);
	output.uv = input.uv;
	output.color = input.color;
	return output;
}

float4 GUI_PixelShader_Color(VS_Output input) : SV_Target
{
	return input.color;
}

float4 GUI_PixelShader_Texture(VS_Output input) : SV_Target
{
	float4 sample = gui_texture.Sample(default_sampler, input.uv);
	return sample;
}

float4 GUI_PixelShader_Font(VS_Output input) : SV_Target
{
	float sample = (float)gui_texture.Sample(default_sampler, input.uv);
	return float4(input.color.rgb, sample);
}

float4 GUI_PixelShader_HDR_To_SDR(VS_Output input) : SV_Target
{
    float exposure   = 1.05f;
    float contrast   = 1.15f;
    float saturation = 1.05f;
    float3 tint      = float3(1.0f, 0.95f, 0.9f);

    float3 color = gui_texture.Sample(default_sampler, input.uv).rgb;


    // Exposure
    color *= exposure;

    // Contrast
    color = (color - 0.5) * contrast + 0.5;

    // Saturation
    float gray = dot(color, float3(0.299, 0.587, 0.114));
    color = lerp(float3(gray, gray, gray), color, saturation);

    // Tint
    color *= tint;

    // Gamma correction and clamping to [0,1]
    color = pow(saturate(color), 1.0 / 2.2);

    return float4(color, 1.0f);
}
