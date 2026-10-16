// Dear ImGui Core Implementation (imgui.cpp)
#include "imgui.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct ImGuiWindow {
    char        Name[64];
    ImVec2      Pos;
    ImVec2      Size;
    bool        Collapsed;
    bool*       Open;
    bool        Dragging;
    bool        Resizing;
    ImVec2      DragOffset;

    ImVec2      CursorPos;
    float       LineMaxY;
    bool        SameLinePending;

    ImVec2      TabBarPos;
    float       TabBarCurX;
    float       TabBarHeight;
    float       TabContentY;
    char        ActiveTab[32];

    ImGuiWindow() {
        memset(Name, 0, sizeof(Name));
        Pos = ImVec2(50, 50);
        Size = ImVec2(620, 520);
        Collapsed = false;
        Open = nullptr;
        Dragging = false;
        Resizing = false;
        DragOffset = ImVec2(0, 0);
        CursorPos = ImVec2(0, 0);
        LineMaxY = 0;
        SameLinePending = false;
        TabBarPos = ImVec2(0, 0);
        TabBarCurX = 0;
        TabBarHeight = 22.0f;
        TabContentY = 0;
        memset(ActiveTab, 0, sizeof(ActiveTab));
        strncpy(ActiveTab, "Camera", sizeof(ActiveTab) - 1);
    }
};

struct ImGuiContext {
    ImGuiIO         IO;
    ImGuiStyle      Style;
    ImDrawList      DrawList;
    ImDrawData      DrawData;
    ImDrawList*     DrawListArray[1];

    ImGuiWindow     Window;
    bool            WithinFrame;

    bool            LastItemHovered;
    char            TooltipText[256];
    bool            HasTooltip;

    // Active widget interaction tracking
    int             ActiveSliderId;
    int             NextItemId;

    ImGuiContext() {
        WithinFrame = false;
        LastItemHovered = false;
        HasTooltip = false;
        ActiveSliderId = -1;
        NextItemId = 0;
        DrawListArray[0] = &DrawList;
        DrawData.CmdLists = DrawListArray;
        DrawData.CmdListsCount = 1;
    }
};

static ImGuiContext* GImGui = nullptr;

// -----------------------------------------------------------------------------
// ImGuiIO & ImGuiStyle
// -----------------------------------------------------------------------------
ImGuiIO::ImGuiIO() {
    ConfigFlags = 0;
    DisplaySize = ImVec2(1280, 720);
    DeltaTime = 1.0f / 60.0f;
    IniFilename = nullptr;
    LogFilename = nullptr;
    MousePos = ImVec2(-1, -1);
    memset(MouseDown, 0, sizeof(MouseDown));
    memset(MouseClicked, 0, sizeof(MouseClicked));
    memset(MouseReleased, 0, sizeof(MouseReleased));
    memset(MouseDownPrev, 0, sizeof(MouseDownPrev));
    MouseWheel = 0.0f;
    KeyShift = false;
    KeyCtrl = false;
    KeyAlt = false;
    memset(KeysDown, 0, sizeof(KeysDown));
    WantCaptureMouse = false;
    WantCaptureKeyboard = false;
    WantTextInput = false;
    MouseDrawCursor = false;
    Fonts = new ImFontAtlas();
}

void ImGuiIO::AddMousePosEvent(float x, float y) {
    MousePos = ImVec2(x, y);
}

void ImGuiIO::AddMouseButtonEvent(int button, bool down) {
    if (button >= 0 && button < 5) {
        MouseDown[button] = down;
    }
}

void ImGuiIO::AddMouseWheelEvent(float wh_x, float wh_y) {
    (void)wh_x;
    MouseWheel += wh_y;
}

void ImGuiIO::AddKeyEvent(int key, bool down) {
    if (key >= 0 && key < 512) {
        KeysDown[key] = down;
    }
}

ImGuiStyle::ImGuiStyle() {
    WindowRounding = 0.0f;
    FrameRounding = 0.0f;
    WindowPadding = ImVec2(8.0f, 8.0f);
    FramePadding = ImVec2(4.0f, 3.0f);
    ItemSpacing = ImVec2(8.0f, 4.0f);
    ImGui::StyleColorsDark(this);
}

