





















#pragma once
#include <stdint.h>
#include <stddef.h>



#ifdef DrawText
#undef DrawText
#endif

#ifndef PRIM32_API
#define PRIM32_API
#endif

namespace prim32 {


struct Vec2 { float x, y; };
static inline Vec2 V2(float x, float y) { return Vec2{ x, y }; }


typedef uint32_t Col;
static inline Col COL32(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}
static inline Col ColA(Col c, uint32_t a) { return (c & 0x00FFFFFFu) | (a << 24); }



enum PrimType : uint32_t {
    PRIM_RECT          = 0,  
    PRIM_GLYPH         = 1,  
    PRIM_RECT_ROUNDED  = 2,  
    PRIM_RECT_STROKE   = 3,  
    PRIM_CIRCLE        = 4,  
    PRIM_CIRCLE_STROKE = 5,  
    PRIM_LINE          = 6,  
    PRIM_IMAGE         = 7,  
    PRIM_SHADOW        = 8,  
};


struct Prim {
    float    x0, y0, x1, y1;   
    uint32_t uv0, uv1;         
                               
    Col      color;
    uint32_t meta;
};
static_assert(sizeof(Prim) == 32, "Prim must be exactly 32 bytes");

struct ClipRect { float x0, y0, x1, y1; };
struct Rect { float x, y, w, h; };      




struct ImageHandle { uint32_t index, gen; };
struct FontHandle  { uint32_t index, gen; };
constexpr ImageHandle InvalidImageHandle = { 0, 0 };
constexpr FontHandle  InvalidFontHandle  = { 0, 0 };
inline bool operator==(ImageHandle a, ImageHandle b) { return a.index == b.index && a.gen == b.gen; }
inline bool operator!=(ImageHandle a, ImageHandle b) { return !(a == b); }
inline bool operator==(FontHandle a, FontHandle b)   { return a.index == b.index && a.gen == b.gen; }
inline bool operator!=(FontHandle a, FontHandle b)   { return !(a == b); }


enum TextAlign  : uint8_t { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum TextVAlign : uint8_t { VALIGN_TOP, VALIGN_MIDDLE, VALIGN_BOTTOM };
struct TextOptions {
    Col        color       = 0;            
    TextAlign  align       = ALIGN_LEFT;
    TextVAlign valign      = VALIGN_TOP;
    float      wrapWidth   = 0;            
    bool       clip        = false;        
    float      lineSpacing = 1.0f;         
    
};

static inline uint32_t PackMeta(uint32_t type, uint32_t clip, uint32_t tex) {
    return type | (clip << 4) | (tex << 16);
}

static inline uint32_t F16(float f) {
    union { float f; uint32_t u; } v; v.f = f * 1.0f;
    uint32_t u = v.u, s = (u >> 16) & 0x8000u;
    int32_t  e = (int32_t)((u >> 23) & 0xFF) - 127 + 15;
    if (e <= 0) return s;
    if (e >= 31) return s | 0x7BFFu;
    return s | (uint32_t)(e << 10) | ((u >> 13) & 0x3FFu);
}
static inline uint32_t PackF16x2(float a, float b) { return F16(a) | (F16(b) << 16); }
static inline uint32_t PackUV(float u, float v) {   
    return (uint32_t)(u * 65535.0f + 0.5f) | ((uint32_t)(v * 65535.0f + 0.5f) << 16);
}


struct Glyph {
    uint32_t uv0, uv1;       
    float    x0, y0, x1, y1; 
    float    advance;
    uint32_t texSlot;        
};







struct FontAtlas {
    
    Glyph*    glyphs;
    uint32_t  glyphCount, glyphCap;
    uint32_t  asciiMap[95];          
    uint32_t* cpKeys;                
    uint32_t* cpVals;
    uint32_t  cpCap, cpCount;
    
    uint8_t*  pagePixels;
    int       pageW, pageH;
    int       penX, penY, shelfH;
    uint32_t  pageSlot;              
    int       pageCount;
    uint32_t  pageSlots[8];          
    
    float ascent, descent, lineHeight, size;
    
