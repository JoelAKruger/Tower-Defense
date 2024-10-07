static void 
CalculateModelVertexNormals(model_triangle* Triangles, u64 TriangleCount)
{
    for (u64 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
    {
        model_triangle* Tri = Triangles + TriangleIndex;
        
        v3 Normal = UnitV(CrossProduct(Tri->Vertices[1].P - Tri->Vertices[0].P, 
                                       Tri->Vertices[2].P - Tri->Vertices[0].P));
        
        Tri->Vertices[0].Normal = Normal;
        Tri->Vertices[1].Normal = Normal;
        Tri->Vertices[2].Normal = Normal;
    }
}