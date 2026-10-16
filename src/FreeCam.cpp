// ==============================================================================
// Deadly Premonition: The Director's Cut
// FreeCam & HUD Hide ASI Mod (FreeCam.asi)
//
// Integrated with Official Dear ImGui C++ Library
// ==============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx9.h"

extern "C" {
void* malloc(size_t sz) { return HeapAlloc(GetProcessHeap(), 0, sz ? sz : 1); }
void free(void* p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
void* realloc(void* p, size_t sz) {
    if (!p) return malloc(sz);
    if (!sz) { free(p); return nullptr; }
    return HeapReAlloc(GetProcessHeap(), 0, p, sz);
}
void* memset(void* dst, int val, size_t size) {
    unsigned char* p = (unsigned char*)dst;
    for (size_t i = 0; i < size; ++i) p[i] = (unsigned char)val;
    return dst;
}
void* memcpy(void* dst, const void* src, size_t size) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < size; ++i) d[i] = s[i];
    return dst;
}
size_t strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}
char* strncpy(char* dst, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}
char* strncat(char* dst, const char* src, size_t n) {
    size_t dlen = strlen(dst);
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[dlen + i] = src[i];
    dst[dlen + i] = '\0';
    return dst;
}
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
char* strrchr(const char* s, int c) {
    const char* last = nullptr;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == '\0') return (char*)s;
    return (char*)last;
}
float sinf(float x) {
    float r;
    __asm__ volatile ("fsin" : "=t"(r) : "0"(x));
    return r;
}
float cosf(float x) {
    float r;
    __asm__ volatile ("fcos" : "=t"(r) : "0"(x));
    return r;
}
float sqrtf(float x) {
    float r;
    __asm__ volatile ("fsqrt" : "=t"(r) : "0"(x));
    return r;
}
float atan2f(float y, float x) {
    float r;
    __asm__ volatile ("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}
float fabsf(float x) { return (x < 0.0f) ? -x : x; }

double atof(const char* s) {
    if (!s) return 0.0;
    while (*s == ' ' || *s == '\t') s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    double val = 0.0;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (double)(*s - '0'); s++; }
    if (*s == '.') {
        s++;
        double f = 0.1;
        while (*s >= '0' && *s <= '9') { val += (double)(*s - '0') * f; f *= 0.1; s++; }
    }
    return val * (double)sign;
}

static void FormatFloatBuf(char* out, int outSize, float val, int decimals) {
    if (outSize <= 0) return;
    int sign = 0;
    if (val < 0.0f) { sign = 1; val = -val; }
    int intPart = (int)val;
    float fracPart = val - (float)intPart;

    char temp[64];
    int pos = 0;
    if (sign) temp[pos++] = '-';

    char intBuf[32];
    int intPos = 0;
    if (intPart == 0) {
        intBuf[intPos++] = '0';
    } else {
        int tempInt = intPart;
        while (tempInt > 0) {
            intBuf[intPos++] = '0' + (tempInt % 10);
            tempInt /= 10;
        }
    }
    for (int i = intPos - 1; i >= 0; i--) temp[pos++] = intBuf[i];

    if (decimals > 0) {
        temp[pos++] = '.';
        for (int d = 0; d < decimals; d++) {
            fracPart *= 10.0f;
            int digit = (int)fracPart;
            if (digit > 9) digit = 9;
            temp[pos++] = '0' + digit;
            fracPart -= (float)digit;
        }
    }
    temp[pos] = '\0';
    int copyLen = pos < outSize - 1 ? pos : outSize - 1;
    for (int i = 0; i < copyLen; i++) out[i] = temp[i];
    out[copyLen] = '\0';
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    if (!buf || size == 0 || !fmt) return 0;
    size_t outPos = 0;

    while (*fmt && outPos + 1 < size) {
        if (*fmt != '%') {
            buf[outPos++] = *fmt++;
            continue;
        }
        fmt++; // skip '%'
        if (*fmt == '%') {
            buf[outPos++] = '%';
            fmt++;
            continue;
        }

        bool zeroPad = false;
        int width = 0;
        int precision = -1;

        if (*fmt == '0') {
            zeroPad = true;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10 + (*fmt - '0');
                fmt++;
            }
        }

        char spec = *fmt++;
        if (spec == 's') {
            const char* s = va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s && outPos + 1 < size) buf[outPos++] = *s++;
        } else if (spec == 'c') {
            char c = (char)va_arg(args, int);
            buf[outPos++] = c;
        } else if (spec == 'd' || spec == 'i') {
            int val = va_arg(args, int);
            char numBuf[32];
            int npos = 0;
            unsigned int uval = (val < 0) ? (unsigned int)(-val) : (unsigned int)val;
            if (uval == 0) numBuf[npos++] = '0';
            else {
                while (uval > 0) {
                    numBuf[npos++] = '0' + (uval % 10);
                    uval /= 10;
                }
            }
            int pad = width - npos - (val < 0 ? 1 : 0);
            if (val < 0 && zeroPad) { if (outPos + 1 < size) buf[outPos++] = '-'; }
            while (!zeroPad && pad > 0) { if (outPos + 1 < size) buf[outPos++] = ' '; pad--; }
            if (val < 0 && !zeroPad) { if (outPos + 1 < size) buf[outPos++] = '-'; }
            while (zeroPad && pad > 0) { if (outPos + 1 < size) buf[outPos++] = '0'; pad--; }
            for (int i = npos - 1; i >= 0; i--) { if (outPos + 1 < size) buf[outPos++] = numBuf[i]; }
        } else if (spec == 'u') {
            unsigned int uval = va_arg(args, unsigned int);
            char numBuf[32];
            int npos = 0;
            if (uval == 0) numBuf[npos++] = '0';
            else {
                while (uval > 0) {
                    numBuf[npos++] = '0' + (uval % 10);
                    uval /= 10;
                }
            }
            int pad = width - npos;
            while (pad > 0) { if (outPos + 1 < size) buf[outPos++] = (zeroPad ? '0' : ' '); pad--; }
            for (int i = npos - 1; i >= 0; i--) { if (outPos + 1 < size) buf[outPos++] = numBuf[i]; }
        } else if (spec == 'x' || spec == 'X') {
            unsigned int uval = va_arg(args, unsigned int);
            const char* hexChars = (spec == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            char numBuf[32];
            int npos = 0;
            if (uval == 0) numBuf[npos++] = '0';
            else {
                while (uval > 0) {
                    numBuf[npos++] = hexChars[uval & 0xF];
                    uval >>= 4;
                }
            }
            int pad = width - npos;
            while (pad > 0) { if (outPos + 1 < size) buf[outPos++] = (zeroPad ? '0' : ' '); pad--; }
            for (int i = npos - 1; i >= 0; i--) { if (outPos + 1 < size) buf[outPos++] = numBuf[i]; }
        } else if (spec == 'p') {
            void* ptr = va_arg(args, void*);
            uintptr_t uval = (uintptr_t)ptr;
            const char* hexChars = "0123456789ABCDEF";
            char numBuf[16];
            for (int i = 7; i >= 0; i--) {
                numBuf[i] = hexChars[uval & 0xF];
                uval >>= 4;
            }
            for (int i = 0; i < 8; i++) { if (outPos + 1 < size) buf[outPos++] = numBuf[i]; }
        } else if (spec == 'f') {
            double v = va_arg(args, double);
            char fStr[64];
            FormatFloatBuf(fStr, sizeof(fStr), (float)v, precision >= 0 ? precision : 3);
            const char* s = fStr;
            while (*s && outPos + 1 < size) buf[outPos++] = *s++;
        } else {
            buf[outPos++] = spec;
        }
    }
    buf[outPos] = '\0';
    return (int)outPos;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return r;
}
}

