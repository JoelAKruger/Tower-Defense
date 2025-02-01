struct font_asset;
v2 TextPixelSize(font_asset* Font, string String);


u64 ReadCPUTimer();
f32 CPUTimeToSeconds(u64 Ticks, u64 Frequency);
extern u64 CPUFrequency;