// -----------------------------------------------------------------------------
// Context & Lifecycle
// -----------------------------------------------------------------------------
bool ImGui::DebugCheckVersionAndDataLayout(const char* version_str, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_vert, size_t sz_idx) {
    (void)version_str; (void)sz_io; (void)sz_style; (void)sz_vec2; (void)sz_vec4; (void)sz_vert; (void)sz_idx;
    return true;
}

ImGuiContext* ImGui::CreateContext(ImFontAtlas* shared_font_atlas) {
    ImGuiContext* ctx = new ImGuiContext();
    if (shared_font_atlas) {
        if (ctx->IO.Fonts) delete ctx->IO.Fonts;
        ctx->IO.Fonts = shared_font_atlas;
    }
    if (GImGui == nullptr) GImGui = ctx;
    StyleColorsDark(&ctx->Style);
    return ctx;
}

void ImGui::DestroyContext(ImGuiContext* ctx) {
    if (ctx == nullptr) ctx = GImGui;
    if (GImGui == ctx) GImGui = nullptr;
    if (ctx) {
        if (ctx->IO.Fonts) delete ctx->IO.Fonts;
        delete ctx;
    }
}

ImGuiContext* ImGui::GetCurrentContext() { return GImGui; }
void ImGui::SetCurrentContext(ImGuiContext* ctx) { GImGui = ctx; }

ImGuiIO& ImGui::GetIO() { return GImGui->IO; }
ImGuiStyle& ImGui::GetStyle() { return GImGui->Style; }

void ImGui::StyleColorsDark(ImGuiStyle* dst) {
    ImGuiStyle* style = dst ? dst : &GImGui->Style;
    ImVec4* colors = style->Colors;

    colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
}

// -----------------------------------------------------------------------------
// Frame Management
// -----------------------------------------------------------------------------
void ImGui::NewFrame() {
    if (!GImGui) return;
    GImGui->WithinFrame = true;
    GImGui->DrawList.Clear();
    GImGui->HasTooltip = false;
    GImGui->NextItemId = 0;

    for (int i = 0; i < 5; i++) {
        GImGui->IO.MouseClicked[i] = GImGui->IO.MouseDown[i] && !GImGui->IO.MouseDownPrev[i];
        GImGui->IO.MouseReleased[i] = !GImGui->IO.MouseDown[i] && GImGui->IO.MouseDownPrev[i];
    }

    // Default WantCapture states
    GImGui->IO.WantCaptureMouse = false;
    GImGui->IO.WantCaptureKeyboard = false;
}

void ImGui::Render() {
    if (!GImGui) return;
    GImGui->WithinFrame = false;

    // Render tooltip if active
    if (GImGui->HasTooltip && GImGui->TooltipText[0]) {
        float tx = GImGui->IO.MousePos.x + 12.0f;
        float ty = GImGui->IO.MousePos.y + 12.0f;
        int text_len = (int)strlen(GImGui->TooltipText);
        float tw = (float)text_len * 7.0f + 16.0f;
        float th = 22.0f;

        ImU32 pop_bg = GImGui->Style.Colors[ImGuiCol_PopupBg];
        ImU32 pop_border = GImGui->Style.Colors[ImGuiCol_Border];
        ImU32 pop_text = GImGui->Style.Colors[ImGuiCol_Text];

        GImGui->DrawList.AddRectFilled(ImVec2(tx, ty), ImVec2(tx + tw, ty + th), pop_bg);
        GImGui->DrawList.AddRect(ImVec2(tx, ty), ImVec2(tx + tw, ty + th), pop_border);
        GImGui->DrawList.AddText(ImVec2(tx + 8.0f, ty + 4.0f), pop_text, GImGui->TooltipText);
    }

    // Render cursor if requested
    if (GImGui->IO.MouseDrawCursor) {
        float cx = GImGui->IO.MousePos.x;
        float cy = GImGui->IO.MousePos.y;
        ImU32 white = IM_COL32(255, 255, 255, 255);
        ImU32 black = IM_COL32(0, 0, 0, 255);

        GImGui->DrawList.AddTriangleFilled(ImVec2(cx, cy), ImVec2(cx, cy + 15.0f), ImVec2(cx + 10.0f, cy + 10.0f), black);
        GImGui->DrawList.AddTriangleFilled(ImVec2(cx + 1.0f, cy + 1.0f), ImVec2(cx + 1.0f, cy + 13.0f), ImVec2(cx + 8.5f, cy + 9.0f), white);
    }

    GImGui->DrawData.Valid = true;
    GImGui->DrawData.CmdListsCount = 1;
    GImGui->DrawData.TotalVtxCount = GImGui->DrawList.VtxCount;
    GImGui->DrawData.TotalIdxCount = GImGui->DrawList.IdxCount;
    GImGui->DrawData.DisplayPos = ImVec2(0, 0);
    GImGui->DrawData.DisplaySize = GImGui->IO.DisplaySize;

    for (int i = 0; i < 5; i++) {
        GImGui->IO.MouseDownPrev[i] = GImGui->IO.MouseDown[i];
    }
}

