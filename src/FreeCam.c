// ==============================================================================
// Deadly Premonition: The Director's Cut
// FreeCam & HUD Hide ASI Mod (FreeCam.asi)
//
// Features & Architecture:
// 1. Dear ImGui (StyleColorsDark) In-Game Settings UI (F9 by default)
//    - Modeled directly on ZachFix's ui_settings.cpp (DrawSettingsWindow,
//      SeparatorText, SliderFloat with left track + centered value + right label,
//      Checkbox with blue frame + vector checkmark, compact TabBar, tooltips,
//      and live Diagnostics / Gamepad / Camera / HUD / About tabs)
// 2. 6-DOF Free Camera flight with position, target, and FOV control (F5)
//    - Accounts for Deadly Premonition's negated projection matrix X-scale
//      (0x006B62E0 called at 0x00401BD1 with param_2=1 negates proj._11):
//      * Screen-right basis vector is (-cos(yaw), 0, sin(yaw)) so D moves right
//        and A moves left naturally.
//      * Horizontal look decrements yaw on rightward mouse/stick input so moving
//        the mouse right turns right and moving left turns left naturally.
// 3. Reliable York Player Teleportation to camera position
// 4. Safe York player movement and action freeze during FreeCam
// 5. Clean 2D HUD toggle (F4) without pause-menu map corruption
// 6. Full compatibility with ZachFix.asi (F10) and MapFix.asi (M)
// ==============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Math & Helper Functions (Pure C & x87 inline assembly)
// -----------------------------------------------------------------------------
#define PI 3.14159265358979323846f
#define DEG2RAD(d) ((d) * (PI / 180.0f))
#define RAD2DEG(r) ((r) * (180.0f / PI))

static inline float FastSin(float x) {
    float r;
    __asm__ volatile ("fsin" : "=t"(r) : "0"(x));
    return r;
}

static inline float FastCos(float x) {
    float r;
    __asm__ volatile ("fcos" : "=t"(r) : "0"(x));
    return r;
}