    uint32_t kernCount;
    uint32_t* kernKeys;              
    float*    kernVals;
    
    void* rasterA; void* rasterB; void* rasterC;
    int   rasterKind;                
    int   rasterScale;               
};



static inline int EncodeUtf8(uint32_t cp, char out[5]) {
    int n = 0;
    if (cp < 0x80)        { out[n++] = (char)cp; }
    else if (cp < 0x800)  { out[n++] = (char)(0xC0 | (cp >> 6));  out[n++] = (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000){ out[n++] = (char)(0xE0 | (cp >> 12)); out[n++] = (char)(0x80 | ((cp >> 6) & 63)); out[n++] = (char)(0x80 | (cp & 63)); }
    else                  { out[n++] = (char)(0xF0 | (cp >> 18)); out[n++] = (char)(0x80 | ((cp >> 12) & 63)); out[n++] = (char)(0x80 | ((cp >> 6) & 63)); out[n++] = (char)(0x80 | (cp & 63)); }
    out[n] = 0;
    return n;
}


struct IO {
    Vec2  displaySize;
    Vec2  mousePos;
    bool  mouseDown[3];
    float mouseWheel;
    float deltaTime;
};


enum ColorId : int {
    ZC_WindowBg, ZC_TitleBar, ZC_TitleBarActive, ZC_Border,
    ZC_FrameBg, ZC_FrameBgHovered, ZC_FrameBgActive,
    ZC_Button, ZC_ButtonHovered, ZC_ButtonActive,
    ZC_SliderGrab, ZC_SliderGrabActive, ZC_CheckMark,
    ZC_Text, ZC_TextDim, ZC_ProgressFill, ZC_COUNT
};
struct Style {
    Col   colors[ZC_COUNT];
    float windowRounding, frameRounding;
    Vec2  windowPadding, framePadding, itemSpacing;
    float titleBarHeight, frameHeight, borderSize;
    PRIM32_API void Dark();   
};


enum WindowFlags : uint32_t {
    WF_None        = 0,
    WF_NoTitleBar  = 1 << 0,
    WF_NoMove      = 1 << 1,
    WF_NoBackground= 1 << 2,
    WF_AutoSize    = 1 << 3,
    WF_Topmost     = 1 << 4,    
    WF_NoBringToFront = 1 << 5,
};

struct Window {
    uint32_t id;
    char     name[24];          
    Vec2     pos, size;
    Vec2     cursor, lineStart;  
    float    lineH, prevLineH;
    Vec2     contentMax;         
    Vec2     autoSizeStore;
    uint32_t flags;
    uint32_t primStart, primEnd; 
    uint32_t clipIdx;
    int      z;                  
    uint32_t lastFrame;
    bool     inUse;
};



enum LayerId : uint8_t { LAYER_BACKGROUND = 0, LAYER_FOREGROUND = 1 };






struct CachedLayer {
    Prim*     prims;          
    uint32_t  primCount, primCap;
    ClipRect  clips[16];      
    uint32_t  clipCount;
    uint32_t  id;             
    uint32_t  version;        
    const char* name;         
    bool      recording;
};


struct DrawRange {
    uint32_t start, count;
    const char* label;
    const CachedLayer* cache;   
    float ox, oy;               
    uint32_t clipBase;          
};
struct DrawData {
    DrawRange ranges[160];
    uint32_t  rangeCount;
    uint32_t  totalPrims;
    uint32_t  clipCount;
    Vec2      displaySize;
};

struct Stats {
    uint32_t prims, clips, ranges;
    float    buildMs;         
    bool     overflow;        
};







struct Context;
struct DrawList {
    Context* ctx;

    
    void FilledRect(const prim32::Rect& r, Col col, float rounding = 0.0f);
    void Rect(const prim32::Rect& r, Col col, float thickness = 1.0f, float rounding = 0.0f);
    void Line(Vec2 a, Vec2 b, Col col, float thickness = 1.0f);
    void Circle(Vec2 center, float radius, Col col, float thickness = 1.0f);
    void FilledCircle(Vec2 center, float radius, Col col);
    void Shadow(const prim32::Rect& r, Col col, float rounding, float softness);
    
    

    
    PRIM32_API void Image(ImageHandle img, const prim32::Rect& r);
    PRIM32_API void Image(ImageHandle img, const prim32::Rect& r, Col tint);
    PRIM32_API void Image(ImageHandle img, const prim32::Rect& r, Vec2 uv0, Vec2 uv1, Col tint = 0xFFFFFFFFu);
    PRIM32_API void Image(ImageHandle img, Vec2 pos);                 
    PRIM32_API void Image(ImageHandle img, Vec2 pos, Vec2 size, Col tint = 0xFFFFFFFFu);

    
    
    PRIM32_API void Text(const char* text, Vec2 pos);
    PRIM32_API void Text(const char* text, Vec2 pos, Col color);
    PRIM32_API void Text(FontHandle font, const char* text, Vec2 pos);
    PRIM32_API void Text(FontHandle font, const char* text, Vec2 pos, Col color);
    PRIM32_API void Text(const char* text, const prim32::Rect& bounds, const TextOptions& opt);
    PRIM32_API void Text(FontHandle font, const char* text, const prim32::Rect& bounds, const TextOptions& opt);

    
    PRIM32_API Vec2 MeasureText(const char* text);
    PRIM32_API Vec2 MeasureText(FontHandle font, const char* text);

    
    void PushClipRect(const prim32::Rect& r, bool intersect = true);
    void PopClipRect();
};



struct Context {
    
    Prim*     prims;      uint32_t primCount, primCap;
    ClipRect* clips;      uint32_t clipCount, clipCap;
    uint32_t  curClip;
    bool      overflow;

    IO        io;
    Style     style;
    FontAtlas font;

    
    uint32_t hotId, activeId, activeWindow, hoveredWindow;
    Vec2     activeGrabOff;       
    bool     mouseClicked[3], mouseReleased[3];
    Vec2     mouseClickPos;

    
    Window   windows[64];
    uint32_t windowCount;
    Window*  curWindow;
    int      zNext;
    uint32_t frameNum;

    
    FontHandle defaultFont;
    FontHandle fontStack[16];
    int        fontStackTop;      

    
    LayerId      curLayer;
    CachedLayer* recLayer;
    Prim*        savePrims;  uint32_t savePrimCount, savePrimCap;
    ClipRect*    saveClips;  uint32_t saveClipCount, saveClipCap, saveCurClip;
    int          saveClipStackTop;

    
    uint32_t idStack[32]; int idStackTop;

    
    uint32_t clipStack[32]; int clipStackTop;

    
    struct { float x0, y0, x1, y1; uint32_t id; bool hovered, active, clicked; } lastItem;
    bool lastItemFromBehavior;   

    
    char      textScratch[4096];
    DrawList  drawList;           
    DrawData  drawData;
    Stats     stats;
    int64_t   frameT0;
    bool      insideFrame;
};


struct FontDesc { const wchar_t* face; float sizePx; bool kerning; };

PRIM32_API Context* CreateContext(const FontDesc* font = nullptr); 
PRIM32_API void     DestroyContext(Context* ctx);
PRIM32_API Context* GetContext();
PRIM32_API void     SetContext(Context* ctx);


struct FrameMem { Prim* prims; uint32_t primCap; ClipRect* clips; uint32_t clipCap; };
PRIM32_API void      NewFrame(const FrameMem& mem, const IO& io);
PRIM32_API DrawData* EndFrame();   
PRIM32_API Stats     GetStats();




static inline Prim* AddPrims(Context* c, uint32_t n) {
    uint32_t i = c->primCount;
    if (i + n > c->primCap) { c->overflow = true; return nullptr; }
    c->primCount = i + n;
    return c->prims + i;
}
static inline void DrawRect(Context* c, float x0, float y0, float x1, float y1, Col col) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    p->uv0 = 0; p->uv1 = 0; p->color = col;
    p->meta = PackMeta(PRIM_RECT, c->curClip, 0);
}
static inline void DrawRectRounded(Context* c, float x0, float y0, float x1, float y1,
                                   Col col, float radius, Col gradBottom = 0) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    p->uv0 = PackF16x2(radius, 0.0f); p->uv1 = gradBottom; p->color = col;
    p->meta = PackMeta(PRIM_RECT_ROUNDED, c->curClip, 0);
}
static inline void DrawRectStroke(Context* c, float x0, float y0, float x1, float y1,
                                  Col col, float radius, float thickness) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    p->uv0 = PackF16x2(radius, thickness); p->uv1 = 0; p->color = col;
    p->meta = PackMeta(PRIM_RECT_STROKE, c->curClip, 0);
}
static inline void DrawCircle(Context* c, float cx, float cy, float r, Col col, Col gradBottom = 0) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = cx - r; p->y0 = cy - r; p->x1 = cx + r; p->y1 = cy + r;
    p->uv0 = 0; p->uv1 = gradBottom; p->color = col;
    p->meta = PackMeta(PRIM_CIRCLE, c->curClip, 0);
}
static inline void DrawCircleStroke(Context* c, float cx, float cy, float r, Col col, float thickness) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = cx - r; p->y0 = cy - r; p->x1 = cx + r; p->y1 = cy + r;
    p->uv0 = PackF16x2(0.0f, thickness); p->uv1 = 0; p->color = col;
    p->meta = PackMeta(PRIM_CIRCLE_STROKE, c->curClip, 0);
}
static inline void DrawLine(Context* c, float x0, float y0, float x1, float y1, Col col, float thickness) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;   
    p->uv0 = PackF16x2(0.0f, thickness); p->uv1 = 0; p->color = col;
    p->meta = PackMeta(PRIM_LINE, c->curClip, 0);
}
static inline void DrawShadow(Context* c, float x0, float y0, float x1, float y1,
                              Col col, float radius, float softness) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    p->uv0 = PackF16x2(radius, softness); p->uv1 = 0; p->color = col;
    p->meta = PackMeta(PRIM_SHADOW, c->curClip, 0);
}
static inline void DrawImage(Context* c, float x0, float y0, float x1, float y1,
                             uint32_t texSlot, Col tint = 0xFFFFFFFFu,
                             float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
    Prim* p = AddPrims(c, 1); if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    p->uv0 = PackUV(u0, v0); p->uv1 = PackUV(u1, v1); p->color = tint;
    p->meta = PackMeta(PRIM_IMAGE, c->curClip, texSlot);
}

