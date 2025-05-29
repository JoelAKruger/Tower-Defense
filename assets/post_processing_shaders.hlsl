struct GUI_VS_Input
{
	float2 pos : POS;
	float4 color : COL;
	float2 uv : UV;
};


Texture2D bloom_input_texture : register(t11);
SamplerState default_sampler : register(s0);

struct Bloom_VS_Output
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer GUI_Constants
{
	float4x4 transform;
};

Bloom_VS_Output Bloom_VertexShader(GUI_VS_Input input)
{
	Bloom_VS_Output output;
	output.pos = mul(float4(input.pos, 0.0f, 1.0f), transform);
	output.uv = input.uv;
	return output;
}

float4 Bloom_PixelShader_Filter(Bloom_VS_Output input) : SV_TARGET
{
	//return bloom_downsample_input_texture.Sample(bloom_downsample_input_sampler, input.uv);

	float3 color = bloom_input_texture.Sample(default_sampler, input.uv);
	color = color - float3(1, 1, 1);
	return float4(color, 1.0f);
}

float4 Bloom_PixelShader_Downsample(Bloom_VS_Output input) : SV_TARGET
{
	float input_texture_width, input_texture_height;
	bloom_input_texture.GetDimensions(input_texture_width, input_texture_height);

	float dudx = 1.0f / input_texture_width;
	float dudy = 1.0f / input_texture_height;

	float contributions[13] = {
		0.03125f, 0.0625f, 0.03125f, 0.125f, 0.125f, 0.0625f, 0.125f, 0.0625f, 0.125f, 0.125f, 0.03125f, 0.0625f, 0.03125f
	};

	float2 coords[13] = {
		{-2, -2}, {0, -2}, {2, -2}, {-1, -1}, {1, -1}, {-2, 0}, {0, 0}, {2, 0}, {-1, 1}, {1, 1}, {-2, 2}, {0, 2}, {2, 2}
	};

	float3 result = float3(0, 0, 0);
	for (int i = 0; i < 13; i++)
	{
		float2 uv = input.uv + float2(coords[i].x * dudx, coords[i].y * dudy);
		result += contributions[i] * bloom_input_texture.Sample(default_sampler, uv);
	}

	return float4(result, 1.0f);
}

float4 Bloom_PixelShader_Upsample(Bloom_VS_Output input) : SV_TARGET
{
	float contributions[9] = {
		0.0625f, 0.125f, 0.0625f, 0.125f, 0.25f, 0.125f, 0.0625f, 0.125f, 0.0625f
	};

	//These are not pixels
	float2 coords[9] = {
		{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}
	};

	float radius = 0.003f;
	float3 result = float3(0, 0, 0);
	for (int i = 0; i < 9; i++)
	{
		float2 uv = input.uv + radius * coords[i];
		result += contributions[i] * bloom_input_texture.Sample(default_sampler, uv);
	}

	return float4(result, 1.0f);
}