ImDrawData* ImGui::GetDrawData() {
    if (!GImGui || !GImGui->DrawData.Valid) return nullptr;
    return &GImGui->DrawData;
}

// -----------------------------------------------------------------------------
// Window (Begin / End)
// -----------------------------------------------------------------------------
bool ImGui::Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) {
    (void)flags;
    if (!GImGui) return false;

    ImGuiWindow& win = GImGui->Window;
    if (name) strncpy(win.Name, name, sizeof(win.Name) - 1);
    win.Open = p_open;

    float mx = GImGui->IO.MousePos.x;
    float my = GImGui->IO.MousePos.y;
    bool mouseDown = GImGui->IO.MouseDown[0];

    // Check dragging window title bar
    float titleH = 24.0f;
    if (mouseDown) {
        if (!win.Dragging && !win.Resizing) {
            if (mx >= win.Pos.x && mx <= win.Pos.x + win.Size.x &&
                my >= win.Pos.y && my <= win.Pos.y + titleH) {
                // If not clicking collapse or close button
                if (mx > win.Pos.x + 24.0f && mx < win.Pos.x + win.Size.x - 24.0f) {
                    win.Dragging = true;
                    win.DragOffset = ImVec2(mx - win.Pos.x, my - win.Pos.y);
                }
            } else if (!win.Collapsed &&
                       mx >= win.Pos.x + win.Size.x - 18.0f && mx <= win.Pos.x + win.Size.x &&
                       my >= win.Pos.y + win.Size.y - 18.0f && my <= win.Pos.y + win.Size.y) {
                win.Resizing = true;
                win.DragOffset = ImVec2(mx - (win.Pos.x + win.Size.x), my - (win.Pos.y + win.Size.y));
            }
        }
    } else {
        win.Dragging = false;
        win.Resizing = false;
    }

    if (win.Dragging) {
        win.Pos.x = mx - win.DragOffset.x;
        win.Pos.y = my - win.DragOffset.y;
        if (win.Pos.y < 0.0f) win.Pos.y = 0.0f;
        if (win.Pos.x < -win.Size.x + 60.0f) win.Pos.x = -win.Size.x + 60.0f;
        if (win.Pos.x > GImGui->IO.DisplaySize.x - 60.0f) win.Pos.x = GImGui->IO.DisplaySize.x - 60.0f;
        if (win.Pos.y > GImGui->IO.DisplaySize.y - 30.0f) win.Pos.y = GImGui->IO.DisplaySize.y - 30.0f;
    } else if (win.Resizing) {
        float newW = (mx - win.DragOffset.x) - win.Pos.x;
        float newH = (my - win.DragOffset.y) - win.Pos.y;
        if (newW < 540.0f) newW = 540.0f;
        if (newH < 440.0f) newH = 440.0f;
        win.Size.x = newW;
        win.Size.y = newH;
    }

    // Set WantCaptureMouse if mouse is over window
    float totalH = win.Collapsed ? titleH : win.Size.y;
    if (mx >= win.Pos.x && mx <= win.Pos.x + win.Size.x &&
        my >= win.Pos.y && my <= win.Pos.y + totalH) {
        GImGui->IO.WantCaptureMouse = true;
        GImGui->IO.WantCaptureKeyboard = true;
    }

    // Render Window Background & Border
    ImU32 win_bg = GImGui->Style.Colors[ImGuiCol_WindowBg];
    ImU32 border = GImGui->Style.Colors[ImGuiCol_Border];
    ImU32 title_bg = GImGui->Style.Colors[ImGuiCol_TitleBgActive];
    ImU32 text_col = GImGui->Style.Colors[ImGuiCol_Text];
    ImU32 btn_hover = GImGui->Style.Colors[ImGuiCol_ButtonHovered];

    if (!win.Collapsed) {
        GImGui->DrawList.AddRectFilled(win.Pos, ImVec2(win.Pos.x + win.Size.x, win.Pos.y + win.Size.y), win_bg);
        GImGui->DrawList.AddRect(win.Pos, ImVec2(win.Pos.x + win.Size.x, win.Pos.y + win.Size.y), border);
    }

    // Title Bar
    GImGui->DrawList.AddRectFilled(win.Pos, ImVec2(win.Pos.x + win.Size.x, win.Pos.y + titleH), title_bg);
    GImGui->DrawList.AddLine(ImVec2(win.Pos.x, win.Pos.y + titleH), ImVec2(win.Pos.x + win.Size.x, win.Pos.y + titleH), border);

    // Collapse Button (Triangle)
    float colBtnX = win.Pos.x + 4.0f;
    float colBtnY = win.Pos.y + 3.0f;
    bool colHover = (mx >= colBtnX && mx <= colBtnX + 18.0f && my >= colBtnY && my <= colBtnY + 18.0f);
    if (colHover) {
        GImGui->DrawList.AddRectFilled(ImVec2(colBtnX, colBtnY), ImVec2(colBtnX + 18.0f, colBtnY + 18.0f), btn_hover);
        if (GImGui->IO.MouseClicked[0] && !win.Dragging) {
            win.Collapsed = !win.Collapsed;
        }
    }
    if (win.Collapsed) {
        GImGui->DrawList.AddTriangleFilled(ImVec2(win.Pos.x + 9.0f, win.Pos.y + 7.0f), ImVec2(win.Pos.x + 9.0f, win.Pos.y + 17.0f), ImVec2(win.Pos.x + 17.0f, win.Pos.y + 12.0f), text_col);
    } else {
        GImGui->DrawList.AddTriangleFilled(ImVec2(win.Pos.x + 8.0f, win.Pos.y + 9.0f), ImVec2(win.Pos.x + 18.0f, win.Pos.y + 9.0f), ImVec2(win.Pos.x + 13.0f, win.Pos.y + 16.0f), text_col);
    }

    // Title Text
    GImGui->DrawList.AddText(ImVec2(win.Pos.x + 26.0f, win.Pos.y + 6.0f), text_col, win.Name);

    // Close Button [X]
    if (p_open != nullptr) {
        float closeX = win.Pos.x + win.Size.x - 22.0f;
        float closeY = win.Pos.y + 3.0f;
        bool closeHover = (mx >= closeX && mx <= closeX + 18.0f && my >= closeY && my <= closeY + 18.0f);
        if (closeHover) {
            GImGui->DrawList.AddRectFilled(ImVec2(closeX, closeY), ImVec2(closeX + 18.0f, closeY + 18.0f), btn_hover);
            if (GImGui->IO.MouseClicked[0] && !win.Dragging) {
                *p_open = false;
            }
        }
        GImGui->DrawList.AddLine(ImVec2(closeX + 4.5f, closeY + 4.5f), ImVec2(closeX + 13.5f, closeY + 13.5f), text_col, 1.8f);
        GImGui->DrawList.AddLine(ImVec2(closeX + 13.5f, closeY + 4.5f), ImVec2(closeX + 4.5f, closeY + 13.5f), text_col, 1.8f);
    }

    // Resize Grip (bottom-right)
    if (!win.Collapsed) {
        float rx = win.Pos.x + win.Size.x;
        float ry = win.Pos.y + win.Size.y;
        ImU32 grip_col = GImGui->Style.Colors[ImGuiCol_ResizeGrip];
        GImGui->DrawList.AddTriangleFilled(ImVec2(rx - 14.0f, ry), ImVec2(rx, ry - 14.0f), ImVec2(rx, ry), grip_col);
    }

    // Initialize layout cursor
    win.CursorPos = ImVec2(win.Pos.x + 10.0f, win.Pos.y + titleH + 10.0f);
    win.LineMaxY = win.CursorPos.y;
    win.SameLinePending = false;

    return !win.Collapsed;
}