PRIM32_API void  DrawText(Context* c, float x, float y, Col col, const char* text, const char* end = nullptr);
PRIM32_API void  DrawText(Context* c, FontHandle font, float x, float y, Col col, const char* text, const char* end = nullptr);
PRIM32_API Vec2  TextSize(Context* c, const char* text, const char* end = nullptr);


inline void DrawList::FilledRect(const prim32::Rect& r, Col col, float rounding) {
    if (rounding > 0) DrawRectRounded(ctx, r.x, r.y, r.x + r.w, r.y + r.h, col, rounding);
    else              DrawRect(ctx, r.x, r.y, r.x + r.w, r.y + r.h, col);
}
inline void DrawList::Rect(const prim32::Rect& r, Col col, float thickness, float rounding) {
    DrawRectStroke(ctx, r.x, r.y, r.x + r.w, r.y + r.h, col, rounding, thickness);
}
inline void DrawList::Line(Vec2 a, Vec2 b, Col col, float thickness) {
    DrawLine(ctx, a.x, a.y, b.x, b.y, col, thickness);
}
inline void DrawList::Circle(Vec2 c, float radius, Col col, float thickness) {
    DrawCircleStroke(ctx, c.x, c.y, radius, col, thickness);
}
inline void DrawList::FilledCircle(Vec2 c, float radius, Col col) {
    DrawCircle(ctx, c.x, c.y, radius, col);
}
inline void DrawList::Shadow(const prim32::Rect& r, Col col, float rounding, float softness) {
    DrawShadow(ctx, r.x, r.y, r.x + r.w, r.y + r.h, col, rounding, softness);
}