void* operator new(size_t size) { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete[](void* ptr) noexcept { free(ptr); }
void operator delete(void* ptr, size_t size) noexcept { free(ptr); }
void operator delete[](void* ptr, size_t size) noexcept { free(ptr); }

// -----------------------------------------------------------------------------
// Math Constants & Helpers
// -----------------------------------------------------------------------------
#define PI 3.14159265358979323846f
#define DEG2RAD(d) ((d) * (PI / 180.0f))
#define RAD2DEG(r) ((r) * (180.0f / PI))

static inline float FastSin(float x) { return sinf(x); }
static inline float FastCos(float x) { return cosf(x); }
static inline float FastSqrt(float x) { return sqrtf(x); }
static inline float FastAtan2(float y, float x) { return atan2f(y, x); }
static inline float FastAbs(float x) { return fabsf(x); }

typedef struct {
    float x, y, z;
} Vec3;

static inline Vec3 Vec3_Add(Vec3 a, Vec3 b) {
    Vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

static inline Vec3 Vec3_Sub(Vec3 a, Vec3 b) {
    Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static inline Vec3 Vec3_Mul(Vec3 a, float s) {
    Vec3 r = { a.x * s, a.y * s, a.z * s };
    return r;
}

// -----------------------------------------------------------------------------
// Engine Addresses & Memory Signatures
// -----------------------------------------------------------------------------
#define ADDR_CAMERA_CONTEXT        0x00BD9648  // Global CRdCameraContext instance
#define ADDR_CALC_VIEW_PROJ        0x006B62E0  // Builds View, Proj, Frustum Banks
#define ADDR_CAMERA_HOOK_SITE      0x00401BD1  // 5-byte CALL 0x006B62E0 in main frame
#define ADDR_HUD_DRAW_FUNC         0x005FFE80  // 2D HUD Render routine
#define ADDR_HUD_JE_SITE           0x005FFEB2  // 6-byte JE 0x00602AF7 in HUD render routine
#define ADDR_HUD_ENABLE_FLAG       0x0147D88C  // Global HUD enable byte
#define ADDR_RADAR_DRAW_FUNC       0x006541B0  // Map/Radar Render routine
#define ADDR_RADAR_FLAG            0x01476A1C  // Global Radar display flag byte
#define ADDR_MENU_OPEN_FLAG        0x0146FC38  // Global Pause / Full-screen Map open byte
#define ADDR_D3D9_DEVICE_PTR       0x01480984  // Global IDirect3DDevice9* pointer
#define ADDR_ACTOR_MGR             0x00BD7670  // Global Actor Manager pointer
#define ADDR_PLAYER_HANDLE         0x008A9BA4  // Global Player Actor Handle
#define ADDR_RESOLVE_ACTOR         0x006C5FD0  // ResolveActor(pActorMgr, handle)
#define ADDR_IAT_SETCURSORPOS      0x0076E1F0  // DP.exe IAT entry for SetCursorPos
#define ADDR_IAT_GETCURSORPOS      0x0076E1F4  // DP.exe IAT entry for GetCursorPos
#define ADDR_IAT_GETASYNCKEYSTATE  0x0076E248  // DP.exe IAT entry for GetAsyncKeyState

// -----------------------------------------------------------------------------
// Configuration & Mod State
// -----------------------------------------------------------------------------
static int   g_MenuKey          = VK_F9;
static int   g_FreeCamKey       = VK_F5;
static int   g_HudHideKey       = VK_F4;
static int   g_FreezePlayer     = 1;
static float g_MoveSpeed        = 15.0f;
static float g_FastMultiplier   = 3.0f;
static float g_SlowMultiplier   = 0.25f;
static float g_MouseSensitivity = 0.003f;
static int   g_InvertPitch      = 0;
static int   g_InvertYaw        = 0;
static float g_TargetFOV_Deg    = 65.0f;
static int   g_CustomFOVEnabled = 0;

static bool  g_MenuOpen         = false;
static int   g_FreeCamActive    = 0;
extern "C" volatile int g_HUDHidden = 0;
static volatile int g_FreeCamInitDone = 0;

static Vec3  g_CamPos           = { 0.0f, 0.0f, 0.0f };
static Vec3  g_CamTarget        = { 0.0f, 0.0f, 1.0f };
static float g_CamPitch         = 0.0f;
static float g_CamYaw           = 0.0f;
static float g_CamFOV_Rad       = 1.134464f;

static int   g_MouseLookWasActive = 0;
static int   g_MenuKeyLatched    = 0;
static int   g_FreeCamKeyLatched = 0;
static int   g_HudKeyLatched     = 0;

static char  g_StatusMessage[160] = "Ready. Press F9 to toggle FreeCam Settings, F5 for FreeCam, F4 for HUD.";

static HWND  g_GameHWnd         = NULL;
static WNDPROC g_OrigWndProc    = NULL;

typedef HRESULT (WINAPI *D3D9ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (WINAPI *D3D9PresentFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT (WINAPI *D3D9EndSceneFn)(IDirect3DDevice9*);

static D3D9ResetFn    g_OrigReset    = NULL;
static D3D9PresentFn  g_OrigPresent  = NULL;
static D3D9EndSceneFn g_OrigEndScene = NULL;

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------
static void ApplyHUDState(int hide);
static int  TeleportYorkToCamera(void);
static int  ResetCameraToPlayer(void);
extern "C" void __cdecl CameraHookWorker(void* pCamCtx);
extern "C" __attribute__((naked)) void CameraHookAsm(void);
extern "C" __attribute__((naked)) void RadarHookAsm(void);

// -----------------------------------------------------------------------------
// ZachFix Interop & Menu State Detection
// -----------------------------------------------------------------------------
static volatile const bool* g_pZachFixMenuOpen = NULL;
static int g_ZachFixScanAttempted = 0;

static bool IsZachFixMenuOpen(void) {
    if (g_pZachFixMenuOpen && !IsBadReadPtr((const void*)g_pZachFixMenuOpen, sizeof(bool))) {
        return *g_pZachFixMenuOpen;
    }

    if (!g_ZachFixScanAttempted) {
        HMODULE hZach = GetModuleHandleA("ZachFix.asi");
        if (!hZach) hZach = GetModuleHandleA("ZachFix");
        if (hZach) {
            uintptr_t base = (uintptr_t)hZach;
            // 1. Direct offset check: in ZachFix release build, base + 0x3ebff is:
            // 6a 40 68 [pOpen: 4 bytes] 68 [pTitle: 4 bytes]
            const uint8_t* p = (const uint8_t*)(base + 0x3ebff);
            if (!IsBadReadPtr(p, 16)) {
                if (p[0] == 0x6a && p[1] == 0x40 && p[2] == 0x68 && p[7] == 0x68) {
                    const char* title = *(const char* const*)(p + 8);
                    if (!IsBadStringPtrA(title, 32) && strcmp(title, "ZachFix Settings") == 0) {
                        g_pZachFixMenuOpen = *(const bool* const*)(p + 3);
                        g_ZachFixScanAttempted = 1;
                        return *g_pZachFixMenuOpen;
                    }
                }
            }

            // 2. Fallback scan across ZachFix text section:
            PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
            if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
                PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
                if (nt->Signature == IMAGE_NT_SIGNATURE) {
                    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
                    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
                        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                            const uint8_t* start = (const uint8_t*)(base + sec[i].VirtualAddress);
                            DWORD len = sec[i].Misc.VirtualSize;
                            if (len > 16 && !IsBadReadPtr(start, len)) {
                                for (DWORD off = 0; off + 16 < len; off++) {
                                    if (start[off] == 0x6a && start[off + 1] == 0x40 &&
                                        start[off + 2] == 0x68 && start[off + 7] == 0x68) {
                                        const char* title = *(const char* const*)(start + off + 8);
                                        if (!IsBadStringPtrA(title, 32) && strcmp(title, "ZachFix Settings") == 0) {
                                            g_pZachFixMenuOpen = *(const bool* const*)(start + off + 3);
                                            g_ZachFixScanAttempted = 1;
                                            return *g_pZachFixMenuOpen;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            g_ZachFixScanAttempted = 1;
        }
    }

    if (g_pZachFixMenuOpen && !IsBadReadPtr((const void*)g_pZachFixMenuOpen, sizeof(bool))) {
        return *g_pZachFixMenuOpen;
    }
    return false;
}

// -----------------------------------------------------------------------------
// IAT Hooks & Caller-Preserving Tail Calls
// -----------------------------------------------------------------------------
typedef BOOL  (WINAPI *SetCursorPosFn)(int x, int y);
typedef BOOL  (WINAPI *GetCursorPosFn)(LPPOINT lpPoint);
typedef SHORT (WINAPI *GetAsyncKeyStateFn)(int vKey);

extern "C" SetCursorPosFn     g_Orig_DP_SetCursorPos     = NULL;
extern "C" GetCursorPosFn     g_Orig_DP_GetCursorPos     = NULL;
extern "C" GetAsyncKeyStateFn g_Orig_DP_GetAsyncKeyState = NULL;

extern "C" int __cdecl ShouldSuppressGameCursor(void) {
    if (g_MenuOpen && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        return 1;
    }
    if (IsZachFixMenuOpen()) {
        return 1;
    }
    return 0;
}

extern "C" BOOL __cdecl SetCursorCenter(LPPOINT lpPoint) {
    if (!lpPoint) return FALSE;
    if (!g_GameHWnd) g_GameHWnd = GetActiveWindow();
    if (g_GameHWnd) {
        RECT rc;
        if (GetClientRect(g_GameHWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
            POINT center = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
            ClientToScreen(g_GameHWnd, &center);
            *lpPoint = center;
            return TRUE;
        }
    }
    lpPoint->x = GetSystemMetrics(SM_CXSCREEN) / 2;
    lpPoint->y = GetSystemMetrics(SM_CYSCREEN) / 2;
    return TRUE;
}

extern "C" int __cdecl ShouldSuppressGameKey(int vKey) {
    if (g_MenuOpen && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        return 1;
    }
    if (IsZachFixMenuOpen()) {
        return 1;
    }
    if (g_FreeCamActive && g_FreezePlayer) {
        if (vKey == 'W' || vKey == 'S' || vKey == 'A' || vKey == 'D' ||
            vKey == 'Q' || vKey == 'E' || vKey == 'C' || vKey == 'R' ||
            vKey == 'F' || vKey == 'Z' || vKey == 'X' ||
            vKey == VK_UP || vKey == VK_DOWN || vKey == VK_LEFT || vKey == VK_RIGHT ||
            vKey == VK_SHIFT || vKey == VK_CONTROL || vKey == VK_SPACE ||
            vKey == VK_LBUTTON || vKey == VK_RBUTTON) {
            return 1;
        }
    }
    return 0;
}

extern "C" __attribute__((naked)) BOOL WINAPI Hook_DP_SetCursorPos(int x, int y) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "push ecx\n"
        "push edx\n"
        "call _ShouldSuppressGameCursor\n"
        "pop edx\n"
        "pop ecx\n"
        "test eax, eax\n"
        "jz 1f\n"
        "mov eax, 1\n"
        "ret 8\n"
        "1:\n"
        "mov eax, dword ptr [_g_Orig_DP_SetCursorPos]\n"
        "test eax, eax\n"
        "jz 2f\n"
        "jmp eax\n"
        "2:\n"
        "mov eax, 1\n"
        "ret 8\n"
        ".att_syntax\n"
    );
}

extern "C" __attribute__((naked)) BOOL WINAPI Hook_DP_GetCursorPos(LPPOINT lpPoint) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "push ecx\n"
        "push edx\n"
        "call _ShouldSuppressGameCursor\n"
        "pop edx\n"
        "pop ecx\n"
        "test eax, eax\n"
        "jz 1f\n"
        "mov eax, dword ptr [esp + 4]\n"
        "push ecx\n"
        "push edx\n"
        "push eax\n"
        "call _SetCursorCenter\n"
        "add esp, 4\n"
        "pop edx\n"
        "pop ecx\n"
        "ret 4\n"
        "1:\n"
        "mov eax, dword ptr [_g_Orig_DP_GetCursorPos]\n"
        "test eax, eax\n"
        "jz 2f\n"
        "jmp eax\n"
        "2:\n"
        "xor eax, eax\n"
        "ret 4\n"
        ".att_syntax\n"
    );
}

extern "C" __attribute__((naked)) SHORT WINAPI Hook_DP_GetAsyncKeyState(int vKey) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "mov eax, dword ptr [esp + 4]\n"
        "push ecx\n"
        "push edx\n"
        "push eax\n"
        "call _ShouldSuppressGameKey\n"
        "add esp, 4\n"
        "pop edx\n"
        "pop ecx\n"
        "test eax, eax\n"
        "jz 1f\n"
        "xor eax, eax\n"
        "ret 4\n"
        "1:\n"
        "mov eax, dword ptr [_g_Orig_DP_GetAsyncKeyState]\n"
        "test eax, eax\n"
        "jz 2f\n"
        "jmp eax\n"
        "2:\n"
        "xor eax, eax\n"
        "ret 4\n"
        ".att_syntax\n"
    );
}

// -----------------------------------------------------------------------------
// Delta Time Tracking
// -----------------------------------------------------------------------------
static float GetDeltaTime(void) {
    static LARGE_INTEGER freq;
    static LARGE_INTEGER lastTime;
    static int initialized = 0;

    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&lastTime);
        initialized = 1;
        return 0.016667f;
    }

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    float dt = (float)(currentTime.QuadPart - lastTime.QuadPart) / (float)freq.QuadPart;
    lastTime = currentTime;

    if (dt <= 0.0f || dt > 0.1f) dt = 0.016667f;
    return dt;
}

// -----------------------------------------------------------------------------
// Dynamic XInput Gamepad Support
// -----------------------------------------------------------------------------
typedef struct {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD_S;

typedef struct {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_S Gamepad;
} XINPUT_STATE_S;

typedef DWORD (WINAPI *XInputGetStateFn)(DWORD dwUserIndex, XINPUT_STATE_S* pState);
static XInputGetStateFn g_XInputGetState = NULL;
static int g_XInputLoaded = 0;
static int g_GamepadConnected = 0;
static XINPUT_GAMEPAD_S g_LastGamepadState;

static void InitXInput(void) {
    if (g_XInputLoaded) return;
    HMODULE hXInput = LoadLibraryA("xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryA("xinput1_3.dll");
    if (!hXInput) hXInput = LoadLibraryA("xinput9_1_0.dll");
    if (hXInput) {
        g_XInputGetState = (XInputGetStateFn)GetProcAddress(hXInput, "XInputGetState");
    }
    g_XInputLoaded = 1;
}

static void ProcessGamepadInput(float dt) {
    if (!g_XInputGetState) return;
    XINPUT_STATE_S state;
    memset(&state, 0, sizeof(state));
    if (g_XInputGetState(0, &state) != 0) {
        g_GamepadConnected = 0;
        return;
    }
    g_GamepadConnected = 1;
    g_LastGamepadState = state.Gamepad;

    SHORT lx = state.Gamepad.sThumbLX;
    SHORT ly = state.Gamepad.sThumbLY;
    SHORT rx = state.Gamepad.sThumbRX;
    SHORT ry = state.Gamepad.sThumbRY;

    const SHORT deadzone = 7849;
    float normLX = 0.0f, normLY = 0.0f;
    float normRX = 0.0f, normRY = 0.0f;

    if (FastAbs((float)lx) > deadzone) normLX = (float)lx / 32767.0f;
    if (FastAbs((float)ly) > deadzone) normLY = (float)ly / 32767.0f;
    if (FastAbs((float)rx) > deadzone) normRX = (float)rx / 32767.0f;
    if (FastAbs((float)ry) > deadzone) normRY = (float)ry / 32767.0f;

    if (normRX != 0.0f || normRY != 0.0f) {
        float rotSpeed = 2.2f;
        float yawDelta = normRX * rotSpeed * dt * (g_InvertYaw ? -1.0f : 1.0f);
        g_CamYaw -= yawDelta;

        float pDelta = normRY * rotSpeed * dt * (g_InvertPitch ? -1.0f : 1.0f);
        g_CamPitch += pDelta;
        const float maxPitch = DEG2RAD(89.0f);
        if (g_CamPitch > maxPitch) g_CamPitch = maxPitch;
        if (g_CamPitch < -maxPitch) g_CamPitch = -maxPitch;
    }

    if (normLX != 0.0f || normLY != 0.0f || state.Gamepad.bRightTrigger > 30 || state.Gamepad.bLeftTrigger > 30 || (state.Gamepad.wButtons & 0x0300)) {
        float speed = g_MoveSpeed;
        if (state.Gamepad.wButtons & 0x0040) speed *= g_FastMultiplier;

        float cp = FastCos(g_CamPitch);
        float sp = FastSin(g_CamPitch);
        float cy = FastCos(g_CamYaw);
        float sy = FastSin(g_CamYaw);

        Vec3 fwd = { sy * cp, sp, cy * cp };
        Vec3 right = { -cy, 0.0f, sy };
        Vec3 upWorld = { 0.0f, 1.0f, 0.0f };

        float distFwd = normLY * speed * dt;
        float distRight = normLX * speed * dt;
        g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(fwd, distFwd));
        g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(right, distRight));

        float upDelta = 0.0f;
        if (state.Gamepad.bRightTrigger > 30) upDelta += ((float)state.Gamepad.bRightTrigger / 255.0f) * speed * dt;
        if (state.Gamepad.bLeftTrigger > 30)  upDelta -= ((float)state.Gamepad.bLeftTrigger / 255.0f) * speed * dt;
        if (state.Gamepad.wButtons & 0x0200)   upDelta += speed * dt;
        if (state.Gamepad.wButtons & 0x0100)   upDelta -= speed * dt;
        g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(upWorld, upDelta));
    }
}

// -----------------------------------------------------------------------------
// Configuration (FreeCam.ini)
// -----------------------------------------------------------------------------
static void GetIniPath(char* outPath, int maxLen) {
    GetModuleFileNameA(NULL, outPath, maxLen);
    char* p = strrchr(outPath, '\\');
    if (p) *(p + 1) = '\0';
    strncat(outPath, "scripts\\FreeCam.ini", maxLen - strlen(outPath) - 1);
}

static int ParseHexOrDec(const char* s, int defVal) {
    if (!s || !*s) return defVal;
    while (*s == ' ' || *s == '\t') s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
        s += 2;
        int val = 0;
        int count = 0;
        while (*s) {
            char c = *s++;
            int digit = -1;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else break;
            val = (val << 4) | digit;
            count++;
        }
        return count > 0 ? (val * sign) : defVal;
    }
    int val = 0;
    int count = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s++ - '0');
        count++;
    }
    return count > 0 ? (val * sign) : defVal;
}

