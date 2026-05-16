// ============================================================
//  dllmain.cpp
//  SA-MP Modern ImGui Chat — DLL entry point
// ============================================================

#define SAMP_CHAT_IMGUI_IMPL
#define SAMP_VERSION_NS sampapi::v037r1

#define _WIN32_WINNT 0x0601
#define WINVER       0x0601

#include <windows.h>
#include <winuser.h>
#include <d3d9.h>
#include <MinHook.h>

#ifndef GWL_WNDPROC
#   define GWL_WNDPROC (-4)
#endif

#include "sampapi/CNetGame.h"
#include "sampapi/CInput.h"
#include "sampapi/CChat.h"
#include "sampapi/CGame.h"
#include "sampapi/Commands.h"
#include "gui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
//  Types & Structures
// ============================================================
struct SAMPOffsets {
    DWORD INPUT_PTR;
    DWORD GAME_PTR;
    DWORD ADDENTRY;
    DWORD ENABLE_BOX;
    DWORD DISABLE_BOX;
    DWORD ENABLE_BOX_JMP;
    DWORD DISABLE_BOX_JMP;
    DWORD DISABLE_ORIGINAL; // CChat::Render
    DWORD KEYPRESS_HANDLER;
    DWORD CHARINPUT_HANDLER;
};

using PFN_Present  = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
using PFN_Reset    = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using PFN_AddEntry       = int(__fastcall*)(void*,void*,int,const char*,const char*,DWORD,DWORD);
using PFN_KeyPressHandler = BOOL(__cdecl*)(unsigned int);
using PFN_CharInputHandler = BOOL(__cdecl*)(unsigned int);

// ============================================================
//  Globals
// ============================================================
PFN_Present         g_Present         = nullptr;
PFN_Reset           g_Reset           = nullptr;
PFN_AddEntry        g_AddEntry        = nullptr;
PFN_KeyPressHandler g_KeyPressHandler = nullptr;
PFN_CharInputHandler g_CharInputHandler = nullptr;

WNDPROC      g_OrigWndProc = nullptr;
DWORD        g_dwSAMP      = 0;
DWORD        g_jmpEnableBox  = 0;
DWORD        g_jmpDisableBox = 0;

// ============================================================
//  Memory Patching
// ============================================================
static void SafeNopCallSite(DWORD addr) {
    if (!addr) return;
    BYTE first = *reinterpret_cast<BYTE*>(addr);
    if (first == 0xE8 || first == 0xFF) {
        constexpr SIZE_T PATCH_LEN = 5;
        DWORD oldProt;
        if (VirtualProtect(reinterpret_cast<LPVOID>(addr), PATCH_LEN, PAGE_EXECUTE_READWRITE, &oldProt)) {
            memset(reinterpret_cast<LPVOID>(addr), 0x90, PATCH_LEN);
            VirtualProtect(reinterpret_cast<LPVOID>(addr), PATCH_LEN, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPVOID>(addr), PATCH_LEN);
        }
    } else {
        DWORD oldProt;
        if (VirtualProtect(reinterpret_cast<LPVOID>(addr), 1, PAGE_EXECUTE_READWRITE, &oldProt)) {
            *reinterpret_cast<BYTE*>(addr) = 0xC3; // RET
            VirtualProtect(reinterpret_cast<LPVOID>(addr), 1, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPVOID>(addr), 1);
        }
    }
}

