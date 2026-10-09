














#pragma once
#include <stdint.h>

namespace p32prof {


void Begin(const char* name);
void End();
void FrameMark();                        

struct Scope { Scope(const char* n) { Begin(n); } ~Scope() { End(); } };
#define P32PROF_CAT2(a,b) a##b
#define P32PROF_CAT(a,b) P32PROF_CAT2(a,b)
#define P32PROF_SCOPE(name) p32prof::Scope P32PROF_CAT(p32prof_scope_, __LINE__)(name)



struct NodeView {
    const char* name;
    int         depth;
    uint32_t    calls;                   
    float       totalMs, selfMs;         
    float       avgMs, minMs, maxMs;     
    bool        open;                    
};

void            SetStatWindow(int frames);      
int             GetStatWindow();

int             NodeCount();
int             NodeFirstChild(int i);   
int             NodeNextSibling(int i);  
const NodeView& NodeGet(int i);
void            NodeSetOpen(int i, bool open);
void            SetPaused(bool p);
bool            IsPaused();


int   FrameHistory(const float** outMs); 
float Fps();
float FrameAvgMs();
float FrameMinMs();
float FrameMaxMs();


struct ProcStats {
    float    cpuTotalPct, cpuCorePct;    
    int      cores;
    uint32_t threads, handles;
    double   wsMB, wsPeakMB, privMB;     
    float    pageFaultsPerSec;
    float    gpu3D, gpuCopy, gpuCompute, gpuVideo, gpuOther;  
    bool     gpuValid;
    double   vramMB, vramBudgetMB, sharedMB, sharedBudgetMB;
    bool     vramValid;
};
void             SampleProcess();        

struct MemDrift {
    float ramMBperMin, vramMBperMin, spanSec;
    bool  valid;
    
    float stepRamMB, stepVramMB, stepAgoSec;
    bool  hasStep;
};
MemDrift         GetMemDrift();
const ProcStats& GetProcStats();


struct GpuZone { const char* name; float ms; int depth; };
void        SetGpuZones(const GpuZone* zones, int count);
int         GetGpuZones(const GpuZone** out);
struct ZoneStats { float avgMs, minMs, maxMs; };
bool        GetGpuZoneWindow(const char* name, ZoneStats* out);  
void        SetVideoMemory(double vramMB, double vramBudgetMB,
                           double sharedMB, double sharedBudgetMB);
void        SetAdapterName(const char* utf8);
const char* GetAdapterName();
const char* OtherEngineName();   


void     TrackMem(const char* name, uint64_t bytes);   
int      MemCount();
void     MemGet(int i, const char** name, uint64_t* bytes);
uint64_t MemTotal();


int  Dump(char* buf, int cap, const char* extraInfo);  
bool CopyToClipboard(const char* extraInfo);           
void SetCsvLog(bool enabled);                          
bool GetCsvLog();

} 


namespace prim32 { struct Context; }
namespace p32prof {
void ShowProfiler(prim32::Context* ctx, bool* open, const char* extraInfo);
}