void ImGui::End() {
    // Window end
}

ImVec2 ImGui::GetWindowPos() { return GImGui ? GImGui->Window.Pos : ImVec2(0, 0); }
ImVec2 ImGui::GetWindowSize() { return GImGui ? GImGui->Window.Size : ImVec2(0, 0); }
ImDrawList* ImGui::GetWindowDrawList() { return GImGui ? &GImGui->DrawList : nullptr; }

// -----------------------------------------------------------------------------
// Text, Layout & Separators
// -----------------------------------------------------------------------------
void ImGui::TextUnformatted(const char* text, const char* text_end) {
    if (!GImGui || !text) return;
    ImGuiWindow& win = GImGui->Window;
    ImU32 col = GImGui->Style.Colors[ImGuiCol_Text];

    GImGui->DrawList.AddText(win.CursorPos, col, text, text_end);
    int len = text_end ? (int)(text_end - text) : (int)strlen(text);
    float w = (float)len * 7.0f;
    float h = 15.0f;

    if (win.CursorPos.y + h > win.LineMaxY) win.LineMaxY = win.CursorPos.y + h;

    if (!win.SameLinePending) {
        win.CursorPos.y = win.LineMaxY + GImGui->Style.ItemSpacing.y;
        win.CursorPos.x = win.Pos.x + 10.0f;
    } else {
        win.CursorPos.x += w + GImGui->Style.ItemSpacing.x;
        win.SameLinePending = false;
    }
}

