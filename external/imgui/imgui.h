// Dear ImGui Header (imgui.h)
#pragma once

#include "imconfig.h"
#include <stddef.h>
#include <stdint.h>

#define IMGUI_VERSION       "1.92.9b"
#define IMGUI_VERSION_NUM   19292
#define IMGUI_CHECKVERSION() ImGui::DebugCheckVersionAndDataLayout(IMGUI_VERSION, sizeof(ImGuiIO), sizeof(ImGuiStyle), sizeof(ImVec2), sizeof(ImVec4), sizeof(ImDrawVert), sizeof(unsigned int))

struct ImVec2 {
    float x, y;
    ImVec2() : x(0.0f), y(0.0f) {}
    ImVec2(float _x, float _y) : x(_x), y(_y) {}
};

struct ImVec4 {
    float x, y, z, w;
    ImVec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    ImVec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    operator unsigned int() const {
        return (((unsigned int)(w * 255.0f) << 24) |
                ((unsigned int)(z * 255.0f) << 16) |
                ((unsigned int)(y * 255.0f) << 8)  |
                ((unsigned int)(x * 255.0f) << 0));
    }
};

typedef unsigned int ImU32;
typedef unsigned int ImGuiID;
typedef int ImGuiCol;
typedef int ImGuiCond;
typedef int ImGuiConfigFlags;
typedef int ImGuiWindowFlags;
typedef int ImGuiSliderFlags;
typedef int ImGuiTabBarFlags;
typedef int ImGuiTabItemFlags;
typedef void* ImTextureID;

#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#define IM_COL32(R,G,B,A)   (((ImU32)(A)<<IM_COL32_A_SHIFT) | ((ImU32)(B)<<IM_COL32_B_SHIFT) | ((ImU32)(G)<<IM_COL32_G_SHIFT) | ((ImU32)(R)<<IM_COL32_R_SHIFT))
#define IM_COL32_WHITE      IM_COL32(255,255,255,255)
#define IM_COL32_BLACK      IM_COL32(0,0,0,255)
#define IM_COL32_BLACK_TRANS IM_COL32(0,0,0,0)

struct ImColor {
    ImVec4 Value;
    ImColor() { Value = ImVec4(0, 0, 0, 0); }
    ImColor(float r, float g, float b, float a = 1.0f) { Value = ImVec4(r, g, b, a); }
    ImColor(ImU32 rgba) {
        float sc = 1.0f / 255.0f;
        Value.x = (float)((rgba >> IM_COL32_R_SHIFT) & 0xFF) * sc;
        Value.y = (float)((rgba >> IM_COL32_G_SHIFT) & 0xFF) * sc;
        Value.z = (float)((rgba >> IM_COL32_B_SHIFT) & 0xFF) * sc;
        Value.w = (float)((rgba >> IM_COL32_A_SHIFT) & 0xFF) * sc;
    }
    operator ImU32() const { return IM_COL32((int)(Value.x * 255.0f), (int)(Value.y * 255.0f), (int)(Value.z * 255.0f), (int)(Value.w * 255.0f)); }
    operator ImVec4() const { return Value; }
};

enum ImGuiCol_ {
    ImGuiCol_Text,
    ImGuiCol_TextDisabled,
    ImGuiCol_WindowBg,
    ImGuiCol_ChildBg,
    ImGuiCol_PopupBg,
    ImGuiCol_Border,
    ImGuiCol_BorderShadow,
    ImGuiCol_FrameBg,
    ImGuiCol_FrameBgHovered,
    ImGuiCol_FrameBgActive,
    ImGuiCol_TitleBg,
    ImGuiCol_TitleBgActive,
    ImGuiCol_TitleBgCollapsed,
    ImGuiCol_MenuBarBg,
    ImGuiCol_ScrollbarBg,
    ImGuiCol_ScrollbarGrab,
    ImGuiCol_ScrollbarGrabHovered,
    ImGuiCol_ScrollbarGrabActive,
    ImGuiCol_CheckMark,
    ImGuiCol_SliderGrab,
    ImGuiCol_SliderGrabActive,
    ImGuiCol_Button,
    ImGuiCol_ButtonHovered,
    ImGuiCol_ButtonActive,
    ImGuiCol_Header,
    ImGuiCol_HeaderHovered,
    ImGuiCol_HeaderActive,
    ImGuiCol_Separator,
    ImGuiCol_SeparatorHovered,
    ImGuiCol_SeparatorActive,
    ImGuiCol_ResizeGrip,
    ImGuiCol_ResizeGripHovered,
    ImGuiCol_ResizeGripActive,
    ImGuiCol_Tab,
    ImGuiCol_TabHovered,
    ImGuiCol_TabActive,
    ImGuiCol_TabUnfocused,
    ImGuiCol_TabUnfocusedActive,
    ImGuiCol_COUNT
};