// ============================================================
//  Hooks
// ============================================================
LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& chat = SAMPChatImGui::Get();

    // Close input when window loses focus (minimize, alt-tab, etc.)
    if (msg == WM_KILLFOCUS && chat.IsInputOpen()) {
        chat.OnInputClose();
        namespace samp = SAMP_VERSION_NS;
        auto* pInput = samp::RefInputBox();
        if (pInput) {
            pInput->Close();
            memset(pInput->m_szInput, 0, sizeof(pInput->m_szInput));
        }
    }

    // Input closed: intercept 'T' to open ImGui input instead of SAMP native
    if (!chat.IsInputOpen()) {
        if (msg == WM_KEYDOWN && wParam == 'T' &&
            !(GetKeyState(VK_CONTROL) & 0x8000) &&
            !(GetKeyState(VK_MENU)    & 0x8000)) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            chat.OnInputOpen();
            return TRUE;
        }
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        return CallWindowProcA(g_OrigWndProc, hWnd, msg, wParam, lParam);
    }

    // Input open: let ImGui handle keyboard, block from SAMP
    if (msg == WM_KEYDOWN || msg == WM_CHAR ||
        msg == WM_SYSKEYDOWN || msg == WM_SYSCHAR) {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        return TRUE;
    }

    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    return CallWindowProcA(g_OrigWndProc, hWnd, msg, wParam, lParam);
}

int __fastcall Hooked_AddEntry(void* pThis, void* EDX, int nType, const char* szText, const char* szPrefix, DWORD textColor, DWORD prefixColor) {
    SAMPChatImGui::Get().OnNewEntry(nType, szText, szPrefix, textColor, prefixColor);
    return g_AddEntry(pThis, EDX, nType, szText, szPrefix, textColor, prefixColor);
}

void __declspec(naked) HOOK_EnableInputBox() {
    __asm pushad
    SAMPChatImGui::Get().OnInputOpen();
    __asm popad
    __asm mov dword ptr [esi + 0x14E0], 1
    __asm jmp [g_jmpEnableBox]
}

void __declspec(naked) HOOK_DisableInputBox() {
    __asm pushad
    SAMPChatImGui::Get().OnInputClose();
    __asm popad
    __asm mov dword ptr [esi + 0x14E0], 0
    __asm jmp [g_jmpDisableBox]
}

// ---- InputHandler hooks: intercept keys at SAMP handler level ----

BOOL __cdecl Hooked_KeyPressHandler(unsigned int nKey) {
    auto& chat = SAMPChatImGui::Get();

    if (chat.IsInputOpen()) {
        // ImGui input open — block ALL keys from SAMP native input
        return TRUE;
    }

    // Input closed: intercept keys to open ImGui input or scroll chat
    bool noMod = !(GetKeyState(VK_CONTROL) & 0x8000) &&
                 !(GetKeyState(VK_MENU)    & 0x8000);

    // T or F6: open ImGui input
    if ((nKey == 'T' || nKey == VK_F6) && noMod) {
        chat.OnInputOpen();
        return TRUE;
    }

    // Page Up / Page Down: scroll chat history
    if (nKey == VK_PRIOR) { chat.ScrollChat(-5); return TRUE; }
    if (nKey == VK_NEXT)  { chat.ScrollChat( 5); return TRUE; }

    return g_KeyPressHandler(nKey);
}

BOOL __cdecl Hooked_CharInputHandler(unsigned int nChar) {
    if (SAMPChatImGui::Get().IsInputOpen()) {
        // ImGui input open — block all character input from SAMP
        return TRUE;
    }
    return g_CharInputHandler(nChar);
}

HRESULT STDMETHODCALLTYPE Hooked_Present(IDirect3DDevice9* pDev, const RECT* pSrc, const RECT* pDst, HWND hWnd, const RGNDATA* pDirty) {
    static bool once = false;
    if (!once) {
        HWND gta = **reinterpret_cast<HWND**>(0xC17054);
        g_OrigWndProc = reinterpret_cast<WNDPROC>(SetWindowLongA(gta, GWL_WNDPROC, (LONG)WndProcHook));
        SAMPChatImGui::Get().Init(gta, pDev);
        once = true;
    }
    SAMPChatImGui::Get().Tick(pDev);
    return g_Present(pDev, pSrc, pDst, hWnd, pDirty);
}

HRESULT STDMETHODCALLTYPE Hooked_Reset(IDirect3DDevice9* pDev, D3DPRESENT_PARAMETERS* pp) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    return g_Reset(pDev, pp);
}