PRIM32_API DrawList* GetDrawList();   

PRIM32_API void  PushClip(float x0, float y0, float x1, float y1, bool intersect = true);
PRIM32_API void  PopClip();

inline void DrawList::PushClipRect(const prim32::Rect& r, bool intersect) { PushClip(r.x, r.y, r.x + r.w, r.y + r.h, intersect); }
inline void DrawList::PopClipRect() { PopClip(); }


PRIM32_API void DrawTextOpt(Context* c, FontHandle font, const char* text, const prim32::Rect& bounds, const TextOptions& opt);


PRIM32_API void  BeginLayer(LayerId layer);   
PRIM32_API void  EndLayer();                  


PRIM32_API CachedLayer* CreateCachedLayer(uint32_t maxPrims, const char* name);
PRIM32_API void  DestroyCachedLayer(CachedLayer* layer);
PRIM32_API void  BeginCache(CachedLayer* layer);  
PRIM32_API void  EndCache();                      
PRIM32_API void  DrawCached(CachedLayer* layer, Vec2 offset);  


PRIM32_API bool  Begin(const char* name, uint32_t flags = 0);
PRIM32_API void  End();
PRIM32_API void  SetNextWindowPos(Vec2 pos, bool always = false);
PRIM32_API void  SetNextWindowSize(Vec2 size, bool always = false);