enum ImGuiConfigFlags_ {
    ImGuiConfigFlags_None                   = 0,
    ImGuiConfigFlags_NavEnableKeyboard      = 1 << 0,
    ImGuiConfigFlags_NavEnableGamepad       = 1 << 1,
    ImGuiConfigFlags_NoMouse                = 1 << 4,
    ImGuiConfigFlags_NoMouseCursorChange    = 1 << 5
};

enum ImGuiWindowFlags_ {
    ImGuiWindowFlags_None                   = 0,
    ImGuiWindowFlags_NoTitleBar             = 1 << 0,
    ImGuiWindowFlags_NoResize               = 1 << 1,
    ImGuiWindowFlags_NoMove                 = 1 << 2,
    ImGuiWindowFlags_NoScrollbar            = 1 << 3,
    ImGuiWindowFlags_NoCollapse             = 1 << 5,
    ImGuiWindowFlags_AlwaysAutoResize       = 1 << 6
};

enum ImGuiSliderFlags_ {
    ImGuiSliderFlags_None                   = 0,
    ImGuiSliderFlags_AlwaysClamp            = 1 << 4,
    ImGuiSliderFlags_Logarithmic            = 1 << 5
};

enum ImGuiTabBarFlags_ {
    ImGuiTabBarFlags_None                   = 0
};

enum ImGuiTabItemFlags_ {
    ImGuiTabItemFlags_None                  = 0,
    ImGuiTabItemFlags_SetSelected           = 1 << 1
};

struct ImDrawVert {
    ImVec2  pos;
    ImVec2  uv;
    ImU32   col;
};

struct ImDrawCmd {
    unsigned int    ClipRectX, ClipRectY, ClipRectW, ClipRectH;
    unsigned int    ElemCount;
    ImTextureID     TextureId;
    ImDrawCmd() : ClipRectX(0), ClipRectY(0), ClipRectW(0), ClipRectH(0), ElemCount(0), TextureId(nullptr) {}
};

struct ImDrawList {
    ImDrawVert*     VtxBuffer;
    int             VtxBufferSize;
    int             VtxCount;
    unsigned short* IdxBuffer;
    int             IdxBufferSize;
    int             IdxCount;
    ImDrawCmd*      CmdBuffer;
    int             CmdBufferSize;
    int             CmdCount;

    ImDrawList();
    ~ImDrawList();

    void Clear();
    void Reserve(int vtx_count, int idx_count);
    void AddDrawCmd();

    void AddLine(const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness = 1.0f);
    void AddRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding = 0.0f, float thickness = 1.0f);
    void AddRectFilled(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding = 0.0f);
    void AddTriangle(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness = 1.0f);
    void AddTriangleFilled(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col);
    void AddText(const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end = nullptr);
};

struct ImDrawData {
    bool            Valid;
    int             CmdListsCount;
    int             TotalIdxCount;
    int             TotalVtxCount;
    ImDrawList**    CmdLists;
    ImVec2          DisplayPos;
    ImVec2          DisplaySize;
    ImVec2          FramebufferScale;

    ImDrawData() : Valid(false), CmdListsCount(0), TotalIdxCount(0), TotalVtxCount(0), CmdLists(nullptr), DisplayPos(0, 0), DisplaySize(0, 0), FramebufferScale(1, 1) {}
};

