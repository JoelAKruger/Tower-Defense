struct profile_entry
{
    const char* Label;
    u64 TicksInclusive;
    u64 TicksExclusive;
    u64 Hits;
};

static profile_entry GlobalProfileEntries[128];
static u64 GlobalProfileStartTime;

static u32 GlobalProfilerParent;

class profile_timer
{
    public:
    
    profile_timer(const char* Label, int Index)
    {
        ParentIndex = GlobalProfilerParent;
        this->Index = Index;
        this->Label = Label;
        
        profile_entry* Entry = GlobalProfileEntries + Index;
        OldTicksInclusive = Entry->TicksInclusive;
        
        GlobalProfilerParent = Index;
        
        Start = ReadCPUTimer();
    }
    
    ~profile_timer()
    {
        u64 Ticks = ReadCPUTimer() - Start;
        GlobalProfilerParent = ParentIndex;
        
        profile_entry* Parent = GlobalProfileEntries + ParentIndex;
        profile_entry* Entry = GlobalProfileEntries + Index;
        
        Parent->TicksExclusive -= Ticks;
        Entry->TicksExclusive += Ticks;
        Entry->TicksInclusive = OldTicksInclusive + Ticks;
        
        Entry->Hits++;
        Entry->Label = Label;
    }
    
    const char* Label;
    u64 Start;
    u64 OldTicksInclusive;
    u64 Hits;
    u32 Index;
    u32 ParentIndex;
};

#define TimeBlock(name) profile_timer _block_timer(name, __COUNTER__ + 1)
#define TimeFunction TimeBlock(__FUNCTION__)

static inline void BeginProfile()
{
    GlobalProfileStartTime = ReadCPUTimer();
}