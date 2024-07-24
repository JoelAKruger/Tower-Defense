struct VS_Input
{
    float3 pos : POS;
    float4 color : COL;
};

struct VS_Output
{
    float4 position : SV_POSITION;
    float4 color : COL;
};

cbuffer Transform
{
	float4x4 transform;
};

VS_Output vs_main(VS_Input input)
{
    VS_Output output;
	output.position = mul(float4(input.pos, 1.0f), transform);
    output.color = input.color;    

    return output;
}

float4 ps_main(VS_Output input) : SV_TARGET
{
    return input.color;   
}