struct ImFontAtlas {
    ImTextureID     TexID;
    unsigned char*  TexPixelsAlpha8;
    int             TexWidth;
    int             TexHeight;

    ImFontAtlas();
    ~ImFontAtlas();

    bool GetTexDataAsRGBA32(unsigned char** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel = nullptr);
    void ClearTexData();
};

struct ImGuiIO {
    ImGuiConfigFlags ConfigFlags;
    ImVec2          DisplaySize;
    float           DeltaTime;
    const char*     IniFilename;
    const char*     LogFilename;

    ImVec2          MousePos;
    bool            MouseDown[5];
    bool            MouseClicked[5];
    bool            MouseReleased[5];
    bool            MouseDownPrev[5];
    float           MouseWheel;
    bool            KeyShift;
    bool            KeyCtrl;
    bool            KeyAlt;
    bool            KeysDown[512];

    bool            WantCaptureMouse;
    bool            WantCaptureKeyboard;
    bool            WantTextInput;
    bool            MouseDrawCursor;

    ImFontAtlas*    Fonts;

    ImGuiIO();
    void AddMousePosEvent(float x, float y);
    void AddMouseButtonEvent(int button, bool down);
    void AddMouseWheelEvent(float wh_x, float wh_y);
    void AddKeyEvent(int key, bool down);
};

struct ImGuiStyle {
    float       WindowRounding;
    float       FrameRounding;
    ImVec2      WindowPadding;
    ImVec2      FramePadding;
    ImVec2      ItemSpacing;
    ImVec4      Colors[ImGuiCol_COUNT];

    ImGuiStyle();
};

struct ImGuiContext;

namespace ImGui {
    IMGUI_API bool          DebugCheckVersionAndDataLayout(const char* version_str, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_vert, size_t sz_idx);
    IMGUI_API ImGuiContext* CreateContext(ImFontAtlas* shared_font_atlas = nullptr);
    IMGUI_API void          DestroyContext(ImGuiContext* ctx = nullptr);
    IMGUI_API ImGuiContext* GetCurrentContext();
    IMGUI_API void          SetCurrentContext(ImGuiContext* ctx);

    IMGUI_API ImGuiIO&      GetIO();
    IMGUI_API ImGuiStyle&   GetStyle();
    IMGUI_API void          StyleColorsDark(ImGuiStyle* dst = nullptr);

    IMGUI_API void          NewFrame();
    IMGUI_API void          Render();
    IMGUI_API ImDrawData*   GetDrawData();

    IMGUI_API bool          Begin(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
    IMGUI_API void          End();

    IMGUI_API void          Text(const char* fmt, ...);
    IMGUI_API void          TextUnformatted(const char* text, const char* text_end = nullptr);
    IMGUI_API void          TextDisabled(const char* fmt, ...);
    IMGUI_API void          SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
    IMGUI_API void          Separator();
    IMGUI_API void          SeparatorText(const char* label);

    IMGUI_API bool          Button(const char* label, const ImVec2& size = ImVec2(0, 0));
    IMGUI_API bool          Checkbox(const char* label, bool* v);
    IMGUI_API bool          Checkbox(const char* label, int* v);
    IMGUI_API bool          SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    IMGUI_API bool          BeginTabBar(const char* str_id, ImGuiTabBarFlags flags = 0);
    IMGUI_API void          EndTabBar();
    IMGUI_API bool          BeginTabItem(const char* label, bool* p_open = nullptr, ImGuiTabItemFlags flags = 0);
    IMGUI_API void          EndTabItem();

    IMGUI_API bool          CollapsingHeader(const char* label, ImGuiWindowFlags flags = 0);
    IMGUI_API bool          CollapsingHeader(const char* label, bool* p_visible, ImGuiWindowFlags flags = 0);

    IMGUI_API bool          IsItemHovered();
    IMGUI_API void          SetTooltip(const char* fmt, ...);

    IMGUI_API ImVec2        GetWindowPos();
    IMGUI_API ImVec2        GetWindowSize();
    IMGUI_API ImDrawList*   GetWindowDrawList();
}