static int ReadIniInt(const char* section, const char* key, int defVal, const char* iniPath) {
    char buf[64];
    if (GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), iniPath) > 0) {
        return ParseHexOrDec(buf, defVal);
    }
    return defVal;
}

static void LoadConfig(void) {
    char iniPath[MAX_PATH];
    GetIniPath(iniPath, MAX_PATH);
    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) return;

    g_MenuKey = ReadIniInt("General", "ToggleKey", g_MenuKey, iniPath);
    g_FreeCamKey = ReadIniInt("General", "FreeCamKey", g_FreeCamKey, iniPath);
    g_HudHideKey = ReadIniInt("General", "HudHideKey", g_HudHideKey, iniPath);
    g_FreezePlayer = ReadIniInt("General", "FreezePlayer", g_FreezePlayer, iniPath);
    int startHidden = ReadIniInt("HUD", "StartHidden", 0, iniPath);
    if (startHidden) g_HUDHidden = 1;

    char buf[64];
    if (GetPrivateProfileStringA("Camera", "MoveSpeed", "", buf, sizeof(buf), iniPath) > 0) {
        g_MoveSpeed = (float)atof(buf);
    }
    if (GetPrivateProfileStringA("Camera", "FastMultiplier", "", buf, sizeof(buf), iniPath) > 0) {
        g_FastMultiplier = (float)atof(buf);
    }
    if (GetPrivateProfileStringA("Camera", "SlowMultiplier", "", buf, sizeof(buf), iniPath) > 0) {
        g_SlowMultiplier = (float)atof(buf);
    }
    if (GetPrivateProfileStringA("Camera", "MouseSensitivity", "", buf, sizeof(buf), iniPath) > 0) {
        g_MouseSensitivity = (float)atof(buf);
    }
    g_InvertPitch = ReadIniInt("Camera", "InvertPitch", g_InvertPitch, iniPath);
    g_InvertYaw   = ReadIniInt("Camera", "InvertYaw",   g_InvertYaw,   iniPath);
    if (GetPrivateProfileStringA("Camera", "DefaultFOV", "", buf, sizeof(buf), iniPath) > 0) {
        float fov = (float)atof(buf);
        if (fov >= 30.0f && fov <= 120.0f) {
            g_TargetFOV_Deg = fov;
            g_CustomFOVEnabled = 1;
        }
    }
}