PRIM32_API void  Text(const char* text);
PRIM32_API void  TextColored(Col col, const char* text);
PRIM32_API void  TextF(const char* fmt, ...);
PRIM32_API bool  Button(const char* label, Vec2 size = { 0, 0 });
PRIM32_API bool  Checkbox(const char* label, bool* v);
PRIM32_API bool  SliderFloat(const char* label, float* v, float vMin, float vMax, const char* fmt = "%.2f");
PRIM32_API bool  SliderInt(const char* label, int* v, int vMin, int vMax);
PRIM32_API void  ProgressBar(float frac, Vec2 size = { 0, 0 });
PRIM32_API void  Separator();
PRIM32_API void  SameLine(float spacing = -1.0f);
PRIM32_API void  Spacing();
PRIM32_API bool  InvisibleButton(const char* id, Vec2 size);
PRIM32_API void  Dummy(Vec2 size);

PRIM32_API bool  IsItemHovered();
PRIM32_API bool  IsItemActive();
PRIM32_API Vec2  GetCursorScreenPos();
PRIM32_API void  SetCursorScreenPos(Vec2 p);
PRIM32_API Vec2  GetContentAvail();
PRIM32_API float GetFrameHeight();

PRIM32_API void  PushID(const char* id);
PRIM32_API void  PushID(int id);
PRIM32_API void  PopID();






PRIM32_API ImageHandle LoadImageFromFile(const char* path);                     
PRIM32_API ImageHandle LoadImageFromMemory(const void* data, size_t size);      
PRIM32_API ImageHandle LoadImageFromRgba(const void* pixels, int width, int height);
PRIM32_API void        DestroyImage(ImageHandle image);
PRIM32_API bool        IsValid(ImageHandle image);
PRIM32_API Vec2        GetImageSize(ImageHandle image);

PRIM32_API FontHandle  LoadFontFromFile(const char* path, float sizePixels);    
PRIM32_API FontHandle  LoadFontFromMemory(const void* data, size_t size, float sizePixels, int weight = 400);
PRIM32_API void        DestroyFont(FontHandle font);
PRIM32_API bool        IsValid(FontHandle font);
PRIM32_API int         GetFontRasterizer(FontHandle font); 




PRIM32_API void        SetDefaultFont(FontHandle font);
PRIM32_API FontHandle  GetDefaultFont();
PRIM32_API void        PushFont(FontHandle font);   
PRIM32_API void        PopFont();                   

PRIM32_API Vec2        MeasureText(const char* text, const char* end = nullptr);            
PRIM32_API Vec2        MeasureText(FontHandle font, const char* text, const char* end = nullptr);
PRIM32_API float       GetFontLineHeight(FontHandle font = InvalidFontHandle);

PRIM32_API const char* GetLastResourceError();      
struct ResourceStats { uint32_t images, fonts; uint64_t textureBytes, atlasBytes; };
PRIM32_API ResourceStats GetResourceStats();

} 
