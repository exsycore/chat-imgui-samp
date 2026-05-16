#pragma once
// ============================================================
//  gui.h
//  Modern ImGui replacement for the SA-MP default chat panel.
// ============================================================

#include <windows.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"

#ifndef SAMP_VERSION_NS
    #define SAMP_VERSION_NS sampapi::v037r1
#endif

#include "sampapi/CChat.h"
#include "sampapi/CInput.h"
#include "sampapi/CNetGame.h"
#include "sampapi/CGame.h"
#include "sampapi/CLocalPlayer.h"
#include "sampapi/Commands.h"
#include "tis620.h"

// ============================================================
//  Chat entry snapshot (decoded, UTF-8)
// ============================================================
struct ChatLine {
    std::string  prefix;
    std::string  text;
    std::string  timestamp;
    ImVec4       prefixColor;
    ImVec4       textColor;
    int          type;
};

// ============================================================
//  Color helpers
// ============================================================
namespace ChatColor {
    inline ImVec4 from_samp(DWORD c, float forced_alpha = -1.f) {
        float a = (forced_alpha >= 0.f) ? forced_alpha : ((c >> 24) & 0xFF) / 255.f;
        float r = ((c >> 16) & 0xFF) / 255.f;
        float g = ((c >>  8) & 0xFF) / 255.f;
        float b = ( c        & 0xFF) / 255.f;
        if (a < 0.01f) a = 1.f;
        return ImVec4(r, g, b, a);
    }

    inline bool parse_color_tag(const char*& p, ImVec4& out) {
        if (*p != '{') return false;
        const char* e = p + 1;
        int hex = 0, digits = 0;
        while (*e && *e != '}' && digits < 6) {
            char c = *e++;
            hex <<= 4;
            if (c >= '0' && c <= '9') hex |= c - '0';
            else if (c >= 'A' && c <= 'F') hex |= c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') hex |= c - 'a' + 10;
            else return false;
            digits++;
        }
        if (*e != '}' || digits != 6) return false;
        float r = ((hex >> 16) & 0xFF) / 255.f;
        float g = ((hex >>  8) & 0xFF) / 255.f;
        float b = ( hex        & 0xFF) / 255.f;
        out = ImVec4(r, g, b, 1.f);
        p = e + 1;
        return true;
    }
}

// ============================================================
//  Fade state machine
// ============================================================
enum class FadeState {
    Idle,
    FadingIn,
    Holding,
    FadingOut,
};

struct FadeSettings {
    float minAlpha        = 0.0f;
    float maxAlpha        = 0.85f;
    float fadeInSpeed     = 6.0f;
    float fadeOutSpeed    = 2.0f;
    float holdSeconds     = 2.0f;
    float chatOpenSeconds = 9999.f;
};

// ============================================================
//  Main chat manager
// ============================================================
class SAMPChatImGui {
public:
    // ---- Fixed settings ----
    FadeSettings fade;

    float fontSize         = 21.f;
    float widthPct         = 55.f;
    float heightPct        = 28.f;
    float windowX          = 20.f;
    float windowY          = 20.f;

    // ---- Public API ----
    void Init(HWND hwnd, IDirect3DDevice9* device);
    void Shutdown();
    void Tick(IDirect3DDevice9* device);

    void OnNewEntry(int type, const char* szText, const char* szPrefix,
                    DWORD textColor, DWORD prefixColor);

    void OnInputOpen();
    void OnInputClose();

    bool IsInputOpen() const { return m_inputOpen; }
    void ScrollChat(int lines) { m_scrollDelta += lines; }

    static SAMPChatImGui& Get() {
        static SAMPChatImGui inst;
        return inst;
    }

private:
    SAMPChatImGui() = default;

    void BuildFonts();
    void ApplyTheme();
    void RenderChatWindow();
    void RenderInputWindow();
    void RenderColoredLine(const ChatLine& line, int rowIdx);
    void UpdateFade(float dt);
    void SnapshotHistory();

    std::deque<ChatLine>  m_lines;
    static constexpr int  MAX_LINES = 100;

    bool          m_initialized = false;
    bool          m_inputOpen   = false;
    bool          m_scrollToBot = false;
    bool          m_focusInput  = false;
    char          m_inputBuf[256] = {0};

    FadeState     m_fadeState   = FadeState::Idle;
    float         m_alpha       = 0.f;
    float         m_holdTimer   = 0.f;
    DWORD64       m_lastTick    = 0;

    HWND          m_hwnd        = nullptr;
    bool          m_fontsBuilt  = false;
    ImFont*       m_chatFont    = nullptr;

    // Command history recall
    int           m_nHistoryIndex = -1;
    char          m_szSavedInput[256] = {0};

    // Chat scroll
    int           m_scrollDelta = 0;
};
