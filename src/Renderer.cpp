static void
SetTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(0, &Transform, sizeof(Transform));
}

static void
SetShaderTime(f32 Time)
{
    f32 Constant[4] = {Time};
    SetVertexShaderConstant(1, Constant, sizeof(Constant));
}

static void
SetModelTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(2, &Transform, sizeof(Transform));
}

static void
SetModelColor(v4 Color)
{
    SetVertexShaderConstant(3, &Color, sizeof(Color));
}

static m4x4
IdentityTransform()
{
    m4x4 Result = {};
    
    Result.Values[0][0] = 1.0f;
    Result.Values[1][1] = 1.0f;
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    
    return Result;
}

static m4x4
ScaleTransform(f32 X, f32 Y, f32 Z)
{
    m4x4 Result = {};
    Result.Values[0][0] = X;
    Result.Values[1][1] = Y;
    Result.Values[2][2] = Z;
    Result.Values[3][3] = 1.0f;
    return Result;
}

static m4x4
TranslateTransform(f32 X, f32 Y, f32 Z)
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[1][1] = 1.0f;
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    Result.Values[3][0] = X;
    Result.Values[3][1] = Y;
    Result.Values[3][2] = Z;
    return Result;
}

static m4x4
ModelRotateTransform()
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[2][1] = 1.0f;
    Result.Values[1][2] = -1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}

static m4x4 
RotateTransform(f32 Radians)
{
    m4x4 Result = {};
    Result.Values[0][0] = cosf(Radians);
    Result.Values[0][1] = -sinf(Radians);
    Result.Values[1][0] = sinf(Radians);
    Result.Values[1][1] = cosf(Radians);
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}