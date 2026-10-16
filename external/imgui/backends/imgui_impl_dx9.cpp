// Dear ImGui DX9 Backend Implementation (imgui_impl_dx9.cpp)
#include "imgui_impl_dx9.h"
#include <d3d9.h>
#include <stdlib.h>

struct ImGui_ImplDX9_Data {
    IDirect3DDevice9*       pd3dDevice;
    IDirect3DTexture9*      pFontTexture;

    ImGui_ImplDX9_Data() : pd3dDevice(nullptr), pFontTexture(nullptr) {}
};

static ImGui_ImplDX9_Data* ImGui_ImplDX9_GetBackendData() {
    static ImGui_ImplDX9_Data s_data;
    return &s_data;
}

bool ImGui_ImplDX9_Init(IDirect3DDevice9* device) {
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    bd->pd3dDevice = device;
    if (device) device->AddRef();
    return ImGui_ImplDX9_CreateDeviceObjects();
}

void ImGui_ImplDX9_Shutdown() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    if (bd->pd3dDevice) {
        bd->pd3dDevice->Release();
        bd->pd3dDevice = nullptr;
    }
}

bool ImGui_ImplDX9_CreateDeviceObjects() {
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    if (!bd->pd3dDevice) return false;

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    if (bd->pFontTexture) {
        bd->pFontTexture->Release();
        bd->pFontTexture = nullptr;
    }

    if (bd->pd3dDevice->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &bd->pFontTexture, NULL) != D3D_OK) {
        return false;
    }

    D3DLOCKED_RECT lr;
    if (bd->pFontTexture->LockRect(0, &lr, NULL, 0) == D3D_OK) {
        for (int y = 0; y < height; y++) {
            memcpy((char*)lr.pBits + y * lr.Pitch, pixels + y * width * 4, width * 4);
        }
        bd->pFontTexture->UnlockRect(0);
    }

    io.Fonts->TexID = (ImTextureID)bd->pFontTexture;
    return true;
}

void ImGui_ImplDX9_InvalidateDeviceObjects() {
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    if (bd->pFontTexture) {
        bd->pFontTexture->Release();
        bd->pFontTexture = nullptr;
    }
}

void ImGui_ImplDX9_NewFrame() {
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    if (!bd->pFontTexture) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

struct Vertex2D {
    float x, y, z, rhw;
    DWORD color;
    float u, v;
};

#define D3DFVF_VERTEX2D (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

void ImGui_ImplDX9_RenderDrawData(ImDrawData* draw_data) {
    if (!draw_data || draw_data->CmdListsCount == 0) return;
    ImGui_ImplDX9_Data* bd = ImGui_ImplDX9_GetBackendData();
    IDirect3DDevice9* dev = bd->pd3dDevice;
    if (!dev) return;

    // Capture state block to preserve all game render states
    IDirect3DStateBlock9* stateBlock = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &stateBlock);
    if (stateBlock) stateBlock->Capture();

    // Setup viewport
    D3DVIEWPORT9 vp;
    vp.X = (DWORD)draw_data->DisplayPos.x;
    vp.Y = (DWORD)draw_data->DisplayPos.y;
    vp.Width = (DWORD)draw_data->DisplaySize.x;
    vp.Height = (DWORD)draw_data->DisplaySize.y;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    dev->SetViewport(&vp);

    // Setup 2D pixel-space render states
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);

    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

    dev->SetFVF(D3DFVF_VERTEX2D);
    dev->SetVertexShader(NULL);
    dev->SetPixelShader(NULL);
    dev->SetTexture(0, (IDirect3DBaseTexture9*)bd->pFontTexture);

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        ImDrawList* cmd_list = draw_data->CmdLists[n];
        if (!cmd_list || cmd_list->VtxCount == 0 || cmd_list->IdxCount == 0) continue;

        // Convert ImDrawVert to transformed D3D9 Vertex2D (-0.5f half-pixel offset)
        Vertex2D* vtx_dst = (Vertex2D*)malloc(cmd_list->VtxCount * sizeof(Vertex2D));
        if (!vtx_dst) continue;
        for (int i = 0; i < cmd_list->VtxCount; i++) {
            vtx_dst[i].x = cmd_list->VtxBuffer[i].pos.x - 0.5f;
            vtx_dst[i].y = cmd_list->VtxBuffer[i].pos.y - 0.5f;
            vtx_dst[i].z = 0.0f;
            vtx_dst[i].rhw = 1.0f;
            vtx_dst[i].color = cmd_list->VtxBuffer[i].col;
            vtx_dst[i].u = cmd_list->VtxBuffer[i].uv.x;
            vtx_dst[i].v = cmd_list->VtxBuffer[i].uv.y;
        }

        int idx_offset = 0;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdCount; cmd_i++) {
            ImDrawCmd& cmd = cmd_list->CmdBuffer[cmd_i];
            if (cmd.ElemCount > 0) {
                dev->DrawIndexedPrimitiveUP(
                    D3DPT_TRIANGLELIST,
                    0,
                    cmd_list->VtxCount,
                    cmd.ElemCount / 3,
                    &cmd_list->IdxBuffer[idx_offset],
                    D3DFMT_INDEX16,
                    vtx_dst,
                    sizeof(Vertex2D)
                );
                idx_offset += cmd.ElemCount;
            }
        }
        free(vtx_dst);
    }

    // Restore full state block
    if (stateBlock) {
        stateBlock->Apply();
        stateBlock->Release();
    }
}