void ImGui::Text(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    TextUnformatted(buf);
}

void ImGui::TextDisabled(const char* fmt, ...) {
    if (!GImGui || !fmt) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ImGuiWindow& win = GImGui->Window;
    ImU32 col = GImGui->Style.Colors[ImGuiCol_TextDisabled];
    GImGui->DrawList.AddText(win.CursorPos, col, buf);
    float w = (float)strlen(buf) * 7.0f;
    float h = 15.0f;

    if (win.CursorPos.y + h > win.LineMaxY) win.LineMaxY = win.CursorPos.y + h;

    if (!win.SameLinePending) {
        win.CursorPos.y = win.LineMaxY + GImGui->Style.ItemSpacing.y;
        win.CursorPos.x = win.Pos.x + 10.0f;
    } else {
        win.CursorPos.x += w + GImGui->Style.ItemSpacing.x;
        win.SameLinePending = false;
    }
}

void ImGui::SameLine(float offset_from_start_x, float spacing) {
    if (!GImGui) return;
    ImGuiWindow& win = GImGui->Window;
    if (offset_from_start_x != 0.0f) {
        win.CursorPos.x = win.Pos.x + 10.0f + offset_from_start_x;
    } else if (spacing >= 0.0f) {
        win.CursorPos.x += spacing;
    }
    win.SameLinePending = true;
}

void ImGui::Separator() {
    if (!GImGui) return;
    ImGuiWindow& win = GImGui->Window;
    float x1 = win.Pos.x + 10.0f;
    float x2 = win.Pos.x + win.Size.x - 10.0f;
    float y = win.CursorPos.y + 4.0f;

    ImU32 col = GImGui->Style.Colors[ImGuiCol_Separator];
    GImGui->DrawList.AddLine(ImVec2(x1, y), ImVec2(x2, y), col, 1.0f);
    win.CursorPos.y = y + 6.0f;
    win.CursorPos.x = x1;
    win.LineMaxY = win.CursorPos.y;
    win.SameLinePending = false;
}

void ImGui::SeparatorText(const char* label) {
    if (!GImGui || !label) return;
    ImGuiWindow& win = GImGui->Window;
    float x1 = win.Pos.x + 10.0f;
    float x2 = win.Pos.x + win.Size.x - 10.0f;
    float y = win.CursorPos.y + 6.0f;

    ImU32 text_col = GImGui->Style.Colors[ImGuiCol_Text];
    ImU32 line_col = GImGui->Style.Colors[ImGuiCol_Separator];

    float tw = (float)strlen(label) * 7.0f;
    GImGui->DrawList.AddText(ImVec2(x1, y), text_col, label);
    GImGui->DrawList.AddLine(ImVec2(x1 + tw + 8.0f, y + 7.0f), ImVec2(x2, y + 7.0f), line_col, 1.0f);

    win.CursorPos.y = y + 18.0f;
    win.CursorPos.x = x1;
    win.LineMaxY = win.CursorPos.y;
    win.SameLinePending = false;
}

bool ImGui::IsItemHovered() {
    return GImGui ? GImGui->LastItemHovered : false;
}

void ImGui::SetTooltip(const char* fmt, ...) {
    if (!GImGui || !fmt) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(GImGui->TooltipText, sizeof(GImGui->TooltipText), fmt, args);
    va_end(args);
    GImGui->HasTooltip = true;
}
