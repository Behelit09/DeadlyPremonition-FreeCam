// Dear ImGui Win32 Backend Implementation (imgui_impl_win32.cpp)
#include "imgui_impl_win32.h"
#include <windows.h>

static HWND g_hWnd = nullptr;

bool ImGui_ImplWin32_Init(void* hwnd) {
    g_hWnd = (HWND)hwnd;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    return true;
}

void ImGui_ImplWin32_Shutdown() {
    g_hWnd = nullptr;
}

void ImGui_ImplWin32_NewFrame() {
    ImGuiIO& io = ImGui::GetIO();
    if (g_hWnd) {
        RECT rect = { 0, 0, 0, 0 };
        GetClientRect(g_hWnd, &rect);
        if (rect.right > 0 && rect.bottom > 0) {
            io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
        }
    }
}

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() == nullptr) return 0;
    ImGuiIO& io = ImGui::GetIO();

    switch (msg) {
        case WM_MOUSEMOVE: {
            io.AddMousePosEvent((float)(short)LOWORD(lParam), (float)(short)HIWORD(lParam));
            return 0;
        }
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
            io.AddMouseButtonEvent(0, true);
            return 0;
        }
        case WM_LBUTTONUP: {
            io.AddMouseButtonEvent(0, false);
            return 0;
        }
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: {
            io.AddMouseButtonEvent(1, true);
            return 0;
        }
        case WM_RBUTTONUP: {
            io.AddMouseButtonEvent(1, false);
            return 0;
        }
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: {
            io.AddMouseButtonEvent(2, true);
            return 0;
        }
        case WM_MBUTTONUP: {
            io.AddMouseButtonEvent(2, false);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
            return 0;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN: {
            if (wParam < 512) io.AddKeyEvent((int)wParam, true);
            return 0;
        }
        case WM_KEYUP: case WM_SYSKEYUP: {
            if (wParam < 512) io.AddKeyEvent((int)wParam, false);
            return 0;
        }
    }
    return 0;
}
