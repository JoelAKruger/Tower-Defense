#include <stdint.h>
#include <cstdarg>
#include <stdio.h>

//TODO: This does not seem to actually calculate the length at compile time :(
constexpr string
String(const char* Text)
{
	string Result = {};
	Result.Text = (char*)Text;
    
	u32 Length = 0;
	while (Text[Length])
		Length++;
    
	Result.Length = Length;
    
	return Result;
}

#if 0
#define String(x) \
string{x, sizeof(x) - 1}
#endif

static bool
StringsAreEqual(string A, string B)
{
    if (A.Length != B.Length)
        return false;
    
    for (u32 I = 0; I < A.Length; I++)
    {
        if (A.Text[I] != B.Text[I])
            return false;
    }
    
    return true;
}

#define AllocStruct(Arena, Type) \
(Type*)Alloc(Arena, sizeof(Type))

#define AllocArray(Arena, Type, Count) \
(Type*)Alloc(Arena, Count * sizeof(Type))

static u8*
Alloc(memory_arena* Arena, u64 Size)
{
    Arena->Used = (Arena->Used + 7) & ~0b111;
    
	u8* Result = Arena->Buffer + Arena->Used;
	Arena->Used += Size;
    
    Assert(Arena->Used < Arena->Size);
    
	for (int i = 0; i < Size; i++)
		Result[i] = 0;
    
    return Result;
}

static u8*
ArenaAt(memory_arena* Arena)
{
    return Arena->Buffer + Arena->Used;
}

static u64
ArenaFreeSpace(memory_arena* Arena)
{
    return Arena->Size - Arena->Used;
}

static bool
WasAllocatedFrom(memory_arena* Arena, void* Memory)
{
    size_t ArenaStart = (size_t)Arena->Buffer;
    size_t ArenaEnd = (size_t)Arena->Buffer + Arena->Used;
    
    size_t Ptr = (size_t)Memory;
    
    bool Result = (Ptr >= ArenaStart && Ptr <= ArenaEnd);
    return Result;
}

static inline memory_arena
CreateSubArena(memory_arena* Arena, u64 Size, memory_arena_type Type = PERMANENT)
{
    memory_arena Result = {};
    Result.Buffer = Alloc(Arena, Size);
    Result.Size = Size;
    Result.Type = Type;
    return Result;
}

static inline void
ResetArena(memory_arena* Arena)
{
#if DEBUG
	for (int i = 0; i < Arena->Used; i++)
		Arena->Buffer[i] = 0xFF;
#endif
    
	Arena->Used = 0;
}

struct context
{
    allocator Allocator;
};

static string
ArenaPrintInternal(memory_arena* Arena, char* Format, va_list Args)
{
    string Result = {};
	char* Buffer = (char*)(Arena->Buffer + Arena->Used);
    
    u64 MaxChars = 4096;
    if (Arena->Size - Arena->Used < MaxChars)
    {
        MaxChars = Arena->Size - Arena->Used;
    }
    
	int CharsWritten = vsprintf_s(Buffer, MaxChars, Format, Args);
	Arena->Used += CharsWritten + 1; //TODO: Arena used should be rounded up to a nice number
    
	Result.Text = Buffer;
	Result.Length = CharsWritten;
    
	Assert(Arena->Used < Arena->Size);
    
    return Result;
}

static string
ArenaPrint(memory_arena* Arena, char* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
    string Result = ArenaPrintInternal(Arena, Format, Args);
    va_end(Args);
    return Result;
}

static string
CopyString(memory_arena* Arena, string String)
{
    string Result = {};
    Result.Text = (char*)Alloc(Arena, String.Length + 1);
    Result.Length = String.Length;
    memcpy(Result.Text, String.Text, String.Length);
    Result.Text[String.Length] = 0;
    return Result;
}

static u8*
Copy(memory_arena* Arena, u8* Source, u64 Bytes)
{
    u8* Dest = Alloc(Arena, Bytes);
    
    for (u64 I = 0; I < Bytes; I++)
    {
        Dest[I] = Source[I];
    }
    
    return Dest;
}

template <typename type>
type* begin(span<type> Span)
{
    return Span.Memory;
}

template <typename type>
type* end(span<type> Span)
{
    return Span.Memory + Span.Count;
}

#define AllocSpan(Arena, Type, Count) \
(span<Type> {AllocArray(Arena, Type, Count), Count})
/*
template <typename type>
span<type>
BeginSpan(memory_arena* Arena)
{
    span<type> Span = {};
    Span.Memory = (type*)(Arena->Buffer + Arena->Used);
    
    return Span;
}
*/
template <typename type>
void
EndSpan(span<type>& Span, memory_arena* Arena)
{
    Assert(WasAllocatedFrom(Arena, Span.Memory));
    Span.Count = (u32)((type*)(Arena->Buffer + Arena->Used) - Span.Memory);
}

template <typename type>
type*
operator+(span<type> Span, u32 Index)
{
    return &Span[Index];
}

template <typename type>
struct static_array
{
    type* Memory;
    u64 Count;
    u64 Capacity;
    
    type& operator[](u32 Index)
    {
        Assert(Index < Count);
        return Memory[Index];
    }
};

template<typename type>
static type* begin(static_array<type> Array)
{
    return Array.Memory;
}

template<typename type>
static type* end(static_array<type> Array)
{
    return Array.Memory + Array.Count;
}

