struct file_reader
{
    char* At;
    char* End;
};

static f32
ParseFloat(file_reader* Reader)
{
    //Parse sign
    f32 Sign = 1.0f;
    if (*Reader->At == '-')
    {
        Reader->At++;
        Sign = -1.0f;
    }
    else if (*Reader->At == '+')
    {
        Reader->At++;
    }
    
    f64 Value = 0.0;
    bool AfterDecimalPoint = false;
    f32 Position = 0.1f;
    while (true)
    {
        char C = *Reader->At;
        if (C >= '0' && C <= '9')
        {
            if (AfterDecimalPoint)
            {
                Value = Value + Position * (C - '0');
                Position *= 0.1f;
            }
            else
            {
                Value = Value * 10 + (C - '0');
            }
        }
        else if (C == '.' && !AfterDecimalPoint)
        {
            AfterDecimalPoint = true;
        }
        else
        {
            break;
        }
        
        Reader->At++;
    }
    
    return (f32)(Sign * Value);
}

static i32
ParseInt(file_reader* Reader)
{
    i32 Result = 0;
    while (*Reader->At >= '0' && *Reader->At <= '9')
    {
        Result = Result * 10 + (*Reader->At++) - '0';
    }
    return Result;
}

static bool
AtEnd(file_reader* Reader)
{
    return (Reader->At >= Reader->End);
}

static bool
Consume(file_reader* Reader, string String)
{
    if (Reader->At + String.Length >= Reader->End)
    {
        return false;
    }
    
    for (u32 I = 0; I < String.Length; I++)
    {
        if (Reader->At[I] != String.Text[I])
        {
            return false;
        }
    }
    
    Reader->At += String.Length;
    return true;
}

static bool
Consume(file_reader* Reader, char* Str)
{
    return Consume(Reader, String(Str));
}

static void
NextLine(file_reader* Reader)
{
    while (!AtEnd(Reader))
    {
        if (*Reader->At++ == '\n')
        {
            return;
        }
    }
}

struct
obj_file_vertex
{
    i32 PositionIndex;
    i32 TexCoordIndex;
    i32 NormalIndex;
};

struct obj_file_face
{
    obj_file_vertex Vertices[3];
};

static obj_file_vertex
ParseVertex(file_reader* Reader)
{
    obj_file_vertex Result = {};
    Result.PositionIndex = ParseInt(Reader);
    Consume(Reader, "/");
    Result.TexCoordIndex = ParseInt(Reader);
    Consume(Reader, "/");
    Result.NormalIndex   = ParseInt(Reader);
    return Result;
}

static span<model_vertex>
LoadModel(allocator Allocator, char* Path)
{
    span<u8> File = PlatformLoadFile(Allocator.Transient, Path);
    file_reader Reader = {(char*)File.Memory, (char*)File.Memory + File.Count};
    
    //First pass: Count elements
    u32 PositionCount = 0;
    u32 NormalCount = 0;
    u32 FaceCount = 0;
    
    while (!AtEnd(&Reader))
    {
        //Vertex position
        if (Consume(&Reader, "v "))
        {
            PositionCount++;
        }
        //Vertex texture coord
        else if (Consume(&Reader, "vt"))
        {
            Assert(0);
        }
        //Vertex normal
        else if (Consume(&Reader, "vn"))
        {
            NormalCount++;
        }
        //Face
        else if (Consume(&Reader, "f"))
        {
            FaceCount++;
        }
        //Comment
        else if (Consume(&Reader, "#"))
        {
        }
        //Object name
        else if (Consume(&Reader, "o"))
        {
        }
        //Smooth shading
        else if (Consume(&Reader, "s"))
        {
        }
        else
        {
            Assert(0);
        }
        NextLine(&Reader);
    }
    
    //Second pass: Get Data
    span<v3> Positions = AllocSpan(Allocator.Transient, v3, PositionCount);
    span<v3> Normals = AllocSpan(Allocator.Transient, v3, NormalCount);
    span<obj_file_face> Faces = AllocSpan(Allocator.Transient, obj_file_face, FaceCount);
    
    u32 PositionIndex = 0;
    u32 NormalIndex = 0;
    u32 FaceIndex = 0;
    
    Reader = {(char*)File.Memory, (char*)File.Memory + File.Count};
    while (!AtEnd(&Reader))
    {
        //Vertex position
        if (Consume(&Reader, "v "))
        {
            v3 Position;
            Position.X = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Position.Y = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Position.Z = ParseFloat(&Reader);
            Positions[PositionIndex++] = Position;
        }
        //Vertex texture coord
        else if (Consume(&Reader, "vt"))
        {
            Assert(0);
        }
        //Vertex normal
        else if (Consume(&Reader, "vn "))
        {
            v3 Normal;
            Normal.X = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Normal.Y = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Normal.Z = ParseFloat(&Reader);
            Normals[NormalIndex++] = Normal;
        }
        //Face
        else if (Consume(&Reader, "f "))
        {
            obj_file_face Face;
            Face.Vertices[0] = ParseVertex(&Reader);
            Consume(&Reader, " ");
            Face.Vertices[1] = ParseVertex(&Reader);
            Consume(&Reader, " ");
            Face.Vertices[2] = ParseVertex(&Reader);
            Faces[FaceIndex++] = Face;
        }
        NextLine(&Reader);
    }
    
    //Create vertex data
    /* Not optimised! (TODO: Optimise) */
    u32 VertexCount = FaceCount * 3;
    
    span<model_vertex> Vertices = AllocSpan(Allocator.Permanent, model_vertex, VertexCount);
    
    for (u32 FaceIndex = 0; FaceIndex < FaceCount; FaceIndex++)
    {
        obj_file_face* Face = Faces + FaceIndex;
        
        //Flip direction of vertices to convert to left-handed
        Vertices[FaceIndex * 3 + 0] = {Positions[Face->Vertices[2].PositionIndex - 1], Normals[Face->Vertices[2].NormalIndex - 1]};
        Vertices[FaceIndex * 3 + 1] = {Positions[Face->Vertices[1].PositionIndex - 1], Normals[Face->Vertices[1].NormalIndex - 1]};
        Vertices[FaceIndex * 3 + 2] = {Positions[Face->Vertices[0].PositionIndex - 1], Normals[Face->Vertices[0].NormalIndex - 1]};
    }
    
    return Vertices;
}