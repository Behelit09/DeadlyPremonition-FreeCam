// Dear ImGui Widgets Implementation (imgui_widgets.cpp)
#include "imgui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern ImGuiContext* GImGui;

// -----------------------------------------------------------------------------
// Button
// -----------------------------------------------------------------------------
bool ImGui::Button(const char* label, const ImVec2& size_arg) {
    if (!GImGui || !label) return false;
    ImGuiIO& io = GImGui->IO;
    ImGuiStyle& style = GImGui->Style;
    ImDrawList& draw = GImGui->DrawList;

    float text_w = (float)strlen(label) * 7.0f;
    float w = size_arg.x > 0.0f ? size_arg.x : text_w + style.FramePadding.x * 4.0f;
    float h = size_arg.y > 0.0f ? size_arg.y : 21.0f;

    ImVec2 pos = GImGui->Window.CursorPos;
    ImVec2 bb_max = ImVec2(pos.x + w, pos.y + h);

    bool hovered = (io.MousePos.x >= pos.x && io.MousePos.x <= bb_max.x &&
                    io.MousePos.y >= pos.y && io.MousePos.y <= bb_max.y);
    GImGui->LastItemHovered = hovered;

    bool clicked = hovered && io.MouseClicked[0];

    ImU32 col = hovered ? (io.MouseDown[0] ? style.Colors[ImGuiCol_ButtonActive] : style.Colors[ImGuiCol_ButtonHovered])
                        : style.Colors[ImGuiCol_Button];
    ImU32 text_col = style.Colors[ImGuiCol_Text];

    draw.AddRectFilled(pos, bb_max, col, style.FrameRounding);
    float text_x = pos.x + (w - text_w) * 0.5f;
    float text_y = pos.y + (h - 13.0f) * 0.5f;
    draw.AddText(ImVec2(text_x, text_y), text_col, label);

    if (pos.y + h > GImGui->Window.LineMaxY) GImGui->Window.LineMaxY = pos.y + h;

    if (!GImGui->Window.SameLinePending) {
        GImGui->Window.CursorPos.y = GImGui->Window.LineMaxY + style.ItemSpacing.y;
        GImGui->Window.CursorPos.x = GImGui->Window.Pos.x + 10.0f;
    } else {
        GImGui->Window.CursorPos.x += w + style.ItemSpacing.x;
        GImGui->Window.SameLinePending = false;
    }

    return clicked;
}

// -----------------------------------------------------------------------------
// Checkbox
// -----------------------------------------------------------------------------
bool ImGui::Checkbox(const char* label, bool* v) {
    if (!GImGui || !label || !v) return false;
    int vi = *v ? 1 : 0;
    bool res = Checkbox(label, &vi);
    *v = (vi != 0);
    return res;
}

bool ImGui::Checkbox(const char* label, int* v) {
    if (!GImGui || !label || !v) return false;
    ImGuiIO& io = GImGui->IO;
    ImGuiStyle& style = GImGui->Style;
    ImDrawList& draw = GImGui->DrawList;

    ImVec2 pos = GImGui->Window.CursorPos;
    float square_sz = 14.0f;
    float text_w = (float)strlen(label) * 7.0f;
    float total_w = square_sz + style.ItemSpacing.x + text_w;
    float total_h = square_sz + 4.0f;

    ImVec2 bb_max = ImVec2(pos.x + total_w, pos.y + total_h);

    bool hovered = (io.MousePos.x >= pos.x && io.MousePos.x <= bb_max.x &&
                    io.MousePos.y >= pos.y && io.MousePos.y <= bb_max.y);
    GImGui->LastItemHovered = hovered;

    bool toggled = false;
    if (hovered && io.MouseClicked[0]) {
        *v = !(*v);
        toggled = true;
    }

    ImU32 frame_col = hovered ? (io.MouseDown[0] ? style.Colors[ImGuiCol_FrameBgActive] : style.Colors[ImGuiCol_FrameBgHovered])
                              : style.Colors[ImGuiCol_FrameBg];
    ImU32 border_col = style.Colors[ImGuiCol_Border];
    ImU32 check_col = style.Colors[ImGuiCol_CheckMark];
    ImU32 text_col = style.Colors[ImGuiCol_Text];

    ImVec2 box_min = ImVec2(pos.x, pos.y + 2.0f);
    ImVec2 box_max = ImVec2(pos.x + square_sz, pos.y + 2.0f + square_sz);

    draw.AddRectFilled(box_min, box_max, frame_col);
    draw.AddRect(box_min, box_max, border_col);

    if (*v) {
        // Draw vector checkmark matching Dear ImGui
        draw.AddLine(ImVec2(box_min.x + 3.0f, box_min.y + 7.0f), ImVec2(box_min.x + 6.0f, box_min.y + 11.0f), check_col, 2.0f);
        draw.AddLine(ImVec2(box_min.x + 6.0f, box_min.y + 11.0f), ImVec2(box_min.x + 12.0f, box_min.y + 3.5f), check_col, 2.0f);
    }

    draw.AddText(ImVec2(pos.x + square_sz + 8.0f, pos.y + 2.0f), text_col, label);

    if (pos.y + total_h > GImGui->Window.LineMaxY) GImGui->Window.LineMaxY = pos.y + total_h;

    if (!GImGui->Window.SameLinePending) {
        GImGui->Window.CursorPos.y = GImGui->Window.LineMaxY + style.ItemSpacing.y;
        GImGui->Window.CursorPos.x = GImGui->Window.Pos.x + 10.0f;
    } else {
        GImGui->Window.CursorPos.x += total_w + style.ItemSpacing.x;
        GImGui->Window.SameLinePending = false;
    }

    return toggled;
}