template <typename type>
static inline u32 Append(static_array<type>* Array, type NewElement)
{
    Assert(Array->Count < Array->Capacity);
    Array->Memory[Array->Count] = NewElement;
    u32 Result = Array->Count;
    Array->Count++;
    return Result;
}

template <typename type>
type* operator+(static_array<type> Array, u32 Index)
{
    return &Array[Index];
}

#define AllocStaticArray(Arena, Type, Capacity) \
(static_array<Type> {AllocArray(Arena, Type, Capacity), 0, Capacity})

template <typename type>
static span<type> ToSpan(static_array<type> Array)
{
    span<type> Span = {};
    Span.Memory = Array.Memory;
    Span.Count = Array.Count;
    return Span;
}

template <typename type>
struct dynamic_array
{
    memory_arena* Arena;
    type* Memory;
    u32 Count;
    u32 Capacity;
    
    type* begin()
    {
        return Memory;
    }
    
    type* end()
    {
        return Memory + Count;
    }
    
    type& operator[] (u32 Index)
    {
        Assert(Index < Count);
        return Memory[Index];
    }
    
};

template <typename type>
type* operator+(dynamic_array<type> Array, u32 Index)
{
    return &Array[Index];
}

template <typename type>
void Append(dynamic_array<type>* Array, type NewElement)
{
    if (Array->Count >= Array->Capacity)
    {
        //Note: Old memory is not freed
        u32 NewCapacity = Max(Array->Capacity * 2, 4);
        type* NewMemory = AllocArray(Array->Arena, type, NewCapacity);
        memcpy(NewMemory, Array->Memory, Array->Count * sizeof(type));
        
        Array->Memory = NewMemory;
        Array->Capacity = NewCapacity;
    }
    Array->Memory[Array->Count++] = NewElement;
}

template <typename type>
void Clear(dynamic_array<type>* Array)
{
    Array->Memory = 0;
    Array->Count = 0;
    Array->Capacity = 0;
}

template <typename type>
static span<type> ToSpan(dynamic_array<type> Array)
{
    span<type> Span = {};
    Span.Memory = Array.Memory;
    Span.Count = Array.Count;
    return Span;
}

template <typename type>
static inline dynamic_array<type>
Array(span<type> Span)
{
    dynamic_array<type> Array = {};
    Array.Memory = Span.Memory;
    Array.Count = Span.Count;
    Array.Capacity = Span.Count;
    return Array;
}

static u32 
StringToU32(string String)
{
    u32 Result = 0;
    
    if (String.Length >= 2 && 
        String.Text[0] == '0' &&
        String.Text[1] == 'x')
    {
        for (u32 I = 2; I < String.Length; I++)
        {
            char C = String.Text[I];
            if (C >= 'A' && C <= 'F')
            {
                Result = Result * 16 + (10 + C - 'A');
            }
            else if (C >= 'a' && C <= 'f')
            {
                Result = Result * 16 + (10 + C - 'a');
            }
            else if (C >= '0' && C <= '9')
            {
                Result = Result * 16 + (C - '0');
            }
            else
            {
                Assert(0);
            }
        }
    }
    else
    {
        for (u32 I = 0; I < String.Length; I++)
        {
            char C = String.Text[I];
            Assert(C >= '0' && C <= '9');
            Result = Result * 10 + (C - '0');
        }
    }
    return Result;
}

template <typename type>
class array_2d
{
    public:
    type* Memory;
    u64 Rows;
    u64 Cols;
    
    type Get(u64 X, u64 Y)
    {
        Assert(X < Cols);
        Assert(Y < Rows);
        type* Ptr = Memory + Y * Cols + X;
        return *Ptr;
    }
    
    void Set(u64 X, u64 Y, type& Value)
    {
        Assert(X < Cols);
        Assert(Y < Rows);
        type* Ptr = Memory + Y * Cols + X;
        *Ptr = Value;
    }
    
    type* At(u64 X, u64 Y)
    {
        Assert(X < Cols);
        Assert(Y < Rows);
        type* Ptr = Memory + Y * Cols + X;
        return Ptr;
    }
};

template <typename type>
array_2d<type>
AllocArray2D(memory_arena* Arena, u32 Rows, u32 Cols)
{
    array_2d<type> Result = {};
    
    Result.Memory = (type*)Alloc(Arena, sizeof(type) * Rows * Cols);
    Result.Rows = Rows;
    Result.Cols = Cols;
    
    return Result;
}

struct mutex
{
    volatile long Value;
};

static void
Lock(mutex* Mutex)
{
    while (_InterlockedCompareExchange(&Mutex->Value, 1, 0) == 1)
    {
        _mm_pause();
    }
}

static void
Unlock(mutex* Mutex)
{
    Mutex->Value = 0;
}

template <typename type>
u64
IndexOfCounted(type Target, type* Array, u64 Count)
{
    for (u64 I = 0; I < Count; I++)
    {
        if (Array[I] == Target)
        {
            return I;
        }
    }
    
    Assert(0);
    
    return 0;
}

template <typename type>
bool
Contains(type Target, type* Array, u64 Count)
{
    for (u64 I = 0; I < Count; I++)
    {
        if (Array[I] == Target)
        {
            return true;
        }
    }
    
    return false;
}

template <typename type>
bool
Contains(type Target, span<type> Span)
{
    return Contains(Target, Span.Memory, Span.Count);
}

//Index of target in array
#define IndexOf(target, array) IndexOfCounted(target, array, ArrayCount(array))