static inline float FastSqrt(float x) {
    float r;
    __asm__ volatile ("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

static inline float FastAtan2(float y, float x) {
    float r;
    __asm__ volatile ("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

static inline float FastAbs(float x) {
    return (x < 0.0f) ? -x : x;
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

#define MemSet memset
#define MemCpy memcpy
#define StrLen (int)strlen

static float ParseFloat(const char* s, float def) {
    if (!s || !*s) return def;
    while (*s == ' ' || *s == '\t') s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    float val = 0.0f;
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0f + (float)(*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        float factor = 0.1f;
        while (*s >= '0' && *s <= '9') {
            val += (float)(*s - '0') * factor;
            factor *= 0.1f;
            s++;
        }
    }
    return val * (float)sign;
}

static void FormatFloat(char* out, int outSize, float val, int decimals) {
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
    for (int i = intPos - 1; i >= 0; i--) {
        temp[pos++] = intBuf[i];
    }

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

// -----------------------------------------------------------------------------
// Vector3 Struct & Ops
// -----------------------------------------------------------------------------
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
#define ADDR_CARNAVI_DRAW_FUNC     0x005F5DF0  // Vehicle Radar / Car Navi Render routine
#define ADDR_RADAR_FLAG            0x01476A1C  // Global Radar display flag byte (0x01476978 + 0xA4)
#define ADDR_MENU_OPEN_FLAG        0x0146FC38  // Global Pause / Full-screen Map open byte (1=open, 0=in-game)
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

// Runtime state
static volatile int g_MenuOpen        = 0;
static volatile int g_WindowCollapsed = 0;
static volatile int g_FreeCamActive   = 0;
volatile int        g_HUDHidden       = 0;
static volatile int g_FreeCamInitDone = 0;

static Vec3  g_CamPos       = { 0.0f, 0.0f, 0.0f };
static Vec3  g_CamTarget    = { 0.0f, 0.0f, 1.0f };
static float g_CamPitch     = 0.0f; // radians
static float g_CamYaw       = 0.0f; // radians
static float g_CamFOV_Rad   = 1.134464f; // ~65 deg

static int   g_SelectedTab      = 0; // 0: Camera, 1: HUD, 2: Gamepad, 3: Diagnostics, 4: About
static int   g_MenuX            = 50;
static int   g_MenuY            = 50;
static int   g_MenuW            = 620;
static int   g_MenuH            = 520;
static int   g_DraggingWindow   = 0;
static int   g_ResizingWindow   = 0;
static int   g_DragOffsetX      = 0;
static int   g_DragOffsetY      = 0;

static int   g_MouseX           = 0;
static int   g_MouseY           = 0;
static int   g_MouseDown        = 0;
static int   g_MouseClicked     = 0;
static int   g_FrameClicked     = 0;
static int   g_PrevLButtonDown  = 0;
static int   g_RightDown        = 0;
static int   g_ActiveSliderId   = -1;
static int   g_UiUpdatedFrame   = 0;
static int   g_UiRenderedFrame  = 0;
static int   g_MouseLookWasActive = 0;

static int   g_MenuKeyLatched    = 0;
static int   g_FreeCamKeyLatched = 0;
static int   g_HudKeyLatched     = 0;

static const char* g_HoveredTooltip = NULL;
static char  g_StatusMessage[160] = "Ready. Press F9 to toggle FreeCam Settings, F5 for FreeCam, F4 for HUD.";

static HWND  g_GameHWnd         = NULL;
static WNDPROC g_OrigWndProc    = NULL;

// -----------------------------------------------------------------------------
// Exact Dear ImGui StyleColorsDark() Palette (32-bit D3DCOLOR ARGB)
// Matches ImGui::StyleColorsDark() used in ZachFix (ui_settings.cpp:2425)
// -----------------------------------------------------------------------------
#define IM_COL32(r,g,b,a)         (((DWORD)(a)<<24)|((DWORD)(r)<<16)|((DWORD)(g)<<8)|((DWORD)(b)))

#define IM_COL_WINDOW_BG          IM_COL32(15,  15,  15,  240) // ImVec4(0.06f, 0.06f, 0.06f, 0.94f)
#define IM_COL_CHILD_BG           IM_COL32(20,  20,  22,  180)
#define IM_COL_POPUP_BG           IM_COL32(20,  20,  20,  240) // ImVec4(0.08f, 0.08f, 0.08f, 0.94f)
#define IM_COL_BORDER             IM_COL32(110, 110, 128, 128) // ImVec4(0.43f, 0.43f, 0.50f, 0.50f)
#define IM_COL_TITLE_BG           IM_COL32(10,  10,  10,  255) // ImVec4(0.04f, 0.04f, 0.04f, 1.00f)
#define IM_COL_TITLE_BG_ACTIVE    IM_COL32(41,  74,  122, 255) // ImVec4(0.16f, 0.29f, 0.48f, 1.00f)
#define IM_COL_FRAME_BG           IM_COL32(41,  74,  122, 138) // ImVec4(0.16f, 0.29f, 0.48f, 0.54f)
#define IM_COL_FRAME_HOVER        IM_COL32(66,  150, 250, 102) // ImVec4(0.26f, 0.59f, 0.98f, 0.40f)
#define IM_COL_FRAME_ACTIVE       IM_COL32(66,  150, 250, 171) // ImVec4(0.26f, 0.59f, 0.98f, 0.67f)
#define IM_COL_CHECKMARK          IM_COL32(66,  150, 250, 255) // ImVec4(0.26f, 0.59f, 0.98f, 1.00f)
#define IM_COL_SLIDER_GRAB        IM_COL32(61,  133, 224, 255) // ImVec4(0.24f, 0.52f, 0.88f, 1.00f)
#define IM_COL_SLIDER_GRAB_ACTIVE IM_COL32(66,  150, 250, 255) // ImVec4(0.26f, 0.59f, 0.98f, 1.00f)
#define IM_COL_BUTTON             IM_COL32(66,  150, 250, 102) // ImVec4(0.26f, 0.59f, 0.98f, 0.40f)
#define IM_COL_BUTTON_HOVER       IM_COL32(66,  150, 250, 255) // ImVec4(0.26f, 0.59f, 0.98f, 1.00f)
#define IM_COL_BUTTON_ACTIVE      IM_COL32(15,  135, 250, 255) // ImVec4(0.06f, 0.53f, 0.98f, 1.00f)
#define IM_COL_HEADER             IM_COL32(66,  150, 250, 79)  // ImVec4(0.26f, 0.59f, 0.98f, 0.31f)
#define IM_COL_HEADER_HOVER       IM_COL32(66,  150, 250, 204) // ImVec4(0.26f, 0.59f, 0.98f, 0.80f)
#define IM_COL_HEADER_ACTIVE      IM_COL32(66,  150, 250, 255) // ImVec4(0.26f, 0.59f, 0.98f, 1.00f)
#define IM_COL_TAB                IM_COL32(46,  89,  148, 220) // ImVec4(0.18f, 0.35f, 0.58f, 0.86f)
#define IM_COL_TAB_HOVER          IM_COL32(66,  150, 250, 204) // ImVec4(0.26f, 0.59f, 0.98f, 0.80f)
#define IM_COL_TAB_ACTIVE         IM_COL32(51,  105, 173, 255) // ImVec4(0.20f, 0.41f, 0.68f, 1.00f)
#define IM_COL_SEPARATOR          IM_COL32(110, 110, 128, 128) // ImVec4(0.43f, 0.43f, 0.50f, 0.50f)
#define IM_COL_RESIZE_GRIP        IM_COL32(66,  150, 250, 51)  // ImVec4(0.26f, 0.59f, 0.98f, 0.20f)
#define IM_COL_RESIZE_GRIP_HOVER  IM_COL32(66,  150, 250, 171) // ImVec4(0.26f, 0.59f, 0.98f, 0.67f)
#define IM_COL_TEXT               IM_COL32(255, 255, 255, 255) // ImVec4(1.00f, 1.00f, 1.00f, 1.00f)
#define IM_COL_TEXT_DISABLED      IM_COL32(128, 128, 128, 255) // ImVec4(0.50f, 0.50f, 0.50f, 1.00f)
#define IM_COL_STATUS_TEXT        IM_COL32(190, 235, 190, 255)

// -----------------------------------------------------------------------------
// IAT Hook Signatures & Handlers
// -----------------------------------------------------------------------------
typedef BOOL  (WINAPI *SetCursorPosFn)(int x, int y);
typedef BOOL  (WINAPI *GetCursorPosFn)(LPPOINT lpPoint);
typedef SHORT (WINAPI *GetAsyncKeyStateFn)(int vKey);

static SetCursorPosFn     g_Orig_DP_SetCursorPos     = NULL;
static GetCursorPosFn     g_Orig_DP_GetCursorPos     = NULL;
static GetAsyncKeyStateFn g_Orig_DP_GetAsyncKeyState = NULL;

static BOOL WINAPI Hook_DP_SetCursorPos(int x, int y) {
    if (g_MenuOpen && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        // Suppress DP from recentering cursor while GUI is open so user can click
        return TRUE;
    }
    if (g_Orig_DP_SetCursorPos) {
        return g_Orig_DP_SetCursorPos(x, y);
    }
    return SetCursorPos(x, y);
}

static BOOL WINAPI Hook_DP_GetCursorPos(LPPOINT lpPoint) {
    if (g_MenuOpen && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000) && lpPoint) {
        // Spoof client center to DP.exe while menu is open (matches ZachFix HookGetCursorPos)
        // so the game's camera doesn't spin as the user moves the cursor across the menu.
        if (g_GameHWnd) {
            RECT rc;
            if (GetClientRect(g_GameHWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
                POINT center = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
                ClientToScreen(g_GameHWnd, &center);
                *lpPoint = center;
                return TRUE;
            }
        }
    }
    if (g_Orig_DP_GetCursorPos) {
        return g_Orig_DP_GetCursorPos(lpPoint);
    }
    return GetCursorPos(lpPoint);
}

static SHORT WINAPI Hook_DP_GetAsyncKeyState(int vKey) {
    if (g_MenuOpen && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        // Suppress all game key polling while menu is open (matches ZachFix HookGetAsyncKeyState)
        return 0;
    }
    if (g_FreeCamActive && g_FreezePlayer) {
        if (vKey == 'W' || vKey == 'S' || vKey == 'A' || vKey == 'D' ||
            vKey == 'Q' || vKey == 'E' || vKey == 'C' || vKey == 'R' ||
            vKey == 'F' || vKey == 'Z' || vKey == 'X' ||
            vKey == VK_UP || vKey == VK_DOWN || vKey == VK_LEFT || vKey == VK_RIGHT ||
            vKey == VK_SHIFT || vKey == VK_CONTROL || vKey == VK_SPACE ||
            vKey == VK_LBUTTON || vKey == VK_RBUTTON) {
            return 0;
        }
    }
    if (g_Orig_DP_GetAsyncKeyState) {
        return g_Orig_DP_GetAsyncKeyState(vKey);
    }
    return GetAsyncKeyState(vKey);
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
    MemSet(&state, 0, sizeof(state));
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

    // In Deadly Premonition, 0x006B62E0 (called with param_2=1 at 0x00401BD1)
    // negates proj._11, mirroring clip-space X relative to D3DXMatrixLookAtLH.
    // Consequently:
    // - Decreasing g_CamYaw rotates fwd=(sy*cp, sp, cy*cp) toward (-cy, 0, sy),
    //   which projects to +X on screen (RIGHT).
    // - Screen-space RIGHT in world coordinates is (-cy, 0, sy).
    if (normRX != 0.0f || normRY != 0.0f) {
        float rotSpeed = 2.2f; // radians/sec
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
        if (state.Gamepad.wButtons & 0x0040) speed *= g_FastMultiplier; // L3

        float cp = FastCos(g_CamPitch);
        float sp = FastSin(g_CamPitch);
        float cy = FastCos(g_CamYaw);
        float sy = FastSin(g_CamYaw);

        Vec3 fwd = { sy * cp, sp, cy * cp };
        Vec3 right = { -cy, 0.0f, sy }; // Screen-right vector accounting for negated proj._11
        Vec3 upWorld = { 0.0f, 1.0f, 0.0f };

        float distFwd = normLY * speed * dt;
        float distRight = normLX * speed * dt;
        g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(fwd, distFwd));
        g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(right, distRight));

        float upDelta = 0.0f;
        if (state.Gamepad.bRightTrigger > 30) upDelta += ((float)state.Gamepad.bRightTrigger / 255.0f);
        if (state.Gamepad.bLeftTrigger > 30) upDelta -= ((float)state.Gamepad.bLeftTrigger / 255.0f);
        if (state.Gamepad.wButtons & 0x0200) upDelta += 1.0f; // RB
        if (state.Gamepad.wButtons & 0x0100) upDelta -= 1.0f; // LB

        if (upDelta != 0.0f) {
            g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(upWorld, upDelta * speed * dt));
        }
    }
}

// -----------------------------------------------------------------------------
// Timing for smooth delta time
// -----------------------------------------------------------------------------
static LARGE_INTEGER g_PerfFreq;
static LARGE_INTEGER g_LastPerfCount;

static float GetDeltaTime(void) {
    if (g_PerfFreq.QuadPart == 0) {
        QueryPerformanceFrequency(&g_PerfFreq);
        QueryPerformanceCounter(&g_LastPerfCount);
        return 0.016667f;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_LastPerfCount.QuadPart) / (float)g_PerfFreq.QuadPart;
    g_LastPerfCount = now;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.016667f;
    return dt;
}

// -----------------------------------------------------------------------------
// INI Configuration Reader & Writer
// -----------------------------------------------------------------------------
static void GetIniPath(char* outPath, int maxLen) {
    GetModuleFileNameA(NULL, outPath, maxLen);
    char* lastSlash = NULL;
    for (char* p = outPath; *p; ++p) {
        if (*p == '\\' || *p == '/') lastSlash = p;
    }
    if (lastSlash) *(lastSlash + 1) = '\0';
    lstrcatA(outPath, "scripts\\FreeCam.ini");
}

static void ApplyHUDState(int hide);

static void LoadConfig(void) {
    char iniPath[MAX_PATH];
    GetIniPath(iniPath, MAX_PATH);

    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) return;

    g_MenuKey = GetPrivateProfileIntA("General", "ToggleKey", g_MenuKey, iniPath);
    g_FreeCamKey = GetPrivateProfileIntA("General", "FreeCamKey", g_FreeCamKey, iniPath);
    g_HudHideKey = GetPrivateProfileIntA("General", "HudHideKey", g_HudHideKey, iniPath);
    g_FreezePlayer = GetPrivateProfileIntA("General", "FreezePlayer", g_FreezePlayer, iniPath);
    int startHidden = GetPrivateProfileIntA("HUD", "StartHidden", 0, iniPath);
    if (startHidden) g_HUDHidden = 1;

    char buf[64];
    if (GetPrivateProfileStringA("Camera", "MoveSpeed", "", buf, sizeof(buf), iniPath) > 0) {
        g_MoveSpeed = ParseFloat(buf, g_MoveSpeed);
    }
    if (GetPrivateProfileStringA("Camera", "FastMultiplier", "", buf, sizeof(buf), iniPath) > 0) {
        g_FastMultiplier = ParseFloat(buf, g_FastMultiplier);
    }
    if (GetPrivateProfileStringA("Camera", "SlowMultiplier", "", buf, sizeof(buf), iniPath) > 0) {
        g_SlowMultiplier = ParseFloat(buf, g_SlowMultiplier);
    }
    if (GetPrivateProfileStringA("Camera", "MouseSensitivity", "", buf, sizeof(buf), iniPath) > 0) {
        g_MouseSensitivity = ParseFloat(buf, g_MouseSensitivity);
    }
    g_InvertPitch = GetPrivateProfileIntA("Camera", "InvertPitch", g_InvertPitch, iniPath);
    g_InvertYaw   = GetPrivateProfileIntA("Camera", "InvertYaw",   g_InvertYaw,   iniPath);
    if (GetPrivateProfileStringA("Camera", "DefaultFOV", "", buf, sizeof(buf), iniPath) > 0) {
        float fov = ParseFloat(buf, 0.0f);
        if (fov >= 30.0f && fov <= 120.0f) {
            g_TargetFOV_Deg = fov;
            g_CustomFOVEnabled = 1;
        }
    }

    g_MenuX = GetPrivateProfileIntA("UI", "MenuX", g_MenuX, iniPath);
    g_MenuY = GetPrivateProfileIntA("UI", "MenuY", g_MenuY, iniPath);
    g_MenuW = GetPrivateProfileIntA("UI", "MenuWidth", g_MenuW, iniPath);
    g_MenuH = GetPrivateProfileIntA("UI", "MenuHeight", g_MenuH, iniPath);
    if (g_MenuW < 540) g_MenuW = 620;
    if (g_MenuH < 440) g_MenuH = 520;
}

static void SaveConfig(void) {
    char iniPath[MAX_PATH];
    GetIniPath(iniPath, MAX_PATH);

    char buf[64];
    wsprintfA(buf, "0x%02X", g_MenuKey);
    WritePrivateProfileStringA("General", "ToggleKey", buf, iniPath);

    wsprintfA(buf, "0x%02X", g_FreeCamKey);
    WritePrivateProfileStringA("General", "FreeCamKey", buf, iniPath);

    wsprintfA(buf, "0x%02X", g_HudHideKey);
    WritePrivateProfileStringA("General", "HudHideKey", buf, iniPath);

    wsprintfA(buf, "%d", g_FreezePlayer);
    WritePrivateProfileStringA("General", "FreezePlayer", buf, iniPath);

    FormatFloat(buf, sizeof(buf), g_MoveSpeed, 2);
    WritePrivateProfileStringA("Camera", "MoveSpeed", buf, iniPath);

    FormatFloat(buf, sizeof(buf), g_FastMultiplier, 2);
    WritePrivateProfileStringA("Camera", "FastMultiplier", buf, iniPath);

    FormatFloat(buf, sizeof(buf), g_SlowMultiplier, 2);
    WritePrivateProfileStringA("Camera", "SlowMultiplier", buf, iniPath);

    FormatFloat(buf, sizeof(buf), g_MouseSensitivity, 4);
    WritePrivateProfileStringA("Camera", "MouseSensitivity", buf, iniPath);

    wsprintfA(buf, "%d", g_InvertPitch);
    WritePrivateProfileStringA("Camera", "InvertPitch", buf, iniPath);

    wsprintfA(buf, "%d", g_InvertYaw);
    WritePrivateProfileStringA("Camera", "InvertYaw", buf, iniPath);

    FormatFloat(buf, sizeof(buf), g_TargetFOV_Deg, 1);
    WritePrivateProfileStringA("Camera", "DefaultFOV", buf, iniPath);

    wsprintfA(buf, "%d", g_HUDHidden);
    WritePrivateProfileStringA("HUD", "StartHidden", buf, iniPath);

    wsprintfA(buf, "%d", g_MenuX);
    WritePrivateProfileStringA("UI", "MenuX", buf, iniPath);

    wsprintfA(buf, "%d", g_MenuY);
    WritePrivateProfileStringA("UI", "MenuY", buf, iniPath);

    wsprintfA(buf, "%d", g_MenuW);
    WritePrivateProfileStringA("UI", "MenuWidth", buf, iniPath);

    wsprintfA(buf, "%d", g_MenuH);
    WritePrivateProfileStringA("UI", "MenuHeight", buf, iniPath);

    lstrcpyA(g_StatusMessage, "Saved editable values to scripts\\FreeCam.ini.");
}

// -----------------------------------------------------------------------------
// Safe 2D HUD Suppression: Uses engine's native exit path without map corruption
// -----------------------------------------------------------------------------
static void ApplyHUDState(int hide) {
    volatile uint8_t* pHudFlag = (volatile uint8_t*)ADDR_HUD_ENABLE_FLAG;
    if (pHudFlag) {
        *pHudFlag = hide ? 0 : 1;
    }

    volatile uint8_t* pRadarFlag = (volatile uint8_t*)ADDR_RADAR_FLAG;
    if (pRadarFlag) {
        *pRadarFlag = hide ? 0 : 1;
    }

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

static volatile void* g_CachedPlayerActor = NULL;

static void* GetPlayerActor(void) {
    if (IsBadReadPtr((const void*)ADDR_ACTOR_MGR, sizeof(void*)) ||
        IsBadReadPtr((const void*)ADDR_PLAYER_HANDLE, sizeof(uint32_t)) ||
        IsBadReadPtr((const void*)ADDR_RESOLVE_ACTOR, 16)) {
        return NULL;
    }
    const unsigned char* fnBytes = (const unsigned char*)ADDR_RESOLVE_ACTOR;
    // Verify DP.exe ResolveActor prologue at 0x006C5FD0 before calling
    if (fnBytes[0] == 0x00 && fnBytes[1] == 0x00) {
        return NULL;
    }
    void** ppActorMgr = (void**)ADDR_ACTOR_MGR;
    uint32_t* pPlayerHandle = (uint32_t*)ADDR_PLAYER_HANDLE;
    if (!ppActorMgr || !(*ppActorMgr) || !pPlayerHandle || !(*pPlayerHandle)) {
        return NULL;
    }
    if (IsBadReadPtr(*ppActorMgr, 64)) {
        return NULL;
    }
    ResolveActorFn ResolveActor = (ResolveActorFn)ADDR_RESOLVE_ACTOR;
    void* actor = ResolveActor(*ppActorMgr, *pPlayerHandle);
    g_CachedPlayerActor = actor;
    return actor;
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
// Camera Hook Callback (Called from 0x00401BD1 right before 0x006B62E0)
// -----------------------------------------------------------------------------
void __cdecl CameraHookWorker(void* pCamCtx) {
    if (!pCamCtx) return;

    float dt = GetDeltaTime();

    // Update cached York actor pointer on the game simulation thread
    (void)GetPlayerActor();

    float* pEye = (float*)((char*)pCamCtx + 4);       // 0x00BD964C (Eye X, Y, Z, W)
    float* pTarget = (float*)((char*)pCamCtx + 20);   // 0x00BD965C (Target X, Y, Z, W)
    float* pUp = (float*)((char*)pCamCtx + 36);       // 0x00BD966C (Up X, Y, Z, W)
    float* pFOV = (float*)((char*)pCamCtx + 84);      // 0x00BD969C (FOV Radians)

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

        // Mouse look when menu is closed OR when holding Right Mouse Button while menu is open
        int mouseLookActive = (g_GameHWnd && GetForegroundWindow() == g_GameHWnd &&
                               (!g_MenuOpen || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)));
        if (mouseLookActive) {
            RECT rc;
            if (GetClientRect(g_GameHWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
                POINT center = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
                ClientToScreen(g_GameHWnd, &center);
                if (!g_MouseLookWasActive) {
                    // First frame entering mouse-look (F5 toggled on, F9 menu closed,
                    // or RMB pressed while menu open): recenter cursor without snapping camera.
                    SetCursorPos(center.x, center.y);
                    g_MouseLookWasActive = 1;
                } else {
                    POINT cur;
                    if (GetCursorPos(&cur)) {
                        int dx = cur.x - center.x;
                        int dy = cur.y - center.y;
                        if (dx != 0 || dy != 0) {
                            // Deadly Premonition calls 0x006B62E0 with param_2=1 at 0x00401BD1,
                            // which negates proj._11 (flipping horizontal clip-space X).
                            // Because +xaxis = (cy, 0, -sy) maps to x_clip < 0 (left on screen)
                            // and d(fwd)/d(yaw) = (cy*cp, 0, -sy*cp), decreasing g_CamYaw when
                            // dx > 0 rotates fwd toward (-cy, 0, sy) = +x_clip (RIGHT on screen).
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

        // Process XInput Gamepad
        ProcessGamepadInput(dt);

        // Process keyboard 6-DOF flight
        float currentSpeed = g_MoveSpeed;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) currentSpeed *= g_FastMultiplier;
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000)) currentSpeed *= g_SlowMultiplier;

        float moveDist = currentSpeed * dt;

        float cp = FastCos(g_CamPitch);
        float sp = FastSin(g_CamPitch);
        float cy = FastCos(g_CamYaw);
        float sy = FastSin(g_CamYaw);

        Vec3 fwd = { sy * cp, sp, cy * cp };
        // Because 0x006B62E0 negates proj._11, D3DXMatrixLookAtLH's +xaxis = (cy, 0, -sy)
        // points LEFT on screen (x_clip < 0), while -xaxis = (-cy, 0, sy) points RIGHT on screen (x_clip > 0).
        Vec3 right = { -cy, 0.0f, sy };
        Vec3 upWorld = { 0.0f, 1.0f, 0.0f };

        if (!g_MenuOpen || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
            if (GetAsyncKeyState('W') & 0x8000) g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(fwd, moveDist));
            if (GetAsyncKeyState('S') & 0x8000) g_CamPos = Vec3_Sub(g_CamPos, Vec3_Mul(fwd, moveDist));
            if (GetAsyncKeyState('D') & 0x8000) g_CamPos = Vec3_Add(g_CamPos, Vec3_Mul(right, moveDist)); // D moves RIGHT on screen
            if (GetAsyncKeyState('A') & 0x8000) g_CamPos = Vec3_Sub(g_CamPos, Vec3_Mul(right, moveDist)); // A moves LEFT on screen
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

// -----------------------------------------------------------------------------
// Naked Hook for Camera Hook Site at 0x00401BD1
// -----------------------------------------------------------------------------
__attribute__((naked)) void CameraHookAsm(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "push ecx\n"
        "call _CameraHookWorker\n"
        "pop ecx\n"
        "mov eax, 0x006B62E0\n"
        "jmp eax\n"
    );
}

// -----------------------------------------------------------------------------
// Naked Hook for Radar / Minimap Render Routine at 0x006541B0
// -----------------------------------------------------------------------------
__attribute__((naked)) void RadarHookAsm(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "cmp dword ptr [_g_HUDHidden], 0\n"
        "jz 1f\n"
        "mov edx, ecx\n"
        "test edx, edx\n"
        "jnz 2f\n"
        "mov edx, 0x01476978\n"               // Static global Map Object
        "2:\n"
        "cmp dword ptr [edx + 0x14], 0\n"    // Mode 0: Overworld Fullscreen Map
        "je 1f\n"
        "cmp dword ptr [edx + 0x14], 1\n"    // Mode 1: Alternate Fullscreen Map
        "je 1f\n"
        "cmp dword ptr [edx + 0x14], 2\n"    // Mode 2: Overworld Map Tiles (MapFix)
        "je 1f\n"
        "cmp dword ptr [edx + 0x14], 3\n"    // Mode 3: Chapter / Interior Map
        "je 1f\n"
        "cmp dword ptr [edx + 0x14], 0x61\n" // Mode 0x61: Special Fullscreen Map
        "je 1f\n"
        "ret\n"                              // Suppress in-game minimap/radar (Mode 4) & blips/markers
        "1:\n"
        "sub esp, 0x10\n"
        "push ebx\n"
        "mov ebx, ecx\n"
        "mov eax, 0x006541B6\n"
        "jmp eax\n"
        ".att_syntax\n"
    );
}

__attribute__((naked)) void CarNaviHookAsm(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "cmp dword ptr [_g_HUDHidden], 0\n"
        "jz 1f\n"
        "cmp dword ptr [esp + 8], 0x12\n"     // Action 0x12 = 2D Render pass
        "jne 1f\n"
        "ret\n"                               // Suppress car navi / vehicle radar rendering
        "1:\n"
        "sub esp, 0x674\n"
        "mov eax, 0x005F5DF6\n"
        "jmp eax\n"
        ".att_syntax\n"
    );
}

// -----------------------------------------------------------------------------
// Dear ImGui D3D9 Renderer (Matches imgui_impl_dx9 vertex pipeline & atlas)
// -----------------------------------------------------------------------------
#define D3DFVF_VERTEX2D (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define ATLAS_W 256
#define ATLAS_H 128
#define FONT_HEIGHT 13
#define WHITE_UV_U (3.5f / (float)ATLAS_W)
#define WHITE_UV_V (3.5f / (float)ATLAS_H)

typedef struct {
    float x, y, z, rhw;
    DWORD color;
    float u, v;
} Vertex2D;

#define VERTEX_BUFFER_CAP 6144
static Vertex2D g_Verts[VERTEX_BUFFER_CAP];
static int g_VertCount = 0;
static IDirect3DTexture9* g_FontTexture = NULL;

static void FlushVerts(IDirect3DDevice9* dev) {
    if (g_VertCount > 0 && dev) {
        dev->lpVtbl->DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, g_VertCount / 3, g_Verts, sizeof(Vertex2D));
        g_VertCount = 0;
    }
}

static inline void AddVertexUV(IDirect3DDevice9* dev, float x, float y, DWORD col, float u, float v) {
    if (g_VertCount >= VERTEX_BUFFER_CAP - 6) {
        FlushVerts(dev);
    }
    // Apply -0.5f D3D9 screen-space half-pixel center offset (matches imgui_impl_dx9)
    g_Verts[g_VertCount].x = x - 0.5f;
    g_Verts[g_VertCount].y = y - 0.5f;
    g_Verts[g_VertCount].z = 0.0f;
    g_Verts[g_VertCount].rhw = 1.0f;
    g_Verts[g_VertCount].color = col;
    g_Verts[g_VertCount].u = u;
    g_Verts[g_VertCount].v = v;
    g_VertCount++;
}

static void DrawQuad(IDirect3DDevice9* dev, float x0, float y0, float x1, float y1, DWORD col) {
    // Sample center of 8x8 solid white texel block at top-left of g_FontTexture
    AddVertexUV(dev, x0, y0, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1, y0, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1, y1, col, WHITE_UV_U, WHITE_UV_V);

    AddVertexUV(dev, x0, y0, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1, y1, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x0, y1, col, WHITE_UV_U, WHITE_UV_V);
}

static void DrawTriangle(IDirect3DDevice9* dev, float x0, float y0, float x1, float y1, float x2, float y2, DWORD col) {
    AddVertexUV(dev, x0, y0, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1, y1, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x2, y2, col, WHITE_UV_U, WHITE_UV_V);
}

static void DrawLine(IDirect3DDevice9* dev, float x0, float y0, float x1, float y1, float thickness, DWORD col) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = FastSqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = (-dy / len) * (thickness * 0.5f);
    float ny = ( dx / len) * (thickness * 0.5f);

    AddVertexUV(dev, x0 + nx, y0 + ny, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1 + nx, y1 + ny, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1 - nx, y1 - ny, col, WHITE_UV_U, WHITE_UV_V);

    AddVertexUV(dev, x0 + nx, y0 + ny, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x1 - nx, y1 - ny, col, WHITE_UV_U, WHITE_UV_V);
    AddVertexUV(dev, x0 - nx, y0 - ny, col, WHITE_UV_U, WHITE_UV_V);
}

static void DrawRectBorder(IDirect3DDevice9* dev, float x, float y, float w, float h, DWORD borderCol, DWORD bgCol) {
    if ((bgCol >> 24) != 0) {
        DrawQuad(dev, x, y, x + w, y + h, bgCol);
    }
    if ((borderCol >> 24) != 0) {
        DrawQuad(dev, x, y, x + w, y + 1.0f, borderCol);
        DrawQuad(dev, x, y + h - 1.0f, x + w, y + h, borderCol);
        DrawQuad(dev, x, y + 1.0f, x + 1.0f, y + h - 1.0f, borderCol);
        DrawQuad(dev, x + w - 1.0f, y + 1.0f, x + w, y + h - 1.0f, borderCol);
    }
}

// -----------------------------------------------------------------------------
// Font Atlas (Exact Dear ImGui ProggyClean.ttf 13px 7x13 Bitmap from ZachFix)
// -----------------------------------------------------------------------------
typedef struct {
    float u0, v0, u1, v1;
    int w, h;
} GlyphInfo;

static GlyphInfo g_Glyphs[128];
static int g_FontAtlasReady = 0;

static const unsigned char g_ProggyClean13[95][13] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 32 ' '
    { 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00 }, // 33 '!'
    { 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 34 '"'
    { 0x00, 0x00, 0x28, 0x28, 0x7E, 0x14, 0x14, 0x3F, 0x0A, 0x0A, 0x00, 0x00, 0x00 }, // 35 '#'
    { 0x00, 0x00, 0x08, 0x3C, 0x0A, 0x0A, 0x1C, 0x28, 0x28, 0x1E, 0x08, 0x00, 0x00 }, // 36 '$'
    { 0x00, 0x00, 0x22, 0x15, 0x15, 0x0A, 0x28, 0x54, 0x54, 0x22, 0x00, 0x00, 0x00 }, // 37 '%'
    { 0x00, 0x00, 0x0C, 0x12, 0x12, 0x4C, 0x52, 0x22, 0x22, 0x5C, 0x00, 0x00, 0x00 }, // 38 '&'
    { 0x00, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 39 '''
    { 0x00, 0x10, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x10, 0x00 }, // 40 '('
    { 0x00, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x04, 0x00 }, // 41 ')'
    { 0x00, 0x00, 0x00, 0x00, 0x08, 0x2A, 0x1C, 0x2A, 0x08, 0x00, 0x00, 0x00, 0x00 }, // 42 '*'
    { 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00 }, // 43 '+'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x02, 0x00 }, // 44 ','
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 45 '-'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00 }, // 46 '.'
    { 0x00, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x00, 0x00 }, // 47 '/'
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x2A, 0x2A, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 48 '0'
    { 0x00, 0x00, 0x08, 0x0C, 0x0A, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00, 0x00, 0x00 }, // 49 '1'
    { 0x00, 0x00, 0x1C, 0x22, 0x20, 0x10, 0x08, 0x04, 0x02, 0x3E, 0x00, 0x00, 0x00 }, // 50 '2'
    { 0x00, 0x00, 0x1C, 0x22, 0x20, 0x18, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 51 '3'
    { 0x00, 0x00, 0x20, 0x30, 0x28, 0x24, 0x22, 0x7E, 0x20, 0x20, 0x00, 0x00, 0x00 }, // 52 '4'
    { 0x00, 0x00, 0x3E, 0x02, 0x02, 0x1E, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 53 '5'
    { 0x00, 0x00, 0x18, 0x04, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 54 '6'
    { 0x00, 0x00, 0x3E, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x00, 0x00, 0x00 }, // 55 '7'
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 56 '8'
    { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x10, 0x0C, 0x00, 0x00, 0x00 }, // 57 '9'
    { 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x00, 0x00, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 58 ':'
    { 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x04, 0x02, 0x00 }, // 59 ';'
    { 0x00, 0x00, 0x00, 0x00, 0x30, 0x0C, 0x03, 0x0C, 0x30, 0x00, 0x00, 0x00, 0x00 }, // 60 '<'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 61 '='
    { 0x00, 0x00, 0x00, 0x00, 0x06, 0x18, 0x60, 0x18, 0x06, 0x00, 0x00, 0x00, 0x00 }, // 62 '>'
    { 0x00, 0x00, 0x1C, 0x22, 0x20, 0x10, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00 }, // 63 '?'
    { 0x00, 0x00, 0x1C, 0x22, 0x59, 0x55, 0x55, 0x39, 0x02, 0x3C, 0x00, 0x00, 0x00 }, // 64 '@'
    { 0x00, 0x00, 0x18, 0x18, 0x24, 0x24, 0x3C, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00 }, // 65 'A'
    { 0x00, 0x00, 0x1E, 0x22, 0x22, 0x3E, 0x42, 0x42, 0x42, 0x3E, 0x00, 0x00, 0x00 }, // 66 'B'
    { 0x00, 0x00, 0x38, 0x44, 0x02, 0x02, 0x02, 0x02, 0x44, 0x38, 0x00, 0x00, 0x00 }, // 67 'C'
    { 0x00, 0x00, 0x1E, 0x22, 0x42, 0x42, 0x42, 0x42, 0x22, 0x1E, 0x00, 0x00, 0x00 }, // 68 'D'
    { 0x00, 0x00, 0x3E, 0x02, 0x02, 0x1E, 0x02, 0x02, 0x02, 0x3E, 0x00, 0x00, 0x00 }, // 69 'E'
    { 0x00, 0x00, 0x3E, 0x02, 0x02, 0x1E, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00 }, // 70 'F'
    { 0x00, 0x00, 0x38, 0x44, 0x02, 0x02, 0x72, 0x42, 0x44, 0x38, 0x00, 0x00, 0x00 }, // 71 'G'
    { 0x00, 0x00, 0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00 }, // 72 'H'
    { 0x00, 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00 }, // 73 'I'
    { 0x00, 0x00, 0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0E, 0x00, 0x00, 0x00 }, // 74 'J'
    { 0x00, 0x00, 0x42, 0x22, 0x12, 0x0A, 0x0E, 0x12, 0x22, 0x42, 0x00, 0x00, 0x00 }, // 75 'K'
    { 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x3E, 0x00, 0x00, 0x00 }, // 76 'L'
    { 0x00, 0x00, 0x63, 0x63, 0x55, 0x55, 0x49, 0x49, 0x41, 0x41, 0x00, 0x00, 0x00 }, // 77 'M'
    { 0x00, 0x00, 0x46, 0x46, 0x4A, 0x4A, 0x52, 0x52, 0x62, 0x62, 0x00, 0x00, 0x00 }, // 78 'N'
    { 0x00, 0x00, 0x18, 0x24, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00, 0x00, 0x00 }, // 79 'O'
    { 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00 }, // 80 'P'
    { 0x00, 0x00, 0x18, 0x24, 0x42, 0x42, 0x42, 0x42, 0x24, 0x58, 0x40, 0x00, 0x00 }, // 81 'Q'
    { 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x12, 0x22, 0x42, 0x00, 0x00, 0x00 }, // 82 'R'
    { 0x00, 0x00, 0x3C, 0x42, 0x02, 0x0C, 0x30, 0x40, 0x42, 0x3C, 0x00, 0x00, 0x00 }, // 83 'S'
    { 0x00, 0x00, 0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 84 'T'
    { 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00 }, // 85 'U'
    { 0x00, 0x00, 0x41, 0x41, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 86 'V'
    { 0x00, 0x00, 0x41, 0x49, 0x49, 0x55, 0x55, 0x36, 0x22, 0x22, 0x00, 0x00, 0x00 }, // 87 'W'
    { 0x00, 0x00, 0x42, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x42, 0x00, 0x00, 0x00 }, // 88 'X'
    { 0x00, 0x00, 0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 89 'Y'
    { 0x00, 0x00, 0x7E, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x7E, 0x00, 0x00, 0x00 }, // 90 'Z'
    { 0x00, 0x1C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1C, 0x00 }, // 91 '['
    { 0x00, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x00, 0x00 }, // 92 'backslash'
    { 0x00, 0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C, 0x00 }, // 93 ']'
    { 0x00, 0x08, 0x08, 0x14, 0x14, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 94 '^'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00 }, // 95 '_'
    { 0x00, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 96 '`'
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x20, 0x3C, 0x22, 0x22, 0x3C, 0x00, 0x00, 0x00 }, // 97 'a'
    { 0x00, 0x02, 0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x1E, 0x00, 0x00, 0x00 }, // 98 'b'
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 99 'c'
    { 0x00, 0x20, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x00, 0x00 }, // 100 'd'
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x3E, 0x02, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 101 'e'
    { 0x00, 0x38, 0x04, 0x04, 0x1E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00 }, // 102 'f'
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x1C }, // 103 'g'
    { 0x00, 0x02, 0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 }, // 104 'h'
    { 0x00, 0x08, 0x00, 0x00, 0x0C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 105 'i'
    { 0x00, 0x10, 0x00, 0x00, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0E, 0x00 }, // 106 'j'
    { 0x00, 0x02, 0x02, 0x02, 0x22, 0x12, 0x0A, 0x0E, 0x12, 0x22, 0x00, 0x00, 0x00 }, // 107 'k'
    { 0x00, 0x0C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 108 'l'
    { 0x00, 0x00, 0x00, 0x00, 0x37, 0x49, 0x49, 0x49, 0x49, 0x49, 0x00, 0x00, 0x00 }, // 109 'm'
    { 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00 }, // 110 'n'
    { 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00 }, // 111 'o'
    { 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x02 }, // 112 'p'
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20 }, // 113 'q'
    { 0x00, 0x00, 0x00, 0x00, 0x1A, 0x26, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00 }, // 114 'r'
    { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x02, 0x0C, 0x10, 0x20, 0x1E, 0x00, 0x00, 0x00 }, // 115 's'
    { 0x00, 0x00, 0x04, 0x04, 0x3C, 0x04, 0x04, 0x04, 0x04, 0x38, 0x00, 0x00, 0x00 }, // 116 't'
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x00, 0x00 }, // 117 'u'
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00, 0x00 }, // 118 'v'
    { 0x00, 0x00, 0x00, 0x00, 0x41, 0x49, 0x49, 0x55, 0x36, 0x22, 0x00, 0x00, 0x00 }, // 119 'w'
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x14, 0x08, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00 }, // 120 'x'
    { 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x1C }, // 121 'y'
    { 0x00, 0x00, 0x00, 0x00, 0x3E, 0x20, 0x10, 0x08, 0x04, 0x3E, 0x00, 0x00, 0x00 }, // 122 'z'
    { 0x00, 0x30, 0x08, 0x08, 0x08, 0x08, 0x06, 0x08, 0x08, 0x08, 0x08, 0x30, 0x00 }, // 123 '{'
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 }, // 124 '|'
    { 0x00, 0x06, 0x08, 0x08, 0x08, 0x08, 0x30, 0x08, 0x08, 0x08, 0x08, 0x06, 0x00 }, // 125 '}'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 126 '~'
};

static uint32_t g_AtlasPixels[ATLAS_W * ATLAS_H];

static void CreateFontAtlas(IDirect3DDevice9* dev) {
    if (g_FontAtlasReady && g_FontTexture) return;

    for (int i = 0; i < ATLAS_W * ATLAS_H; i++) {
        g_AtlasPixels[i] = 0x00FFFFFF;
    }

    // Reserve top-left 8x8 pixel opaque white block for DrawQuad / DrawLine / DrawTriangle
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            g_AtlasPixels[y * ATLAS_W + x] = 0xFFFFFFFF;
        }
    }

    int curX = 12;
    int curY = 2;
    int rowH = 18;

    for (int c = 32; c < 127; c++) {
        int idx = c - 32;
        if (curX + 7 + 4 >= ATLAS_W) {
            curX = 12;
            curY += rowH;
        }

        for (int r = 0; r < 13; r++) {
            unsigned char rowBits = g_ProggyClean13[idx][r];
            for (int col = 0; col < 7; col++) {
                if ((rowBits >> col) & 1) {
                    g_AtlasPixels[(curY + r) * ATLAS_W + (curX + col)] = 0xFFFFFFFF;
                }
            }
        }

        g_Glyphs[c].u0 = (float)curX / (float)ATLAS_W;
        g_Glyphs[c].v0 = (float)curY / (float)ATLAS_H;
        g_Glyphs[c].u1 = (float)(curX + 7) / (float)ATLAS_W;
        g_Glyphs[c].v1 = (float)(curY + 13) / (float)ATLAS_H;
        g_Glyphs[c].w = 7;
        g_Glyphs[c].h = 13;

        curX += 11;
    }

    if (dev->lpVtbl->CreateTexture(dev, ATLAS_W, ATLAS_H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_FontTexture, NULL) == D3D_OK) {
        D3DLOCKED_RECT lr;
        if (g_FontTexture->lpVtbl->LockRect(g_FontTexture, 0, &lr, NULL, 0) == D3D_OK) {
            for (int y = 0; y < ATLAS_H; y++) {
                MemCpy((char*)lr.pBits + y * lr.Pitch, &g_AtlasPixels[y * ATLAS_W], ATLAS_W * 4);
            }
            g_FontTexture->lpVtbl->UnlockRect(g_FontTexture, 0);
            g_FontAtlasReady = 1;
        }
    }
}

static int DrawCharTextured(IDirect3DDevice9* dev, float x, float y, char c, DWORD col) {
    if (c < 32 || c > 126) c = ' ';
    GlyphInfo* g = &g_Glyphs[(int)c];

    float x0 = (float)(int)x;
    float y0 = (float)(int)y;
    float x1 = x0 + (float)g->w;
    float y1 = y0 + (float)g->h;

    AddVertexUV(dev, x0, y0, col, g->u0, g->v0);
    AddVertexUV(dev, x1, y0, col, g->u1, g->v0);
    AddVertexUV(dev, x1, y1, col, g->u1, g->v1);

    AddVertexUV(dev, x0, y0, col, g->u0, g->v0);
    AddVertexUV(dev, x1, y1, col, g->u1, g->v1);
    AddVertexUV(dev, x0, y1, col, g->u0, g->v1);

    return g->w;
}

static void DrawTextString(IDirect3DDevice9* dev, float x, float y, const char* str, DWORD col) {
    if (!str) return;
    float startX = (float)(int)x;
    float curX = startX;
    float curY = (float)(int)y;
    while (*str) {
        if (*str == '\n') {
            curX = startX;
            curY += 17.0f;
            str++;
            continue;
        }
        int w = DrawCharTextured(dev, curX, curY, *str, col);
        curX += (float)w;
        str++;
    }
}

static int MeasureTextWidth(const char* str) {
    if (!str) return 0;
    int w = 0;
    int maxW = 0;
    while (*str) {
        if (*str == '\n') {
            if (w > maxW) maxW = w;
            w = 0;
            str++;
            continue;
        }
        w += 7;
        str++;
    }
    return (w > maxW) ? w : maxW;
}

// -----------------------------------------------------------------------------
// Dear ImGui Style Widgets (Modeled directly on ZachFix's ui_settings.cpp)
// -----------------------------------------------------------------------------

// ImGui::SeparatorText(label) - Text on left, horizontal line extending to right edge
static void ImGui_SeparatorText(IDirect3DDevice9* dev, float x, float y, float w, const char* label) {
    DrawTextString(dev, x, y, label, IM_COL_TEXT);
    int tw = MeasureTextWidth(label);
    float lineX0 = x + (float)tw + 8.0f;
    float lineX1 = x + w;
    float lineY  = y + 7.0f;
    if (lineX1 > lineX0) {
        DrawQuad(dev, lineX0, lineY, lineX1, lineY + 1.0f, IM_COL_SEPARATOR);
    }
}

// ImGui::CollapsingHeader(label) - Full-width header bar with disclosure arrow
static int ImGui_CollapsingHeader(IDirect3DDevice9* dev, float x, float y, float w, const char* label, int* pOpen) {
    float h = 20.0f;
    int hovered = (g_MouseX >= x && g_MouseX <= x + w && g_MouseY >= y && g_MouseY <= y + h);
    if (hovered && g_FrameClicked && pOpen) {
        *pOpen = !(*pOpen);
    }
    DWORD bgCol = IM_COL_HEADER;
    if (hovered) bgCol = g_MouseDown ? IM_COL_HEADER_ACTIVE : IM_COL_HEADER_HOVER;
    DrawQuad(dev, x, y, x + w, y + h, bgCol);

    int isOpen = pOpen ? *pOpen : 1;
    if (isOpen) {
        DrawTriangle(dev, x + 6.0f, y + 7.0f, x + 14.0f, y + 7.0f, x + 10.0f, y + 13.0f, IM_COL_TEXT);
    } else {
        DrawTriangle(dev, x + 7.0f, y + 6.0f, x + 7.0f, y + 14.0f, x + 13.0f, y + 10.0f, IM_COL_TEXT);
    }
    DrawTextString(dev, x + 20.0f, y + 4.0f, label, IM_COL_TEXT);
    return isOpen;
}

// ImGui::BulletText(text) - Crisp bullet point + indented text
static void ImGui_BulletText(IDirect3DDevice9* dev, float x, float y, const char* text, DWORD col) {
    DrawQuad(dev, x + 4.0f, y + 5.0f, x + 8.0f, y + 9.0f, col);
    DrawTextString(dev, x + 16.0f, y, text, col);
}

// ImGui::BeginTabItem(label) - Compact tab sized to text + 16px horizontal padding
static int ImGui_TabItem(IDirect3DDevice9* dev, float* ioCurX, float y, float h, const char* label, int isActive) {
    int textW = MeasureTextWidth(label);
    float w = (float)textW + 18.0f;
    float x = *ioCurX;
    *ioCurX += w + 4.0f;

    int hovered = (g_MouseX >= x && g_MouseX <= x + w && g_MouseY >= y && g_MouseY <= y + h);
    int clicked = (hovered && g_FrameClicked);

    DWORD bgCol = isActive ? IM_COL_TAB_ACTIVE : (hovered ? IM_COL_TAB_HOVER : IM_COL_TAB);
    // Slightly beveled top corners like Dear ImGui tabs
    DrawQuad(dev, x + 2.0f, y, x + w - 2.0f, y + 2.0f, bgCol);
    DrawQuad(dev, x, y + 2.0f, x + w, y + h, bgCol);

    float textX = x + 9.0f;
    float textY = y + 5.0f;
    DrawTextString(dev, textX, textY, label, IM_COL_TEXT);
    return clicked;
}

// ImGui::Button(label) - Auto-sized or fixed-size button with StyleColorsDark blue
static int ImGui_Button(IDirect3DDevice9* dev, float x, float y, float w, float h, const char* label, const char* tooltip) {
    if (w <= 0.0f) {
        w = (float)MeasureTextWidth(label) + 16.0f;
    }
    int hovered = (g_MouseX >= x && g_MouseX <= x + w && g_MouseY >= y && g_MouseY <= y + h);
    if (hovered && tooltip) g_HoveredTooltip = tooltip;
    int clicked = (hovered && g_FrameClicked);

    DWORD bgCol = IM_COL_BUTTON;
    if (hovered) bgCol = g_MouseDown ? IM_COL_BUTTON_ACTIVE : IM_COL_BUTTON_HOVER;

    DrawQuad(dev, x, y, x + w, y + h, bgCol);

    int textW = MeasureTextWidth(label);
    float textX = x + (float)(int)((w - (float)textW) * 0.5f);
    float textY = y + (float)(int)((h - 13.0f) * 0.5f);
    DrawTextString(dev, textX, textY, label, IM_COL_TEXT);
    return clicked;
}

// ImGui::Checkbox(label, &v) + SameLine + TextDisabled(hint)
static int ImGui_Checkbox(IDirect3DDevice9* dev, float x, float y, const char* label, const char* disabledHint, volatile int* pValue, const char* tooltip) {
    float boxSize = 18.0f;
    int labelW = MeasureTextWidth(label);
    int hovered = (g_MouseX >= x && g_MouseX <= x + boxSize + 8.0f + (float)labelW &&
                   g_MouseY >= y && g_MouseY <= y + boxSize);
    if (hovered && tooltip) g_HoveredTooltip = tooltip;

    int changed = 0;
    if (hovered && g_FrameClicked) {
        *pValue = !(*pValue);
        changed = 1;
    }

    DWORD boxBg = IM_COL_FRAME_BG;
    if (hovered) boxBg = g_MouseDown ? IM_COL_FRAME_ACTIVE : IM_COL_FRAME_HOVER;
    DrawQuad(dev, x, y, x + boxSize, y + boxSize, boxBg);

    if (*pValue) {
        // Draw authentic Dear ImGui vector checkmark inside the 18x18 box
        DrawLine(dev, x + 3.5f, y + 9.5f, x + 7.5f, y + 13.5f, 2.2f, IM_COL_CHECKMARK);
        DrawLine(dev, x + 7.0f, y + 13.5f, x + 14.5f, y + 4.5f, 2.2f, IM_COL_CHECKMARK);
    }

    float textX = x + boxSize + 6.0f;
    DrawTextString(dev, textX, y + 3.0f, label, IM_COL_TEXT);

    if (disabledHint && *disabledHint) {
        float hintX = textX + (float)labelW + 8.0f;
        DrawTextString(dev, hintX, y + 3.0f, disabledHint, IM_COL_TEXT_DISABLED);
    }
    return changed;
}

// ImGui::SliderFloat(label, &v, min, max, format)
// Authentic Dear ImGui layout:
// - Slider frame on the LEFT (width = 60% of content width)
// - Grab handle inside the frame
// - Formatted value string centered INSIDE the slider frame
// - Label on the RIGHT of the slider frame
static int ImGui_SliderFloat(IDirect3DDevice9* dev, int id, float x, float y, float w, const char* label, float* pValue, float minVal, float maxVal, int decimals, const char* suffix, const char* tooltip) {
    float h = 19.0f;
    float trackX = x;
    float trackW = (float)(int)(w * 0.58f);
    float labelX = trackX + trackW + 8.0f;

    int hovered = (g_MouseX >= trackX && g_MouseX <= trackX + trackW &&
                   g_MouseY >= y && g_MouseY <= y + h);
    int labelHovered = (g_MouseX >= trackX && g_MouseX <= labelX + (float)MeasureTextWidth(label) &&
                        g_MouseY >= y && g_MouseY <= y + h);
    if (labelHovered && tooltip) g_HoveredTooltip = tooltip;

    if (hovered && g_FrameClicked && g_ActiveSliderId == -1) {
        g_ActiveSliderId = id;
    }

    int changed = 0;
    if (g_ActiveSliderId == id) {
        if (g_MouseDown) {
            float ratio = ((float)g_MouseX - (trackX + 6.0f)) / (trackW - 12.0f);
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            float newVal = minVal + ratio * (maxVal - minVal);
            if (newVal != *pValue) {
                *pValue = newVal;
                changed = 1;
            }
        } else {
            g_ActiveSliderId = -1;
        }
    }

    DWORD frameBg = IM_COL_FRAME_BG;
    if (g_ActiveSliderId == id) frameBg = IM_COL_FRAME_ACTIVE;
    else if (hovered) frameBg = IM_COL_FRAME_HOVER;

    DrawQuad(dev, trackX, y, trackX + trackW, y + h, frameBg);

    float ratio = (*pValue - minVal) / (maxVal - minVal);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    float grabW = 10.0f;
    float grabX = trackX + 2.0f + ratio * (trackW - 4.0f - grabW);
    DWORD grabCol = (g_ActiveSliderId == id) ? IM_COL_SLIDER_GRAB_ACTIVE : IM_COL_SLIDER_GRAB;
    DrawQuad(dev, grabX, y + 2.0f, grabX + grabW, y + h - 2.0f, grabCol);

    char valStr[48];
    FormatFloat(valStr, sizeof(valStr), *pValue, decimals);
    if (suffix && *suffix) lstrcatA(valStr, suffix);

    int valW = MeasureTextWidth(valStr);
    float valX = trackX + (float)(int)((trackW - (float)valW) * 0.5f);
    DrawTextString(dev, valX, y + 3.0f, valStr, IM_COL_TEXT);

    DrawTextString(dev, labelX, y + 3.0f, label, IM_COL_TEXT);
    return changed;
}

static void DrawTooltipPopup(IDirect3DDevice9* dev, const char* text) {
    if (!text || !*text) return;

    int lines = 1;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') lines++;
    }
    int maxW = MeasureTextWidth(text);
    float boxW = (float)maxW + 16.0f;
    float boxH = (float)(lines * 17 + 10);
    float bx = (float)g_MouseX + 16.0f;
    float by = (float)g_MouseY + 18.0f;

    DrawRectBorder(dev, bx, by, boxW, boxH, IM_COL_BORDER, IM_COL_POPUP_BG);
    DrawTextString(dev, bx + 8.0f, by + 5.0f, text, IM_COL_TEXT);
}

static void DrawImGuiCursor(IDirect3DDevice9* dev, float mx, float my) {
    // Crisp Dear ImGui software arrow cursor (io.MouseDrawCursor)
    DrawTriangle(dev, mx, my, mx, my + 17.0f, mx + 12.0f, my + 12.0f, 0xFF000000);
    DrawTriangle(dev, mx + 1.0f, my + 2.0f, mx + 1.0f, my + 15.0f, mx + 10.0f, my + 11.5f, 0xFFFFFFFF);
}

// -----------------------------------------------------------------------------
// Poll Input State Once Per Frame (Matches ZachFix RenderSettingsUiInternal)
// -----------------------------------------------------------------------------
static void UpdateUiInputOncePerFrame(void) {
    if (g_UiUpdatedFrame) return;
    g_UiUpdatedFrame = 1;

    // 1. Poll toggle keys on render thread so DirectInput / other WndProcs never block them
    int menuDown = (GetAsyncKeyState(g_MenuKey) & 0x8000) != 0;
    if (menuDown) {
        if (!g_MenuKeyLatched) {
            g_MenuKeyLatched = 1;
            g_MenuOpen = !g_MenuOpen;
            if (g_MenuOpen) ClipCursor(NULL);
            ShowCursor(g_MenuOpen ? TRUE : FALSE);
        }
    } else {
        g_MenuKeyLatched = 0;
    }

    int fcDown = (GetAsyncKeyState(g_FreeCamKey) & 0x8000) != 0;
    if (fcDown) {
        if (!g_FreeCamKeyLatched) {
            g_FreeCamKeyLatched = 1;
            g_FreeCamActive = !g_FreeCamActive;
            lstrcpyA(g_StatusMessage, g_FreeCamActive
                ? "Free Camera enabled (F5). Use WASD + Mouse to fly."
                : "Free Camera disabled (F5). Returned to game camera.");
        }
    } else {
        g_FreeCamKeyLatched = 0;
    }

    int hudDown = (GetAsyncKeyState(g_HudHideKey) & 0x8000) != 0;
    if (hudDown) {
        if (!g_HudKeyLatched) {
            g_HudKeyLatched = 1;
            g_HUDHidden = !g_HUDHidden;
            ApplyHUDState(g_HUDHidden);
            lstrcpyA(g_StatusMessage, g_HUDHidden
                ? "In-game 2D HUD hidden (F4)."
                : "In-game 2D HUD restored (F4).");
        }
    } else {
        g_HudKeyLatched = 0;
    }

    if (g_HUDHidden) {
        volatile uint8_t* pSuppressFlag = (volatile uint8_t*)ADDR_GLOBAL_SUPPRESS_FLAG;
        if (pSuppressFlag && *pSuppressFlag == 0) {
            *pSuppressFlag = 1;
        }
        volatile uint8_t* pRadarFlag = (volatile uint8_t*)ADDR_RADAR_FLAG;
        if (pRadarFlag && *pRadarFlag != 0) {
            *pRadarFlag = 0;
        }
    }

    if (!g_MenuOpen) {
        g_DraggingWindow = 0;
        g_ResizingWindow = 0;
        g_ActiveSliderId = -1;
        g_MouseClicked = 0;
        g_FrameClicked = 0;
        return;
    }

    // 2. Update mouse position every frame via GetCursorPos + ScreenToClient (matches ImGui_ImplWin32)
    POINT pt;
    if (GetCursorPos(&pt)) {
        if (g_GameHWnd) {
            ScreenToClient(g_GameHWnd, &pt);
        }
        g_MouseX = pt.x;
        g_MouseY = pt.y;
    }

    // 3. Poll left mouse button edge for reliable clicking regardless of WndProc chain order
    int lbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    int risingEdge = (lbDown && !g_PrevLButtonDown);
    g_PrevLButtonDown = lbDown;
    g_MouseDown = lbDown;

    g_FrameClicked = (g_MouseClicked || risingEdge) ? 1 : 0;
    g_MouseClicked = 0;

    // 4. Handle window titlebar dragging & bottom-right corner resizing
    float mx = (float)g_MenuX;
    float my = (float)g_MenuY;
    float mw = (float)g_MenuW;
    float mh = g_WindowCollapsed ? 24.0f : (float)g_MenuH;

    if (g_FrameClicked) {
        // Check bottom-right resize grip (16x16)
        if (!g_WindowCollapsed &&
            g_MouseX >= mx + mw - 16.0f && g_MouseX <= mx + mw &&
            g_MouseY >= my + mh - 16.0f && g_MouseY <= my + mh) {
            g_ResizingWindow = 1;
            g_DragOffsetX = (int)(mx + mw) - g_MouseX;
            g_DragOffsetY = (int)(my + mh) - g_MouseY;
        }
        // Check titlebar drag (excluding collapse arrow [0..24] and close [X] button [mw-24..mw])
        else if (g_MouseX >= mx + 24.0f && g_MouseX <= mx + mw - 24.0f &&
                 g_MouseY >= my && g_MouseY <= my + 24.0f) {
            g_DraggingWindow = 1;
            g_DragOffsetX = g_MouseX - g_MenuX;
            g_DragOffsetY = g_MouseY - g_MenuY;
        }
    }

    if (!g_MouseDown) {
        g_DraggingWindow = 0;
        g_ResizingWindow = 0;
        g_ActiveSliderId = -1;
    } else {
        if (g_DraggingWindow) {
            g_MenuX = g_MouseX - g_DragOffsetX;
            g_MenuY = g_MouseY - g_DragOffsetY;
        } else if (g_ResizingWindow) {
            int newW = (g_MouseX + g_DragOffsetX) - g_MenuX;
            int newH = (g_MouseY + g_DragOffsetY) - g_MenuY;
            if (newW < 540) newW = 540;
            if (newH < 440) newH = 440;
            g_MenuW = newW;
            g_MenuH = newH;
        }
    }
}

// -----------------------------------------------------------------------------
// Master UI Render Function (Direct Clone of ZachFix's DrawSettingsWindow)
// -----------------------------------------------------------------------------
static int g_HudHeaderOpen   = 1;
static int g_GpHeaderOpen    = 1;
static int g_DiagHeaderOpen  = 1;

static void RenderModUI(IDirect3DDevice9* dev) {
    if (!dev || !g_MenuOpen) return;

    CreateFontAtlas(dev);
    if (!g_FontTexture) return;

    g_HoveredTooltip = NULL;

    RECT rc;
    if (g_GameHWnd && GetClientRect(g_GameHWnd, &rc) && rc.right > 0 && rc.bottom > 0) {
        D3DVIEWPORT9 vp = { 0, 0, (DWORD)rc.right, (DWORD)rc.bottom, 0.0f, 1.0f };
        dev->lpVtbl->SetViewport(dev, &vp);
    }

    dev->lpVtbl->SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->lpVtbl->SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->lpVtbl->SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    dev->lpVtbl->SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0x0000000F);

    dev->lpVtbl->SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    dev->lpVtbl->SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->lpVtbl->SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->lpVtbl->SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev->lpVtbl->SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->lpVtbl->SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev->lpVtbl->SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->lpVtbl->SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    // Use D3DTEXF_POINT so ProggyClean 13px bitmap glyphs render with 1:1 pixel sharpness
    dev->lpVtbl->SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->lpVtbl->SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->lpVtbl->SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, FALSE);

    dev->lpVtbl->SetFVF(dev, D3DFVF_VERTEX2D);
    dev->lpVtbl->SetVertexShader(dev, NULL);
    dev->lpVtbl->SetPixelShader(dev, NULL);
    dev->lpVtbl->SetTexture(dev, 0, (IDirect3DBaseTexture9*)g_FontTexture);

    float mx = (float)g_MenuX;
    float my = (float)g_MenuY;
    float mw = (float)g_MenuW;
    float mh = g_WindowCollapsed ? 24.0f : (float)g_MenuH;

    // 1. Window Body & Border (ImGui::Begin("FreeCam Settings", &g_open))
    if (!g_WindowCollapsed) {
        DrawRectBorder(dev, mx, my, mw, mh, IM_COL_BORDER, IM_COL_WINDOW_BG);
    }

    // 2. Title Bar (ImGuiCol_TitleBgActive)
    DrawQuad(dev, mx + 1.0f, my + 1.0f, mx + mw - 1.0f, my + 24.0f, IM_COL_TITLE_BG_ACTIVE);
    DrawQuad(dev, mx, my + 24.0f, mx + mw, my + 25.0f, IM_COL_BORDER);

    // Collapse Arrow Triangle on left of Title Bar
    int collapseHover = (g_MouseX >= mx + 4.0f && g_MouseX <= mx + 22.0f &&
                         g_MouseY >= my + 2.0f && g_MouseY <= my + 22.0f);
    if (collapseHover) {
        DrawQuad(dev, mx + 4.0f, my + 3.0f, mx + 22.0f, my + 21.0f, IM_COL_BUTTON_HOVER);
        if (g_FrameClicked) g_WindowCollapsed = !g_WindowCollapsed;
    }
    if (g_WindowCollapsed) {
        DrawTriangle(dev, mx + 9.0f, my + 7.0f, mx + 9.0f, my + 17.0f, mx + 17.0f, my + 12.0f, IM_COL_TEXT);
    } else {
        DrawTriangle(dev, mx + 8.0f, my + 9.0f, mx + 18.0f, my + 9.0f, mx + 13.0f, my + 16.0f, IM_COL_TEXT);
    }

    // Window Title (Matches "ZachFix Settings" in ui_settings.cpp:2272)
    DrawTextString(dev, mx + 26.0f, my + 6.0f, "FreeCam Settings", IM_COL_TEXT);

    // Close Button [X] on right of Title Bar
    float closeX = mx + mw - 22.0f;
    float closeY = my + 3.0f;
    int closeHover = (g_MouseX >= closeX && g_MouseX <= closeX + 18.0f &&
                      g_MouseY >= closeY && g_MouseY <= closeY + 18.0f);
    if (closeHover) {
        DrawQuad(dev, closeX, closeY, closeX + 18.0f, closeY + 18.0f, IM_COL_BUTTON_HOVER);
        if (g_FrameClicked) {
            g_MenuOpen = 0;
            ShowCursor(FALSE);
        }
    }
    DrawLine(dev, closeX + 4.5f, closeY + 4.5f, closeX + 13.5f, closeY + 13.5f, 1.8f, IM_COL_TEXT);
    DrawLine(dev, closeX + 13.5f, closeY + 4.5f, closeX + 4.5f, closeY + 13.5f, 1.8f, IM_COL_TEXT);

    if (g_WindowCollapsed) {
        DrawImGuiCursor(dev, (float)g_MouseX, (float)g_MouseY);
        FlushVerts(dev);
        return;
    }

    float contentX = mx + 10.0f;
    float contentW = mw - 20.0f;
    float curY = my + 30.0f;

    // 3. Display Name Header (Matches ImGui::TextUnformatted(kZachFixDisplayName) at ui_settings.cpp:2278)
    DrawTextString(dev, contentX, curY, "FreeCam v1.2.2", IM_COL_TEXT);
    curY += 18.0f;

    // 4. Top Action Button Row (Matches ui_settings.cpp:2280-2295: Save to INI | Reload INI | Apply | F9 closes)
    float btnX = contentX;
    float btnW1 = (float)MeasureTextWidth("Save to INI") + 16.0f;
    if (ImGui_Button(dev, btnX, curY, btnW1, 21.0f, "Save to INI", "Save current FreeCam and HUD settings to scripts\\FreeCam.ini.")) {
        SaveConfig();
    }
    btnX += btnW1 + 6.0f;

    float btnW2 = (float)MeasureTextWidth("Reload INI") + 16.0f;
    if (ImGui_Button(dev, btnX, curY, btnW2, 21.0f, "Reload INI", "Reload settings from scripts\\FreeCam.ini.")) {
        LoadConfig();
        ApplyHUDState(g_HUDHidden);
        lstrcpyA(g_StatusMessage, "Reloaded editable settings from scripts\\FreeCam.ini.");
    }
    btnX += btnW2 + 6.0f;

    float btnW3 = (float)MeasureTextWidth("Apply") + 16.0f;
    if (ImGui_Button(dev, btnX, curY, btnW3, 21.0f, "Apply", "Apply live settings immediately.")) {
        ApplyHUDState(g_HUDHidden);
        lstrcpyA(g_StatusMessage, "Live settings applied. Save to INI to persist them.");
    }
    btnX += btnW3 + 6.0f;

    float btnW4 = (float)MeasureTextWidth("Reset Defaults") + 16.0f;
    if (ImGui_Button(dev, btnX, curY, btnW4, 21.0f, "Reset Defaults", "Restore default movement speed, sensitivity, and FOV.")) {
        g_MoveSpeed = 15.0f;
        g_FastMultiplier = 3.0f;
        g_SlowMultiplier = 0.25f;
        g_MouseSensitivity = 0.003f;
        g_InvertPitch = 0;
        g_InvertYaw = 0;
        g_TargetFOV_Deg = 65.0f;
        g_CustomFOVEnabled = 0;
        g_FreezePlayer = 1;
        lstrcpyA(g_StatusMessage, "Restored default settings (live). Save to INI to persist.");
    }
    btnX += btnW4 + 10.0f;

    // ImGui::SameLine(); ImGui::TextDisabled("F9 closes");
    DrawTextString(dev, btnX, curY + 4.0f, "F9 closes", IM_COL_TEXT_DISABLED);
    curY += 25.0f;

    // 5. Status Line & Separator (Matches ui_settings.cpp:2296-2297)
    DrawTextString(dev, contentX, curY, g_StatusMessage, IM_COL_STATUS_TEXT);
    curY += 18.0f;
    DrawQuad(dev, contentX, curY, contentX + contentW, curY + 1.0f, IM_COL_SEPARATOR);
    curY += 7.0f;

    // 6. Tab Bar (Matches ImGui::BeginTabBar("ZachFixTabs") at ui_settings.cpp:2299)
    float tabX = contentX;
    float tabH = 22.0f;
    if (ImGui_TabItem(dev, &tabX, curY, tabH, "Camera",      g_SelectedTab == 0)) g_SelectedTab = 0;
    if (ImGui_TabItem(dev, &tabX, curY, tabH, "HUD",         g_SelectedTab == 1)) g_SelectedTab = 1;
    if (ImGui_TabItem(dev, &tabX, curY, tabH, "Gamepad",     g_SelectedTab == 2)) g_SelectedTab = 2;
    if (ImGui_TabItem(dev, &tabX, curY, tabH, "Diagnostics", g_SelectedTab == 3)) g_SelectedTab = 3;
    if (ImGui_TabItem(dev, &tabX, curY, tabH, "About",       g_SelectedTab == 4)) g_SelectedTab = 4;

    // Dear ImGui active tab bar underline
    DrawQuad(dev, contentX, curY + tabH, contentX + contentW, curY + tabH + 2.0f, IM_COL_TAB_ACTIVE);
    curY += tabH + 10.0f;

    // 7. Tab Contents
    if (g_SelectedTab == 0) {
        // --- CAMERA TAB (Modeled on ZachFix's DrawSettingsTab) ---
        ImGui_SeparatorText(dev, contentX, curY, contentW, "Free Camera Flight");
        curY += 20.0f;

        if (ImGui_Checkbox(dev, contentX, curY, "Enable Free Camera", "(F5 immediate)", &g_FreeCamActive,
                "Detaches the camera from York and enables 6-DOF flight.\nHotkey: F5")) {
            lstrcpyA(g_StatusMessage, g_FreeCamActive ? "Free Camera: enabled (live)." : "Free Camera: disabled (live).");
        }
        curY += 22.0f;

        if (ImGui_Checkbox(dev, contentX, curY, "Freeze Player (York) While Flying", "(immediate)", &g_FreezePlayer,
                "Suppresses gameplay WASD/action inputs from moving York while FreeCam is active.")) {
            lstrcpyA(g_StatusMessage, g_FreezePlayer ? "Freeze Player: enabled (live)." : "Freeze Player: disabled (live).");
        }
        curY += 24.0f;

        ImGui_SeparatorText(dev, contentX, curY, contentW, "Movement & Speed");
        curY += 20.0f;

        ImGui_SliderFloat(dev, 1, contentX, curY, contentW, "Flight Speed", &g_MoveSpeed, 1.0f, 100.0f, 1, " u/s",
            "Base camera movement speed in world units per second.");
        curY += 23.0f;

        ImGui_SliderFloat(dev, 2, contentX, curY, contentW, "Boost Multiplier (Shift / L3)", &g_FastMultiplier, 1.0f, 10.0f, 2, "x",
            "Speed multiplier applied when holding Left Shift or Gamepad L3.");
        curY += 23.0f;

        ImGui_SliderFloat(dev, 3, contentX, curY, contentW, "Slow Multiplier (Ctrl / Alt)", &g_SlowMultiplier, 0.05f, 1.0f, 2, "x",
            "Precision speed multiplier applied when holding Ctrl or Alt.");
        curY += 24.0f;

        ImGui_SeparatorText(dev, contentX, curY, contentW, "Mouse Look & Orientation");
        curY += 20.0f;

        ImGui_SliderFloat(dev, 4, contentX, curY, contentW, "Mouse Sensitivity", &g_MouseSensitivity, 0.0005f, 0.0200f, 4, "",
            "Radians per pixel of mouse movement during FreeCam look.");
        curY += 23.0f;

        ImGui_Checkbox(dev, contentX, curY, "Invert Vertical Look (Pitch)", "(immediate)", &g_InvertPitch,
            "Inverts vertical mouse and right-stick pitch rotation.");
        curY += 22.0f;

        ImGui_Checkbox(dev, contentX, curY, "Invert Horizontal Look (Yaw)", "(immediate)", &g_InvertYaw,
            "Inverts horizontal mouse and right-stick yaw rotation.\nDefault (off) turns right when moving mouse right.");
        curY += 24.0f;

        ImGui_SeparatorText(dev, contentX, curY, contentW, "Field of View & Actor Teleport");
        curY += 20.0f;

        ImGui_Checkbox(dev, contentX, curY, "Override Field of View", "(immediate)", &g_CustomFOVEnabled,
            "Overrides Deadly Premonition's camera FOV while FreeCam is active.");
        curY += 22.0f;

        if (ImGui_SliderFloat(dev, 5, contentX, curY, contentW, "Field of View", &g_TargetFOV_Deg, 30.0f, 120.0f, 1, " deg",
                "Vertical Field of View in degrees (65 deg matches native gameplay).")) {
            g_CustomFOVEnabled = 1;
        }
        curY += 25.0f;

        float tpW1 = (float)MeasureTextWidth("Teleport York to Camera") + 20.0f;
        if (ImGui_Button(dev, contentX, curY, tpW1, 21.0f, "Teleport York to Camera",
                "Moves York's world position (+0x58) and collision center (+0x6C) to the current FreeCam position.")) {
            if (TeleportYorkToCamera()) {
                lstrcpyA(g_StatusMessage, "Teleported York to current camera position.");
            } else {
                lstrcpyA(g_StatusMessage, "Could not resolve York actor (ensure a gameplay level is loaded).");
            }
        }

        float tpW2 = (float)MeasureTextWidth("Snap Camera to York") + 20.0f;
        if (ImGui_Button(dev, contentX + tpW1 + 8.0f, curY, tpW2, 21.0f, "Snap Camera to York",
                "Repositions the FreeCam directly behind York.")) {
            if (ResetCameraToPlayer()) {
                lstrcpyA(g_StatusMessage, "Snapped camera behind York.");
            } else {
                lstrcpyA(g_StatusMessage, "Could not resolve York actor.");
            }
        }

    } else if (g_SelectedTab == 1) {
        // --- HUD TAB ---
        ImGui_SeparatorText(dev, contentX, curY, contentW, "In-Game 2D HUD Visibility");
        curY += 20.0f;

        if (ImGui_Checkbox(dev, contentX, curY, "Hide In-Game 2D HUD", "(F4 immediate)", &g_HUDHidden,
                "Toggles the 2D HUD (minimap, health, stamina, clock, ammo, crosshair).\nHotkey: F4")) {
            ApplyHUDState(g_HUDHidden);
            lstrcpyA(g_StatusMessage, g_HUDHidden ? "2D HUD: hidden (live)." : "2D HUD: visible (live).");
        }
        curY += 24.0f;

        DrawTextString(dev, contentX, curY,
            "Suppresses DP's 2D HUD pass (0x005FFE80) and in-game Radar pass (0x006541B0)\n"
            "while cleanly preserving pause-menu and overworld MapFix navigation.",
            IM_COL_TEXT_DISABLED);
        curY += 40.0f;

        if (ImGui_CollapsingHeader(dev, contentX, curY, contentW, "HUD Elements Controlled", &g_HudHeaderOpen)) {
            curY += 26.0f;
            ImGui_BulletText(dev, contentX, curY, "Top-left Minimap & Compass Ring", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Health, Stamina / Pulse, and Hunger Gauges", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Digital Clock, Weather Icon, Ammo Counter & Reticle", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Preserves Pause Menu & MapFix 64-tile Overworld Map integrity", IM_COL_TEXT);
        }

    } else if (g_SelectedTab == 2) {
        // --- GAMEPAD TAB (Modeled on ZachFix's DrawGamepadTab) ---
        ImGui_SeparatorText(dev, contentX, curY, contentW, "XInput Controller Backend");
        curY += 20.0f;

        DrawTextString(dev, contentX, curY,
            g_GamepadConnected
                ? "Controller 0 Status: Connected (XInput active)"
                : "Controller 0 Status: Not detected (Press any button on XInput controller)",
            g_GamepadConnected ? IM_COL_STATUS_TEXT : IM_COL_TEXT_DISABLED);
        curY += 22.0f;

        if (g_GamepadConnected) {
            char gpBuf[128];
            wsprintfA(gpBuf, "Left Stick: (%d, %d)   Right Stick: (%d, %d)   LT: %u   RT: %u",
                (int)g_LastGamepadState.sThumbLX, (int)g_LastGamepadState.sThumbLY,
                (int)g_LastGamepadState.sThumbRX, (int)g_LastGamepadState.sThumbRY,
                (unsigned)g_LastGamepadState.bLeftTrigger, (unsigned)g_LastGamepadState.bRightTrigger);
            DrawTextString(dev, contentX, curY, gpBuf, IM_COL_TEXT);
            curY += 22.0f;
        }

        if (ImGui_CollapsingHeader(dev, contentX, curY, contentW, "Gamepad Flight Controls", &g_GpHeaderOpen)) {
            curY += 26.0f;
            ImGui_BulletText(dev, contentX, curY, "Left Thumbstick    : Fly Forward / Backward & Strafe Left / Right", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Right Thumbstick   : 360-Degree Camera Yaw & Pitch Look", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "RT / RB (Shoulder) : Ascend (+Y World Up)", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "LT / LB (Shoulder) : Descend (-Y World Down)", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "L3 (Left Stick In) : Hold for Fast Flight Boost Multiplier", IM_COL_TEXT);
        }

    } else if (g_SelectedTab == 3) {
        // --- DIAGNOSTICS TAB (Modeled on ZachFix's DrawDiagnosticsTab) ---
        ImGui_SeparatorText(dev, contentX, curY, contentW, "Live Camera Telemetry (0x00BD9648)");
        curY += 20.0f;

        char diagLine[128];
        char sx[24], sy[24], sz[24];
        FormatFloat(sx, sizeof(sx), g_CamPos.x, 2);
        FormatFloat(sy, sizeof(sy), g_CamPos.y, 2);
        FormatFloat(sz, sizeof(sz), g_CamPos.z, 2);
        wsprintfA(diagLine, "Camera Eye Position : (%s, %s, %s)", sx, sy, sz);
        DrawTextString(dev, contentX, curY, diagLine, IM_COL_TEXT);
        curY += 18.0f;

        FormatFloat(sx, sizeof(sx), RAD2DEG(g_CamYaw), 1);
        FormatFloat(sy, sizeof(sy), RAD2DEG(g_CamPitch), 1);
        FormatFloat(sz, sizeof(sz), g_TargetFOV_Deg, 1);
        wsprintfA(diagLine, "Yaw / Pitch / FOV   : Yaw %s deg | Pitch %s deg | FOV %s deg", sx, sy, sz);
        DrawTextString(dev, contentX, curY, diagLine, IM_COL_TEXT);
        curY += 18.0f;

        void* playerActor = (void*)g_CachedPlayerActor;
        wsprintfA(diagLine, "York Actor Pointer  : 0x%08X", (unsigned int)(uintptr_t)playerActor);
        DrawTextString(dev, contentX, curY, diagLine, IM_COL_TEXT_DISABLED);
        curY += 24.0f;

        if (ImGui_CollapsingHeader(dev, contentX, curY, contentW, "Keyboard & Mouse Shortcuts", &g_DiagHeaderOpen)) {
            curY += 26.0f;
            ImGui_BulletText(dev, contentX, curY, "F9                 : Open / Close FreeCam Settings UI", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "F5                 : Toggle Free Camera Flight On / Off", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "F4                 : Toggle In-Game 2D HUD On / Off", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "W / S              : Fly Forward / Backward", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "A / D              : Strafe Left / Right (corrected for DP's mirrored proj._11)", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "E / Space          : Fly Upward (+Y)", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Q / C              : Fly Downward (-Y)", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Shift / Ctrl       : Hold for Fast Boost (3x) / Precision Slow (0.25x)", IM_COL_TEXT);
            curY += 18.0f;
            ImGui_BulletText(dev, contentX, curY, "Right Mouse (Hold) : Rotate & fly camera while Settings UI is open", IM_COL_TEXT);
        }

    } else if (g_SelectedTab == 4) {
        // --- ABOUT TAB (Modeled on ZachFix's DrawAboutTab) ---
        ImGui_SeparatorText(dev, contentX, curY, contentW, "FreeCam & HUD Hide ASI Mod");
        curY += 20.0f;

        DrawTextString(dev, contentX, curY,
            "Deadly Premonition: The Director's Cut - FreeCam v1.2.2\n"
            "In-game 6-DOF Free Camera, 2D HUD toggle, and York actor teleportation.",
            IM_COL_TEXT);
        curY += 40.0f;
    }

    // 8. Bottom-Right Dear ImGui Resize Grip Triangle
    int gripHover = (g_MouseX >= mx + mw - 16.0f && g_MouseX <= mx + mw &&
                     g_MouseY >= my + mh - 16.0f && g_MouseY <= my + mh);
    DWORD gripCol = (gripHover || g_ResizingWindow) ? IM_COL_RESIZE_GRIP_HOVER : IM_COL_RESIZE_GRIP;
    DrawTriangle(dev, mx + mw - 2.0f, my + mh - 15.0f,
                      mx + mw - 2.0f, my + mh - 2.0f,
                      mx + mw - 15.0f, my + mh - 2.0f, gripCol);

    // 9. Floating Tooltip (ImGui::SetTooltip)
    if (g_HoveredTooltip) {
        DrawTooltipPopup(dev, g_HoveredTooltip);
    }

    // 10. Software Cursor (io.MouseDrawCursor)
    DrawImGuiCursor(dev, (float)g_MouseX, (float)g_MouseY);

    FlushVerts(dev);
}

// -----------------------------------------------------------------------------
// D3D9 Hooks: EndScene, Reset, Present Detours
// -----------------------------------------------------------------------------
typedef HRESULT (WINAPI *D3D9PresentFn)(IDirect3DDevice9* dev, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
typedef HRESULT (WINAPI *D3D9EndSceneFn)(IDirect3DDevice9* dev);
typedef HRESULT (WINAPI *D3D9ResetFn)(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pParams);

static D3D9PresentFn  g_OrigPresent  = NULL;
static D3D9EndSceneFn g_OrigEndScene = NULL;
static D3D9ResetFn    g_OrigReset    = NULL;

static void RenderOverlayWithStateBlock(IDirect3DDevice9* dev) {
    if (!dev || !g_MenuOpen) return;
    IDirect3DStateBlock9* pStateBlock = NULL;
    if (SUCCEEDED(dev->lpVtbl->CreateStateBlock(dev, D3DSBT_ALL, &pStateBlock)) && pStateBlock) {
        pStateBlock->lpVtbl->Capture(pStateBlock);
        RenderModUI(dev);
        pStateBlock->lpVtbl->Apply(pStateBlock);
        pStateBlock->lpVtbl->Release(pStateBlock);
    } else {
        RenderModUI(dev);
    }
}

static HRESULT WINAPI HookEndScene(IDirect3DDevice9* dev) {
    if (dev) {
        UpdateUiInputOncePerFrame();
        if (g_MenuOpen && !g_UiRenderedFrame) {
            g_UiRenderedFrame = 1;
            RenderOverlayWithStateBlock(dev);
        }
    }
    return g_OrigEndScene(dev);
}

static HRESULT WINAPI HookPresent(IDirect3DDevice9* dev, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    if (dev) {
        UpdateUiInputOncePerFrame();
    }
    HRESULT hr = g_OrigPresent(dev, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    g_UiUpdatedFrame = 0;
    g_UiRenderedFrame = 0;
    return hr;
}

static HRESULT WINAPI HookReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pParams) {
    if (g_FontTexture) {
        g_FontTexture->lpVtbl->Release(g_FontTexture);
        g_FontTexture = NULL;
    }
    g_FontAtlasReady = 0;
    return g_OrigReset(dev, pParams);
}

// -----------------------------------------------------------------------------
// Window Subclassing (WndProc) - Matches ZachFix input isolation
// -----------------------------------------------------------------------------
LRESULT CALLBACK HookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (wParam == (WPARAM)g_MenuKey || wParam == (WPARAM)g_FreeCamKey || wParam == (WPARAM)g_HudHideKey) {
                return 0;
            }
            if (g_MenuOpen) {
                return 1;
            }
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

        case WM_MOUSEMOVE: {
            g_MouseX = (short)LOWORD(lParam);
            g_MouseY = (short)HIWORD(lParam);
            if (g_MenuOpen) return 1;
            break;
        }

        case WM_LBUTTONDOWN: {
            g_MouseDown = 1;
            g_MouseClicked = 1;
            if (g_MenuOpen) return 1;
            break;
        }

        case WM_LBUTTONUP: {
            g_MouseDown = 0;
            if (g_MenuOpen) return 1;
            break;
        }

        case WM_RBUTTONDOWN: {
            g_RightDown = 1;
            if (g_MenuOpen) return 1;
            break;
        }

        case WM_RBUTTONUP: {
            g_RightDown = 0;
            if (g_MenuOpen) return 1;
            break;
        }

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL: {
            if (g_MenuOpen) return 1;
            break;
        }
    }

    if (g_OrigWndProc) {
        return CallWindowProcW(g_OrigWndProc, hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// -----------------------------------------------------------------------------
// D3D9 Device Hook Installation Worker
// -----------------------------------------------------------------------------
static DWORD WINAPI HookThread(LPVOID lpParam) {
    (void)lpParam;

    // 1. Wait for IDirect3DDevice9 pointer at 0x01480984
    IDirect3DDevice9** ppDevice = (IDirect3DDevice9**)ADDR_D3D9_DEVICE_PTR;
    while (!ppDevice || !(*ppDevice)) {
        Sleep(50);
    }
    IDirect3DDevice9* dev = *ppDevice;

    // 2. Initialize XInput gamepad backend
    InitXInput();

    // 3. Obtain window handle
    D3DDEVICE_CREATION_PARAMETERS cp;
    MemSet(&cp, 0, sizeof(cp));
    if (dev->lpVtbl->GetCreationParameters(dev, &cp) == D3D_OK && cp.hFocusWindow) {
        g_GameHWnd = cp.hFocusWindow;
        g_OrigWndProc = (WNDPROC)SetWindowLongPtrW(g_GameHWnd, GWLP_WNDPROC, (LONG_PTR)HookWndProc);
    }

    // 4. Hook Reset (slot 16), Present (slot 17), EndScene (slot 42) in device VTable
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

    // 5. Hook DP.exe IAT entries for SetCursorPos, GetCursorPos and GetAsyncKeyState
    if (VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_Orig_DP_SetCursorPos = *(SetCursorPosFn*)ADDR_IAT_SETCURSORPOS;
        *(SetCursorPosFn*)ADDR_IAT_SETCURSORPOS = Hook_DP_SetCursorPos;
        VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
    }

    if (VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_Orig_DP_GetCursorPos = *(GetCursorPosFn*)ADDR_IAT_GETCURSORPOS;
        *(GetCursorPosFn*)ADDR_IAT_GETCURSORPOS = Hook_DP_GetCursorPos;
        VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
    }

    if (VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_Orig_DP_GetAsyncKeyState = *(GetAsyncKeyStateFn*)ADDR_IAT_GETASYNCKEYSTATE;
        *(GetAsyncKeyStateFn*)ADDR_IAT_GETASYNCKEYSTATE = Hook_DP_GetAsyncKeyState;
        VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), oldProtect, &oldProtect);
    }

    // 6. Hook Camera CalcViewProj call at 0x00401BD1
    unsigned char* pHookSite = (unsigned char*)ADDR_CAMERA_HOOK_SITE;
    if (VirtualProtect(pHookSite, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pHookSite = 0xE8; // CALL rel32
        int32_t rel = (int32_t)((uintptr_t)CameraHookAsm - ((uintptr_t)pHookSite + 5));
        *(int32_t*)(pHookSite + 1) = rel;
        VirtualProtect(pHookSite, 5, oldProtect, &oldProtect);
    }

    // 7. Hook Radar / Minimap Render routine at 0x006541B0
    unsigned char* pRadarHookSite = (unsigned char*)ADDR_RADAR_DRAW_FUNC;
    if (VirtualProtect(pRadarHookSite, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pRadarHookSite = 0xE9; // JMP rel32
        int32_t rel = (int32_t)((uintptr_t)RadarHookAsm - ((uintptr_t)pRadarHookSite + 5));
        *(int32_t*)(pRadarHookSite + 1) = rel;
        pRadarHookSite[5] = 0x90; // NOP
        VirtualProtect(pRadarHookSite, 6, oldProtect, &oldProtect);
    }

    // 8. Hook Car Navi / Vehicle Radar Render routine at 0x005F5DF0
    unsigned char* pCarNaviHookSite = (unsigned char*)ADDR_CARNAVI_DRAW_FUNC;
    if (VirtualProtect(pCarNaviHookSite, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pCarNaviHookSite = 0xE9; // JMP rel32
        int32_t rel = (int32_t)((uintptr_t)CarNaviHookAsm - ((uintptr_t)pCarNaviHookSite + 5));
        *(int32_t*)(pCarNaviHookSite + 1) = rel;
        pCarNaviHookSite[5] = 0x90; // NOP
        VirtualProtect(pCarNaviHookSite, 6, oldProtect, &oldProtect);
    }

    // 9. Apply initial HUD state if configured
    if (g_HUDHidden) {
        ApplyHUDState(1);
    }

    return 0;
}

// -----------------------------------------------------------------------------
// DLL Entry Point
// -----------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
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
        unsigned char* pCarNaviHookSite = (unsigned char*)ADDR_CARNAVI_DRAW_FUNC;
        if (VirtualProtect(pCarNaviHookSite, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            static const unsigned char origCarNavi[6] = { 0x81, 0xEC, 0x74, 0x06, 0x00, 0x00 };
            memcpy(pCarNaviHookSite, origCarNavi, 6);
            VirtualProtect(pCarNaviHookSite, 6, oldProtect, &oldProtect);
        }
        if (g_Orig_DP_SetCursorPos) {
            DWORD oldProtect;
            if (VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *(SetCursorPosFn*)ADDR_IAT_SETCURSORPOS = g_Orig_DP_SetCursorPos;
                VirtualProtect((void*)ADDR_IAT_SETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        if (g_Orig_DP_GetCursorPos) {
            DWORD oldProtect;
            if (VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *(GetCursorPosFn*)ADDR_IAT_GETCURSORPOS = g_Orig_DP_GetCursorPos;
                VirtualProtect((void*)ADDR_IAT_GETCURSORPOS, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        if (g_Orig_DP_GetAsyncKeyState) {
            DWORD oldProtect;
            if (VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                *(GetAsyncKeyStateFn*)ADDR_IAT_GETASYNCKEYSTATE = g_Orig_DP_GetAsyncKeyState;
                VirtualProtect((void*)ADDR_IAT_GETASYNCKEYSTATE, sizeof(void*), oldProtect, &oldProtect);
            }
        }
        if (g_FontTexture) {
            g_FontTexture->lpVtbl->Release(g_FontTexture);
            g_FontTexture = NULL;
        }
        if (g_GameHWnd && g_OrigWndProc) {
            SetWindowLongPtrW(g_GameHWnd, GWLP_WNDPROC, (LONG_PTR)g_OrigWndProc);
        }
    }
    return TRUE;
}