// -----------------------------------------------------------------------------
// SliderFloat
// -----------------------------------------------------------------------------
bool ImGui::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    (void)flags;
    if (!GImGui || !label || !v) return false;
    ImGuiIO& io = GImGui->IO;
    ImGuiStyle& style = GImGui->Style;
    ImDrawList& draw = GImGui->DrawList;

    int slider_id = GImGui->NextItemId++;
    ImVec2 pos = GImGui->Window.CursorPos;
    float track_w = 200.0f;
    float track_h = 19.0f;

    ImVec2 bb_max = ImVec2(pos.x + track_w, pos.y + track_h);
    bool hovered = (io.MousePos.x >= pos.x && io.MousePos.x <= bb_max.x &&
                    io.MousePos.y >= pos.y && io.MousePos.y <= bb_max.y);
    GImGui->LastItemHovered = hovered;

    bool value_changed = false;
    if (hovered && io.MouseClicked[0] && GImGui->ActiveSliderId == -1) {
        GImGui->ActiveSliderId = slider_id;
    }
    if (!io.MouseDown[0] && GImGui->ActiveSliderId == slider_id) {
        GImGui->ActiveSliderId = -1;
    }

    if (GImGui->ActiveSliderId == slider_id) {
        float norm = (io.MousePos.x - pos.x) / track_w;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        float new_v = v_min + norm * (v_max - v_min);
        if (new_v != *v) {
            *v = new_v;
            value_changed = true;
        }
    }

    ImU32 frame_col = style.Colors[ImGuiCol_FrameBg];
    ImU32 border_col = style.Colors[ImGuiCol_Border];
    ImU32 grab_col = (GImGui->ActiveSliderId == slider_id) ? style.Colors[ImGuiCol_SliderGrabActive] : style.Colors[ImGuiCol_SliderGrab];
    ImU32 text_col = style.Colors[ImGuiCol_Text];

    // Track frame
    draw.AddRectFilled(pos, bb_max, frame_col);
    draw.AddRect(pos, bb_max, border_col);

    // Grab handle
    float norm = (*v - v_min) / (v_max - v_min);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    float grab_w = 8.0f;
    float grab_x = pos.x + norm * (track_w - grab_w);
    draw.AddRectFilled(ImVec2(grab_x, pos.y + 1.0f), ImVec2(grab_x + grab_w, pos.y + track_h - 1.0f), grab_col);

    // Centered value text
    char val_buf[64];
    snprintf(val_buf, sizeof(val_buf), format ? format : "%.3f", *v);
    float val_w = (float)strlen(val_buf) * 7.0f;
    draw.AddText(ImVec2(pos.x + (track_w - val_w) * 0.5f, pos.y + 3.0f), text_col, val_buf);

    // Right-hand label
    draw.AddText(ImVec2(pos.x + track_w + 10.0f, pos.y + 3.0f), text_col, label);

    if (pos.y + track_h > GImGui->Window.LineMaxY) GImGui->Window.LineMaxY = pos.y + track_h;

    if (!GImGui->Window.SameLinePending) {
        GImGui->Window.CursorPos.y = GImGui->Window.LineMaxY + style.ItemSpacing.y;
        GImGui->Window.CursorPos.x = GImGui->Window.Pos.x + 10.0f;
    } else {
        float label_w = (float)strlen(label) * 7.0f;
        GImGui->Window.CursorPos.x += track_w + 10.0f + label_w + style.ItemSpacing.x;
        GImGui->Window.SameLinePending = false;
    }

    return value_changed;
}

// -----------------------------------------------------------------------------
// TabBar & TabItem
// -----------------------------------------------------------------------------
bool ImGui::BeginTabBar(const char* str_id, ImGuiTabBarFlags flags) {
    (void)flags; (void)str_id;
    if (!GImGui) return false;
    ImGuiWindow& win = GImGui->Window;
    win.TabBarPos = win.CursorPos;
    win.TabBarCurX = win.CursorPos.x;
    win.TabBarHeight = 22.0f;
    win.TabContentY = win.CursorPos.y + win.TabBarHeight + 10.0f;
    return true;
}

