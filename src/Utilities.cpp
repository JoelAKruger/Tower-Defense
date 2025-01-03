#include <stdint.h>
#include <cstdarg>
#include <stdio.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;
typedef double f64;
typedef u32 b32;

#define Kilobytes(n) (n * 1024)
#define Megabytes(n) (n * 1024 * 1024)

#define ArrayCount(x) (sizeof((x))/sizeof((x)[0]))

struct string
{
	char* Text;
	u64 Length;
};

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

#if DEBUG
#define Assert(x) DoAssert(x)
#else
#define Assert(x)
#endif

static inline void
DoAssert(bool Condition)
{
	if (!Condition)
	{
		__debugbreak();
	}
}

enum memory_arena_type
{
	NORMAL, PERMANENT, TRANSIENT
};

struct memory_arena
{
	u8* Buffer;
	u64 Used;
	u64 Size;
	memory_arena_type Type;
};

#define AllocStruct(Arena, Type) \
(Type*)Alloc(Arena, sizeof(Type))

#define AllocArray(Arena, Type, Count) \
(Type*)Alloc(Arena, Count * sizeof(Type))

static u8*
Alloc(memory_arena* Arena, u64 Size)
{
	u8* Result = Arena->Buffer + Arena->Used;
	Arena->Used += Size;
    
	for (int i = 0; i < Size; i++)
		Result[i] = 0;
    
	Assert(Arena->Used < Arena->Size);
    
    //Assert(((size_t)Result & 0b11) == 0);
    
    return Result;
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

struct allocator
{
	memory_arena* Permanent;
	memory_arena* Transient;
};

struct context
{
    allocator Allocator;
};

static string
ArenaPrint(memory_arena* Arena, char* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
    
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
    
	va_end(Args);
    
	Assert(Arena->Used < Arena->Size);
    
	return Result;
}

static string
CopyString(memory_arena* Arena, string String)
{
    string Result = {};
    Result.Text = (char*)Alloc(Arena, String.Length);
    Result.Length = String.Length;
    memcpy(Result.Text, String.Text, String.Length);
    return Result;
}

template <typename type>
struct span
{
	type* Memory;
	u32 Count;
    
    type& operator[](u32 Index)
    {
#if DEBUG
        Assert(Index < Count);
#endif
        return Memory[Index];
    }
};

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
static inline u32 Add(static_array<type>* Array, type NewElement)
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
void Add(dynamic_array<type>* Array, type NewElement)
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