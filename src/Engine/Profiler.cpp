static inline dynamic_array<string> GetProfileReadout(memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    uint64_t TotalTicks = ReadCPUTimer() - GlobalProfileStartTime;
    
    dynamic_array<string> Strings = {};
    Strings.Arena = Arena;
    
    for (u32 EntryIndex = 0; EntryIndex < ArrayCount(GlobalProfileEntries); EntryIndex++)
    {
        profile_entry* Entry = GlobalProfileEntries + EntryIndex;
        
        if (Entry->Hits == 0)
        {
            continue;
        }
        
        f64 Percent = 100.0 * (f64)Entry->TicksInclusive / (f64)TotalTicks;
        f64 Milliseconds = 1000.0 * (f64)Entry->TicksInclusive / (f64)CPUFrequency;
        string Display = ArenaPrint(Arena, "%-15s %2.2f ms (%2.1f%%)", Entry->Label, Milliseconds, Percent);
        
        Append(&Strings, Display);
        
        *Entry = {};
    }
    
    f64 TotalSeconds = (f64)TotalTicks / (f64)CPUFrequency;
    string FPSString = ArenaPrint(Arena, "FPS: %.0f", 1.0 / TotalSeconds);
    
    Append(&Strings, FPSString);
    
    return Strings;
}