void ImGui::EndTabBar() {
    if (!GImGui) return;
    ImGuiWindow& win = GImGui->Window;
    ImGuiStyle& style = GImGui->Style;
    ImDrawList& draw = GImGui->DrawList;

    float lineY = win.TabBarPos.y + win.TabBarHeight;
    float x1 = win.Pos.x + 10.0f;
    float x2 = win.Pos.x + win.Size.x - 10.0f;

    draw.AddLine(ImVec2(x1, lineY), ImVec2(x2, lineY), style.Colors[ImGuiCol_TabActive], 2.0f);
}

bool ImGui::BeginTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags) {
    (void)p_open; (void)flags;
    if (!GImGui || !label) return false;
    ImGuiWindow& win = GImGui->Window;
    ImGuiIO& io = GImGui->IO;
    ImGuiStyle& style = GImGui->Style;
    ImDrawList& draw = GImGui->DrawList;

    float text_w = (float)strlen(label) * 7.0f;
    float tab_w = text_w + 18.0f;
    float tab_h = win.TabBarHeight;

    ImVec2 pos = ImVec2(win.TabBarCurX, win.TabBarPos.y);
    ImVec2 bb_max = ImVec2(pos.x + tab_w, pos.y + tab_h);

    bool hovered = (io.MousePos.x >= pos.x && io.MousePos.x <= bb_max.x &&
                    io.MousePos.y >= pos.y && io.MousePos.y <= bb_max.y);
    GImGui->LastItemHovered = hovered;

    if (hovered && io.MouseClicked[0]) {
        memset(win.ActiveTab, 0, sizeof(win.ActiveTab));
        strncpy(win.ActiveTab, label, sizeof(win.ActiveTab) - 1);
    }

    bool active = (strcmp(win.ActiveTab, label) == 0);

    ImU32 col = active ? style.Colors[ImGuiCol_TabActive] : (hovered ? style.Colors[ImGuiCol_TabHovered] : style.Colors[ImGuiCol_Tab]);
    ImU32 text_col = style.Colors[ImGuiCol_Text];

    draw.AddRectFilled(pos, bb_max, col);
    draw.AddText(ImVec2(pos.x + 9.0f, pos.y + 4.0f), text_col, label);

    win.TabBarCurX += tab_w + 4.0f;

    if (active) {
        win.CursorPos = ImVec2(win.Pos.x + 10.0f, win.TabContentY);
        win.LineMaxY = win.CursorPos.y;
        win.SameLinePending = false;
        return true;
    }
    return false;
}

void ImGui::EndTabItem() {
    // EndTabItem
}

// -----------------------------------------------------------------------------
// CollapsingHeader
// -----------------------------------------------------------------------------
bool ImGui::CollapsingHeader(const char* label, ImGuiWindowFlags flags) {
    static bool s_open = true;
    return CollapsingHeader(label, &s_open, flags);
}

bool ImGui::CollapsingHeader(const char* label, bool* p_visible, ImGuiWindowFlags flags) {
    (void)flags;
    if (!GImGui || !label || !p_visible) return false;
    ImGuiIO& io = GImGui->IO;
    ImGuiStyle& style = GImGui->Style;
    ImDrawList& draw = GImGui->DrawList;

    ImVec2 pos = GImGui->Window.CursorPos;
    float w = GImGui->Window.Size.x - 20.0f;
    float h = 22.0f;

    ImVec2 bb_max = ImVec2(pos.x + w, pos.y + h);
    bool hovered = (io.MousePos.x >= pos.x && io.MousePos.x <= bb_max.x &&
                    io.MousePos.y >= pos.y && io.MousePos.y <= bb_max.y);
    GImGui->LastItemHovered = hovered;

    if (hovered && io.MouseClicked[0]) {
        *p_visible = !(*p_visible);
    }

    ImU32 col = hovered ? style.Colors[ImGuiCol_HeaderHovered] : style.Colors[ImGuiCol_Header];
    ImU32 text_col = style.Colors[ImGuiCol_Text];

    draw.AddRectFilled(pos, bb_max, col);

    // Arrow indicator
    if (*p_visible) {
        draw.AddTriangleFilled(ImVec2(pos.x + 6.0f, pos.y + 7.0f), ImVec2(pos.x + 16.0f, pos.y + 7.0f), ImVec2(pos.x + 11.0f, pos.y + 14.0f), text_col);
    } else {
        draw.AddTriangleFilled(ImVec2(pos.x + 8.0f, pos.y + 6.0f), ImVec2(pos.x + 8.0f, pos.y + 16.0f), ImVec2(pos.x + 15.0f, pos.y + 11.0f), text_col);
    }

    draw.AddText(ImVec2(pos.x + 22.0f, pos.y + 4.0f), text_col, label);

    GImGui->Window.CursorPos.y = pos.y + h + style.ItemSpacing.y;
    GImGui->Window.CursorPos.x = GImGui->Window.Pos.x + 10.0f;
    GImGui->Window.LineMaxY = GImGui->Window.CursorPos.y;
    GImGui->Window.SameLinePending = false;

    return *p_visible;
}