static void SaveConfig(void) {
    char iniPath[MAX_PATH];
    GetIniPath(iniPath, MAX_PATH);

    char buf[64];
    snprintf(buf, sizeof(buf), "0x%02X", g_MenuKey);
    WritePrivateProfileStringA("General", "ToggleKey", buf, iniPath);

    snprintf(buf, sizeof(buf), "0x%02X", g_FreeCamKey);
    WritePrivateProfileStringA("General", "FreeCamKey", buf, iniPath);

    snprintf(buf, sizeof(buf), "0x%02X", g_HudHideKey);
    WritePrivateProfileStringA("General", "HudHideKey", buf, iniPath);

    snprintf(buf, sizeof(buf), "%d", g_FreezePlayer);
    WritePrivateProfileStringA("General", "FreezePlayer", buf, iniPath);

    snprintf(buf, sizeof(buf), "%.2f", g_MoveSpeed);
    WritePrivateProfileStringA("Camera", "MoveSpeed", buf, iniPath);

    snprintf(buf, sizeof(buf), "%.2f", g_FastMultiplier);
    WritePrivateProfileStringA("Camera", "FastMultiplier", buf, iniPath);

    snprintf(buf, sizeof(buf), "%.2f", g_SlowMultiplier);
    WritePrivateProfileStringA("Camera", "SlowMultiplier", buf, iniPath);

    snprintf(buf, sizeof(buf), "%.4f", g_MouseSensitivity);
    WritePrivateProfileStringA("Camera", "MouseSensitivity", buf, iniPath);

    snprintf(buf, sizeof(buf), "%d", g_InvertPitch);
    WritePrivateProfileStringA("Camera", "InvertPitch", buf, iniPath);

    snprintf(buf, sizeof(buf), "%d", g_InvertYaw);
    WritePrivateProfileStringA("Camera", "InvertYaw", buf, iniPath);

    snprintf(buf, sizeof(buf), "%.1f", g_TargetFOV_Deg);
    WritePrivateProfileStringA("Camera", "DefaultFOV", buf, iniPath);

    snprintf(buf, sizeof(buf), "%d", g_HUDHidden);
    WritePrivateProfileStringA("HUD", "StartHidden", buf, iniPath);

    snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Saved editable values to scripts\\FreeCam.ini.");
}