// ============================================================
//  Initialization
// ============================================================
static void* GetVTableFunc(int idx) {
    char buf[MAX_PATH];
    GetSystemDirectoryA(buf, MAX_PATH);
    strcat_s(buf, "\\d3d9.dll");
    DWORD base = (DWORD)LoadLibraryA(buf);
    DWORD end  = base + 0x128000;
    for (DWORD p = base; p < end; ++p) {
        if (*(WORD*)(p+0x00) == 0x06C7 && *(WORD*)(p+0x06) == 0x8689 && *(WORD*)(p+0x0C) == 0x8689) {
            PDWORD vt = *(PDWORD*)(p + 2);
            return reinterpret_cast<void*>(vt[idx]);
        }
    }
    return nullptr;
}

static bool GetOffsets(DWORD ep, SAMPOffsets& o) {
    switch (ep) {
    case 0x31DF13:  // 0.3.7-R1
        o = { 0x21A0E8, 0x21A10C, 0x64010, 0x658C7, 0x6591C, 0x658D1, 0x65926, 0x63D70, 0x5D850, 0x5DA80 };
        return true;
    case 0xCC4D0:   // 0.3.7-R3-1
        o = { 0x26E8CC, 0x26E8F4, 0x67460, 0x68DF7, 0x68E4C, 0x68E01, 0x68E56, 0x673B0, 0x60BF0, 0x60E20 };
        return true;
    default: return false;
    }
}

static void InstallHooks(const SAMPOffsets& o) {
    SafeNopCallSite(g_dwSAMP + o.DISABLE_ORIGINAL);

    MH_CreateHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.ADDENTRY), Hooked_AddEntry, reinterpret_cast<LPVOID*>(&g_AddEntry));
    MH_EnableHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.ADDENTRY));

    g_jmpEnableBox  = g_dwSAMP + o.ENABLE_BOX_JMP;
    g_jmpDisableBox = g_dwSAMP + o.DISABLE_BOX_JMP;
    MH_CreateHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.ENABLE_BOX), HOOK_EnableInputBox, nullptr);
    MH_EnableHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.ENABLE_BOX));
    MH_CreateHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.DISABLE_BOX), HOOK_DisableInputBox, nullptr);
    MH_EnableHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.DISABLE_BOX));

    // Hook InputHandler to intercept 'T' at SAMP handler level
    MH_CreateHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.KEYPRESS_HANDLER), Hooked_KeyPressHandler, reinterpret_cast<LPVOID*>(&g_KeyPressHandler));
    MH_EnableHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.KEYPRESS_HANDLER));
    MH_CreateHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.CHARINPUT_HANDLER), Hooked_CharInputHandler, reinterpret_cast<LPVOID*>(&g_CharInputHandler));
    MH_EnableHook(reinterpret_cast<LPVOID>(g_dwSAMP + o.CHARINPUT_HANDLER));

    MH_CreateHook(GetVTableFunc(17), Hooked_Present, reinterpret_cast<LPVOID*>(&g_Present));
    MH_EnableHook(GetVTableFunc(17));
    MH_CreateHook(GetVTableFunc(16), Hooked_Reset, reinterpret_cast<LPVOID*>(&g_Reset));
    MH_EnableHook(GetVTableFunc(16));
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_dwSAMP = (DWORD)GetModuleHandleA("samp.dll");
    if (!g_dwSAMP) return FALSE;
    auto* nth = reinterpret_cast<IMAGE_NT_HEADERS*>(g_dwSAMP + reinterpret_cast<IMAGE_DOS_HEADER*>(g_dwSAMP)->e_lfanew);
    DWORD ep = nth->OptionalHeader.AddressOfEntryPoint;
    SAMPOffsets offsets{};
    if (!GetOffsets(ep, offsets)) return FALSE;
    MH_Initialize();
    InstallHooks(offsets);
    return TRUE;
}