// -----------------------------------------------------------------------------
// Safe 2D HUD Suppression
// -----------------------------------------------------------------------------
static void ApplyHUDState(int hide) {
    volatile uint8_t* pHudFlag = (volatile uint8_t*)ADDR_HUD_ENABLE_FLAG;
    if (pHudFlag) *pHudFlag = hide ? 0 : 1;

    volatile uint8_t* pRadarFlag = (volatile uint8_t*)ADDR_RADAR_FLAG;
    if (pRadarFlag) *pRadarFlag = hide ? 0 : 1;

    DWORD oldProtect;
    unsigned char* pJe = (unsigned char*)ADDR_HUD_JE_SITE;
    if (VirtualProtect(pJe, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        if (hide) {
            pJe[0] = 0xE9; // jmp rel32 -> 0x00602AF7
            pJe[1] = 0x40;
            pJe[2] = 0x2C;
            pJe[3] = 0x00;
            pJe[4] = 0x00;
            pJe[5] = 0x90;
        } else {
            pJe[0] = 0x0F; // je rel32 -> 0x00602AF7
            pJe[1] = 0x84;
            pJe[2] = 0x3F;
            pJe[3] = 0x2C;
            pJe[4] = 0x00;
            pJe[5] = 0x00;
        }
        VirtualProtect(pJe, 6, oldProtect, &oldProtect);
    }
}

// -----------------------------------------------------------------------------
// York Teleportation & Player Resolution
// -----------------------------------------------------------------------------
typedef void* (__thiscall *ResolveActorFn)(void* pActorMgr, uint32_t handle);

static void* GetPlayerActor(void) {
    if (IsBadReadPtr((const void*)ADDR_ACTOR_MGR, sizeof(void*)) ||
        IsBadReadPtr((const void*)ADDR_PLAYER_HANDLE, sizeof(uint32_t)) ||
        IsBadReadPtr((const void*)ADDR_RESOLVE_ACTOR, 16)) {
        return NULL;
    }
    const unsigned char* fnBytes = (const unsigned char*)ADDR_RESOLVE_ACTOR;
    if (fnBytes[0] == 0x00 && fnBytes[1] == 0x00) return NULL;

    void** ppActorMgr = (void**)ADDR_ACTOR_MGR;
    uint32_t* pPlayerHandle = (uint32_t*)ADDR_PLAYER_HANDLE;
    if (!ppActorMgr || !(*ppActorMgr) || !pPlayerHandle || !(*pPlayerHandle)) return NULL;
    if (IsBadReadPtr(*ppActorMgr, 64)) return NULL;

    ResolveActorFn ResolveActor = (ResolveActorFn)ADDR_RESOLVE_ACTOR;
    return ResolveActor(*ppActorMgr, *pPlayerHandle);
}

static int TeleportYorkToCamera(void) {
    void* playerActor = GetPlayerActor();
    if (!playerActor) return 0;

    float* pPos = (float*)((char*)playerActor + 0x58);
    pPos[0] = g_CamPos.x;
    pPos[1] = g_CamPos.y;
    pPos[2] = g_CamPos.z;

    float* pBoxPos = (float*)((char*)playerActor + 0x6C);
    pBoxPos[0] = g_CamPos.x;
    pBoxPos[1] = g_CamPos.y;
    pBoxPos[2] = g_CamPos.z;
    return 1;
}

static int ResetCameraToPlayer(void) {
    void* playerActor = GetPlayerActor();
    if (!playerActor) return 0;

    float* pPos = (float*)((char*)playerActor + 0x58);
    g_CamPos.x = pPos[0];
    g_CamPos.y = pPos[1] + 1.8f;
    g_CamPos.z = pPos[2] - 2.5f;
    return 1;
}

// -----------------------------------------------------------------------------
// Camera Hook Worker & Assembly Thunk
// -----------------------------------------------------------------------------
extern "C" void __cdecl CameraHookWorker(void* pCamCtx) {
    if (!pCamCtx) return;

    float dt = GetDeltaTime();
    (void)GetPlayerActor();

    float* pEye = (float*)((char*)pCamCtx + 4);
    float* pTarget = (float*)((char*)pCamCtx + 20);
    float* pUp = (float*)((char*)pCamCtx + 36);
    float* pFOV = (float*)((char*)pCamCtx + 84);

    if (g_FreeCamActive) {
        if (!g_FreeCamInitDone) {
            g_CamPos.x = pEye[0];
            g_CamPos.y = pEye[1];
            g_CamPos.z = pEye[2];

            float dx = pTarget[0] - pEye[0];
            float dy = pTarget[1] - pEye[1];
            float dz = pTarget[2] - pEye[2];
            float distXZ = FastSqrt(dx * dx + dz * dz);
            if (distXZ < 0.001f) distXZ = 0.001f;

            g_CamYaw = FastAtan2(dx, dz);
            g_CamPitch = FastAtan2(dy, distXZ);
            g_CamFOV_Rad = *pFOV;
            if (!g_CustomFOVEnabled) {
                g_TargetFOV_Deg = RAD2DEG(g_CamFOV_Rad);
            }
            g_FreeCamInitDone = 1;
            g_MouseLookWasActive = 0;
        }

        bool anyMenuOpen = g_MenuOpen || IsZachFixMenuOpen();
        int mouseLookActive = (g_GameHWnd && GetForegroundWindow() == g_GameHWnd &&
                               (!anyMenuOpen || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)));
        if (mouseLookActive) {
            RECT rc;
            if (GetClientRect(g_GameHWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
                POINT center = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
                ClientToScreen(g_GameHWnd, &center);
                if (!g_MouseLookWasActive) {
                    SetCursorPos(center.x, center.y);
                    g_MouseLookWasActive = 1;
                } else {
                    POINT cur;
                    if (GetCursorPos(&cur)) {
                        int dx = cur.x - center.x;
                        int dy = cur.y - center.y;
                        if (dx != 0 || dy != 0) {
                            float yawDelta = (float)dx * g_MouseSensitivity * (g_InvertYaw ? -1.0f : 1.0f);
                            g_CamYaw -= yawDelta;

                            float pitchDelta = (float)dy * g_MouseSensitivity * (g_InvertPitch ? -1.0f : 1.0f);
                            g_CamPitch -= pitchDelta;

                            const float maxPitch = DEG2RAD(89.0f);
                            if (g_CamPitch > maxPitch) g_CamPitch = maxPitch;
                            if (g_CamPitch < -maxPitch) g_CamPitch = -maxPitch;

                            SetCursorPos(center.x, center.y);
                        }
                    }
                }
            }
        } else {
            g_MouseLookWasActive = 0;
        }

        if (!anyMenuOpen) {
            ProcessGamepadInput(dt);
        }

        float currentSpeed = g_MoveSpeed;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) currentSpeed *= g_FastMultiplier;
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000)) currentSpeed *= g_SlowMultiplier;

        float moveDist = currentSpeed * dt;

        float cp = FastCos(g_CamPitch);
        float sp = FastSin(g_CamPitch);
        float cy = FastCos(g_CamYaw);
        float sy = FastSin(g_CamYaw);

        Vec3 fwd = { sy * cp, sp, cy * cp };
        Vec3 right = { -cy, 0.0f, sy };
        Vec3 upWorld = { 0.0f, 1.0f, 0.0f };

        if (!anyMenuOpen || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
            if (GetAsyncKeyState('W') & 0x8000) g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(fwd, moveDist));
            if (GetAsyncKeyState('S') & 0x8000) g_CamPos = Vec3_Sub(g_CamPos, Vec3_Mul(fwd, moveDist));
            if (GetAsyncKeyState('D') & 0x8000) g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(right, moveDist));
            if (GetAsyncKeyState('A') & 0x8000) g_CamPos = Vec3_Sub(g_CamPos, Vec3_Mul(right, moveDist));
            if ((GetAsyncKeyState('E') & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(upWorld, moveDist));
            if ((GetAsyncKeyState('Q') & 0x8000) || (GetAsyncKeyState('C') & 0x8000)) g_CamPos = Vec3_Sub(g_CamPos, Vec3_Mul(upWorld, moveDist));
        }

        g_CamTarget.x = g_CamPos.x + fwd.x * 100.0f;
        g_CamTarget.y = g_CamPos.y + fwd.y * 100.0f;
        g_CamTarget.z = g_CamPos.z + fwd.z * 100.0f;

        pEye[0] = g_CamPos.x;
        pEye[1] = g_CamPos.y;
        pEye[2] = g_CamPos.z;
        pEye[3] = 1.0f;

        pTarget[0] = g_CamTarget.x;
        pTarget[1] = g_CamTarget.y;
        pTarget[2] = g_CamTarget.z;
        pTarget[3] = 1.0f;

        pUp[0] = 0.0f;
        pUp[1] = 1.0f;
        pUp[2] = 0.0f;
        pUp[3] = 0.0f;

        if (g_CustomFOVEnabled) {
            *pFOV = DEG2RAD(g_TargetFOV_Deg);
        }
    } else {
        g_FreeCamInitDone = 0;
        g_MouseLookWasActive = 0;
        g_CamPos.x = pEye[0];
        g_CamPos.y = pEye[1];
        g_CamPos.z = pEye[2];
        g_CamTarget.x = pTarget[0];
        g_CamTarget.y = pTarget[1];
        g_CamTarget.z = pTarget[2];
        g_CamFOV_Rad = *pFOV;
    }
}

extern "C" __attribute__((naked)) void CameraHookAsm(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "push ecx\n"
        "call _CameraHookWorker\n"
        "pop ecx\n"
        "mov eax, 0x006B62E0\n"
        "jmp eax\n"
    );
}

extern "C" __attribute__((naked)) void RadarHookAsm(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "cmp dword ptr [_g_HUDHidden], 0\n"
        "jz 1f\n"
        "cmp byte ptr [0x0146FC38], 0\n"
        "jnz 1f\n"
        "cmp dword ptr [ecx + 0x14], 6\n"
        "je 1f\n"
        "ret\n"
        "1:\n"
        "sub esp, 0x10\n"
        "push ebx\n"
        "mov ebx, ecx\n"
        "mov eax, 0x006541B6\n"
        "jmp eax\n"
        ".att_syntax\n"
    );
}

// -----------------------------------------------------------------------------
// Dear ImGui Settings Window
// -----------------------------------------------------------------------------
static void DrawFreeCamSettingsWindow() {
    if (!ImGui::Begin("FreeCam Settings", &g_MenuOpen)) {
        ImGui::End();
        return;
    }

    ImGui::Text("FreeCam v1.1.0 (Official Dear ImGui C++)");

    if (ImGui::Button("Save to INI")) {
        SaveConfig();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save current FreeCam and HUD settings to scripts\\FreeCam.ini.");

    ImGui::SameLine();
    if (ImGui::Button("Reload INI")) {
        LoadConfig();
        ApplyHUDState(g_HUDHidden);
        snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Reloaded editable settings from scripts\\FreeCam.ini.");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reload settings from scripts\\FreeCam.ini.");

    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        ApplyHUDState(g_HUDHidden);
        snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Live settings applied. Save to INI to persist them.");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply live settings immediately.");

    ImGui::SameLine();
    if (ImGui::Button("Reset Defaults")) {
        g_MoveSpeed = 15.0f;
        g_FastMultiplier = 3.0f;
        g_SlowMultiplier = 0.25f;
        g_MouseSensitivity = 0.003f;
        g_InvertPitch = 0;
        g_InvertYaw = 0;
        g_TargetFOV_Deg = 65.0f;
        g_CustomFOVEnabled = 0;
        g_FreezePlayer = 1;
        snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Restored default settings (live). Save to INI to persist.");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restore default movement speed, sensitivity, and FOV.");

    ImGui::SameLine();
    ImGui::TextDisabled("F9 closes");

    ImGui::Text("%s", g_StatusMessage);
    ImGui::Separator();

    if (ImGui::BeginTabBar("FreeCamTabs")) {
        if (ImGui::BeginTabItem("Camera")) {
            ImGui::SeparatorText("Free Camera Flight");
            if (ImGui::Checkbox("Enable Free Camera (F5)", &g_FreeCamActive)) {
                snprintf(g_StatusMessage, sizeof(g_StatusMessage), g_FreeCamActive ? "Free Camera: enabled (live)." : "Free Camera: disabled (live).");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Detaches camera from York and enables 6-DOF flight.\nHotkey: F5");

            if (ImGui::Checkbox("Freeze Player (York) While Flying", &g_FreezePlayer)) {
                snprintf(g_StatusMessage, sizeof(g_StatusMessage), g_FreezePlayer ? "Freeze Player: enabled (live)." : "Freeze Player: disabled (live).");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Suppresses gameplay WASD/action inputs from moving York while FreeCam is active.");

            ImGui::SeparatorText("Movement & Speed");
            ImGui::SliderFloat("Flight Speed", &g_MoveSpeed, 1.0f, 100.0f, "%.1f u/s");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Base camera movement speed in world units per second.");

            ImGui::SliderFloat("Boost Multiplier (Shift / L3)", &g_FastMultiplier, 1.0f, 10.0f, "%.2fx");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Speed multiplier applied when holding Left Shift or Gamepad L3.");

            ImGui::SliderFloat("Slow Multiplier (Ctrl / Alt)", &g_SlowMultiplier, 0.05f, 1.0f, "%.2fx");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Precision speed multiplier applied when holding Ctrl or Alt.");

            ImGui::SeparatorText("Mouse Look & Orientation");
            ImGui::SliderFloat("Mouse Sensitivity", &g_MouseSensitivity, 0.0005f, 0.0200f, "%.4f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Radians per pixel of mouse movement during FreeCam look.");

            ImGui::Checkbox("Invert Vertical Look (Pitch)", &g_InvertPitch);
            ImGui::Checkbox("Invert Horizontal Look (Yaw)", &g_InvertYaw);

            ImGui::SeparatorText("Field of View & Actor Teleport");
            ImGui::Checkbox("Override Field of View", &g_CustomFOVEnabled);
            if (ImGui::SliderFloat("Field of View", &g_TargetFOV_Deg, 30.0f, 120.0f, "%.1f deg")) {
                g_CustomFOVEnabled = 1;
            }

            if (ImGui::Button("Teleport York to Camera")) {
                if (TeleportYorkToCamera()) {
                    snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Teleported York to current camera position.");
                } else {
                    snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Could not resolve York actor (ensure level is loaded).");
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Moves York's world position (+0x58) and collision center (+0x6C) to camera.");

            ImGui::SameLine();
            if (ImGui::Button("Snap Camera to York")) {
                if (ResetCameraToPlayer()) {
                    snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Snapped camera behind York.");
                } else {
                    snprintf(g_StatusMessage, sizeof(g_StatusMessage), "Could not resolve York actor.");
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Repositions FreeCam directly behind York.");

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("HUD")) {
            ImGui::SeparatorText("In-Game 2D HUD Visibility");
            if (ImGui::Checkbox("Hide In-Game 2D HUD (F4)", (int*)&g_HUDHidden)) {
                ApplyHUDState(g_HUDHidden);
                snprintf(g_StatusMessage, sizeof(g_StatusMessage), g_HUDHidden ? "2D HUD: hidden (live)." : "2D HUD: visible (live).");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggles 2D HUD (minimap, health, clock, ammo).\nHotkey: F4");

            ImGui::TextDisabled("Suppresses DP's 2D HUD pass (0x005FFE80) and in-game Radar pass (0x006541B0)\nwhile cleanly preserving pause-menu and overworld MapFix navigation.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Gamepad")) {
            ImGui::SeparatorText("XInput Gamepad Diagnostics");
            ImGui::Text("Controller 0: %s", g_GamepadConnected ? "Connected (XInput active)" : "Disconnected");
            if (g_GamepadConnected) {
                ImGui::Text("Left Stick (Fly):  X=%d  Y=%d", g_LastGamepadState.sThumbLX, g_LastGamepadState.sThumbLY);
                ImGui::Text("Right Stick (Look): X=%d  Y=%d", g_LastGamepadState.sThumbRX, g_LastGamepadState.sThumbRY);
                ImGui::Text("Triggers (Elevate): LT=%u  RT=%u", g_LastGamepadState.bLeftTrigger, g_LastGamepadState.bRightTrigger);
                ImGui::Text("Buttons: 0x%04X", g_LastGamepadState.wButtons);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Diagnostics")) {
            ImGui::SeparatorText("Live Engine Diagnostics");
            ImGui::Text("Camera Pos:    X=%.2f  Y=%.2f  Z=%.2f", g_CamPos.x, g_CamPos.y, g_CamPos.z);
            ImGui::Text("Camera Target: X=%.2f  Y=%.2f  Z=%.2f", g_CamTarget.x, g_CamTarget.y, g_CamTarget.z);
            ImGui::Text("Camera Pitch:  %.2f deg (%.3f rad)", RAD2DEG(g_CamPitch), g_CamPitch);
            ImGui::Text("Camera Yaw:    %.2f deg (%.3f rad)", RAD2DEG(g_CamYaw), g_CamYaw);
            ImGui::Text("Camera FOV:    %.2f deg", g_TargetFOV_Deg);
            ImGui::Text("York Actor:    %p", GetPlayerActor());
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("About")) {
            ImGui::SeparatorText("Deadly Premonition FreeCam Mod");
            ImGui::Text("Version: 1.1.0");
            ImGui::Text("UI Backend: Dear ImGui C++ Engine (Zero-CRT x86)");
            ImGui::TextDisabled("Free Camera, York Teleportation & HUD Suppression Plugin");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// -----------------------------------------------------------------------------
// D3D9 Hooks
// -----------------------------------------------------------------------------
static void UpdateHotkeys() {
    // F9 Menu Toggle
    if (GetAsyncKeyState(g_MenuKey) & 0x8000) {
        if (!g_MenuKeyLatched) {
            g_MenuKeyLatched = 1;
            g_MenuOpen = !g_MenuOpen;
            if (g_MenuOpen && g_GameHWnd) {
                POINT pt;
                if (GetCursorPos(&pt)) {
                    ScreenToClient(g_GameHWnd, &pt);
                    ImGui::GetIO().AddMousePosEvent((float)pt.x, (float)pt.y);
                }
            }
            ImGui::GetIO().MouseDrawCursor = g_MenuOpen;
        }
    } else {
        g_MenuKeyLatched = 0;
    }

    // F5 FreeCam Toggle
    if (GetAsyncKeyState(g_FreeCamKey) & 0x8000) {
        if (!g_FreeCamKeyLatched) {
            g_FreeCamKeyLatched = 1;
            g_FreeCamActive = !g_FreeCamActive;
            snprintf(g_StatusMessage, sizeof(g_StatusMessage), g_FreeCamActive ? "Free Camera: enabled (F5)." : "Free Camera: disabled (F5).");
        }
    } else {
        g_FreeCamKeyLatched = 0;
    }

    // F4 HUD Toggle
    if (GetAsyncKeyState(g_HudHideKey) & 0x8000) {
        if (!g_HudKeyLatched) {
            g_HudKeyLatched = 1;
            g_HUDHidden = !g_HUDHidden;
            ApplyHUDState(g_HUDHidden);
            snprintf(g_StatusMessage, sizeof(g_StatusMessage), g_HUDHidden ? "2D HUD: hidden (F4)." : "2D HUD: visible (F4).");
        }
    } else {
        g_HudKeyLatched = 0;
    }
}

static HRESULT WINAPI HookEndScene(IDirect3DDevice9* dev) {
    if (dev && g_MenuOpen) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawFreeCamSettingsWindow();

        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    return g_OrigEndScene(dev);
}

static HRESULT WINAPI HookPresent(IDirect3DDevice9* dev, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    if (dev) {
        UpdateHotkeys();
    }
    return g_OrigPresent(dev, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

static HRESULT WINAPI HookReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pParams) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_OrigReset(dev, pParams);
    if (SUCCEEDED(hr)) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return hr;
}

// -----------------------------------------------------------------------------
// Window Subclassing (WndProc)
// -----------------------------------------------------------------------------
LRESULT CALLBACK HookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_MenuOpen) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (wParam == (WPARAM)g_MenuKey || wParam == (WPARAM)g_FreeCamKey || wParam == (WPARAM)g_HudHideKey) {
                return 0;
            }
            if (g_MenuOpen) return 1;
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if (wParam == (WPARAM)g_MenuKey || wParam == (WPARAM)g_FreeCamKey || wParam == (WPARAM)g_HudHideKey) {
                return 0;
            }
            if (g_MenuOpen) return 1;
            break;
        }
        case WM_CHAR: {
            if (g_MenuOpen) return 1;
            break;
        }
        case WM_SETCURSOR: {
            if (g_MenuOpen) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                return TRUE;
            }
            break;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL: {
            if (g_MenuOpen) return 1;
            break;
        }
    }

    if (g_OrigWndProc) return CallWindowProcW(g_OrigWndProc, hWnd, uMsg, wParam, lParam);
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// -----------------------------------------------------------------------------
// Hook Installation Thread
// -----------------------------------------------------------------------------
static DWORD WINAPI HookThread(LPVOID lpParam) {
    (void)lpParam;

    IDirect3DDevice9** ppDevice = (IDirect3DDevice9**)ADDR_D3D9_DEVICE_PTR;
    while (!ppDevice || !(*ppDevice)) {
        Sleep(50);
    }
    IDirect3DDevice9* dev = *ppDevice;

    InitXInput();

    D3DDEVICE_CREATION_PARAMETERS cp;
    memset(&cp, 0, sizeof(cp));
    if (dev->GetCreationParameters(&cp) == D3D_OK && cp.hFocusWindow) {
        g_GameHWnd = cp.hFocusWindow;
        g_OrigWndProc = (WNDPROC)SetWindowLongPtrW(g_GameHWnd, GWLP_WNDPROC, (LONG_PTR)HookWndProc);
    }

    // Initialize Official Dear ImGui Context & Backends
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_GameHWnd);
    ImGui_ImplDX9_Init(dev);

    // Hook Reset (16), Present (17), EndScene (42)
    void** vtbl = *(void***)dev;
    DWORD oldProtect;
    if (VirtualProtect(&vtbl[16], sizeof(void*) * 27, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_OrigReset    = (D3D9ResetFn)vtbl[16];
        g_OrigPresent  = (D3D9PresentFn)vtbl[17];
        g_OrigEndScene = (D3D9EndSceneFn)vtbl[42];
        vtbl[16] = (void*)HookReset;
        vtbl[17] = (void*)HookPresent;
        vtbl[42] = (void*)HookEndScene;
        VirtualProtect(&vtbl[16], sizeof(void*) * 27, oldProtect, &oldProtect);
    }

    // Clear 0x01476A1E in case a previous session/build left this cinematic event flag set
    volatile uint8_t* pLegacySuppress = (volatile uint8_t*)0x01476A1E;
    if (pLegacySuppress && *pLegacySuppress == 1) {
        *pLegacySuppress = 0;
    }

    // Hook DP.exe IAT entries for cursor & key isolation with caller-preserving chaining
    if (VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        SetCursorPosFn cur = *(SetCursorPosFn*)ADDR_IAT_SETCURSORPOS;
        if (cur != Hook_DP_SetCursorPos) {
            g_Orig_DP_SetCursorPos = cur;
            *(SetCursorPosFn*)ADDR_IAT_SETCURSORPOS = Hook_DP_SetCursorPos;
        }
        VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
    }

    if (VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        GetCursorPosFn cur = *(GetCursorPosFn*)ADDR_IAT_GETCURSORPOS;
        if (cur != Hook_DP_GetCursorPos) {
            g_Orig_DP_GetCursorPos = cur;
            *(GetCursorPosFn*)ADDR_IAT_GETCURSORPOS = Hook_DP_GetCursorPos;
        }
        VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
    }

    if (VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        GetAsyncKeyStateFn cur = *(GetAsyncKeyStateFn*)ADDR_IAT_GETASYNCKEYSTATE;
        if (cur != Hook_DP_GetAsyncKeyState) {
            g_Orig_DP_GetAsyncKeyState = cur;
            *(GetAsyncKeyStateFn*)ADDR_IAT_GETASYNCKEYSTATE = Hook_DP_GetAsyncKeyState;
        }
        VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), oldProtect, &oldProtect);
    }

    // Hook Camera CalcViewProj call at 0x00401BD1
    unsigned char* pHookSite = (unsigned char*)ADDR_CAMERA_HOOK_SITE;
    if (VirtualProtect(pHookSite, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pHookSite = 0xE8;
        int32_t rel = (int32_t)((uintptr_t)CameraHookAsm - ((uintptr_t)pHookSite + 5));
        *(int32_t*)(pHookSite + 1) = rel;
        VirtualProtect(pHookSite, 5, oldProtect, &oldProtect);
    }

    // Hook Radar / Minimap Render routine at 0x006541B0
    unsigned char* pRadarHookSite = (unsigned char*)ADDR_RADAR_DRAW_FUNC;
    if (VirtualProtect(pRadarHookSite, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pRadarHookSite = 0xE9;
        int32_t rel = (int32_t)((uintptr_t)RadarHookAsm - ((uintptr_t)pRadarHookSite + 5));
        *(int32_t*)(pRadarHookSite + 1) = rel;
        pRadarHookSite[5] = 0x90;
        VirtualProtect(pRadarHookSite, 6, oldProtect, &oldProtect);
    }

    if (g_HUDHidden) {
        ApplyHUDState(1);
    }

    return 0;
}

// -----------------------------------------------------------------------------
// DLL Entry Point
// -----------------------------------------------------------------------------
extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        LoadConfig();
        CreateThread(NULL, 0, HookThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        ApplyHUDState(0);
        DWORD oldProtect;
        unsigned char* pRadarHookSite = (unsigned char*)ADDR_RADAR_DRAW_FUNC;
        if (VirtualProtect(pRadarHookSite, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            static const unsigned char origRadar[6] = { 0x83, 0xEC, 0x10, 0x53, 0x8B, 0xD9 };
            memcpy(pRadarHookSite, origRadar, 6);
            VirtualProtect(pRadarHookSite, 6, oldProtect, &oldProtect);
        }
        if (g_Orig_DP_SetCursorPos) {
            if (VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *(SetCursorPosFn*)ADDR_IAT_SETCURSORPOS = g_Orig_DP_SetCursorPos;
                VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        if (g_Orig_DP_GetCursorPos) {
            if (VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *(GetCursorPosFn*)ADDR_IAT_GETCURSORPOS = g_Orig_DP_GetCursorPos;
                VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        if (g_Orig_DP_GetAsyncKeyState) {
            if (VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *(GetAsyncKeyStateFn*)ADDR_IAT_GETASYNCKEYSTATE = g_Orig_DP_GetAsyncKeyState;
                VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (g_GameHWnd && g_OrigWndProc) {
            SetWindowLongPtrW(g_GameHWnd, GWLP_WNDPROC, (LONG_PTR)g_OrigWndProc);
        }
    }
    return TRUE;
}
