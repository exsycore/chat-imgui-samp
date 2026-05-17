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
#include <unordered_map>

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

    inline const std::unordered_map<std::string, ImVec4>* GetColorNameMap() {
        static std::unordered_map<std::string, ImVec4> colors = {
            {"WHITE", ImVec4(1.00f,1.00f,1.00f,1.f)}, {"BLACK", ImVec4(0.00f,0.00f,0.00f,1.f)},
            {"RED", ImVec4(1.00f,0.00f,0.00f,1.f)}, {"GREEN", ImVec4(0.00f,1.00f,0.00f,1.f)},
            {"BLUE", ImVec4(0.00f,0.00f,1.00f,1.f)}, {"YELLOW", ImVec4(1.00f,1.00f,0.00f,1.f)},
            {"CYAN", ImVec4(0.00f,1.00f,1.00f,1.f)}, {"MAGENTA", ImVec4(1.00f,0.00f,1.00f,1.f)},
            {"ORANGE", ImVec4(1.00f,0.65f,0.00f,1.f)}, {"PINK", ImVec4(1.00f,0.75f,0.80f,1.f)},
            {"PURPLE", ImVec4(0.50f,0.00f,0.50f,1.f)}, {"GRAY", ImVec4(0.50f,0.50f,0.50f,1.f)},
            {"GREY", ImVec4(0.50f,0.50f,0.50f,1.f)}, {"LIME", ImVec4(0.00f,1.00f,0.00f,1.f)},
            {"MAROON", ImVec4(0.50f,0.00f,0.00f,1.f)}, {"NAVY", ImVec4(0.00f,0.00f,0.50f,1.f)},
            {"OLIVE", ImVec4(0.50f,0.50f,0.00f,1.f)}, {"TEAL", ImVec4(0.00f,0.50f,0.50f,1.f)},
            {"SILVER", ImVec4(0.75f,0.75f,0.75f,1.f)}, {"GOLD", ImVec4(1.00f,0.84f,0.00f,1.f)},
            {"BROWN", ImVec4(0.65f,0.16f,0.16f,1.f)}, {"CORAL", ImVec4(1.00f,0.50f,0.31f,1.f)},
            {"CRIMSON", ImVec4(0.86f,0.08f,0.24f,1.f)}, {"INDIGO", ImVec4(0.29f,0.00f,0.51f,1.f)},
            {"IVORY", ImVec4(1.00f,1.00f,0.94f,1.f)}, {"KHAKI", ImVec4(0.94f,0.90f,0.55f,1.f)},
            {"LINEN", ImVec4(0.98f,0.94f,0.90f,1.f)}, {"MINT", ImVec4(0.74f,0.99f,0.79f,1.f)},
            {"PLUM", ImVec4(0.87f,0.63f,0.87f,1.f)}, {"SALMON", ImVec4(0.98f,0.50f,0.45f,1.f)},
            {"SIENNA", ImVec4(0.63f,0.32f,0.18f,1.f)}, {"SNOW", ImVec4(1.00f,0.98f,0.98f,1.f)},
            {"TAN", ImVec4(0.82f,0.71f,0.55f,1.f)}, {"THISTLE", ImVec4(0.85f,0.75f,0.85f,1.f)},
            {"TOMATO", ImVec4(1.00f,0.39f,0.28f,1.f)}, {"VIOLET", ImVec4(0.93f,0.51f,0.93f,1.f)},
            {"WHEAT", ImVec4(0.96f,0.87f,0.70f,1.f)}, {"AZURE", ImVec4(0.94f,1.00f,1.00f,1.f)},
            {"BEIGE", ImVec4(0.96f,0.96f,0.86f,1.f)}, {"CHARTREUSE", ImVec4(0.50f,1.00f,0.00f,1.f)},
            {"CORN", ImVec4(1.00f,0.97f,0.86f,1.f)}, {"CORNSILK", ImVec4(1.00f,0.97f,0.86f,1.f)},
            {"INDIANRED", ImVec4(0.80f,0.36f,0.36f,1.f)}, {"LAVENDER", ImVec4(0.90f,0.90f,0.98f,1.f)},
            {"LEMON", ImVec4(1.00f,0.97f,0.00f,1.f)}, {"LIGHTBLUE", ImVec4(0.68f,0.85f,0.90f,1.f)},
            {"LIGHTCORAL", ImVec4(0.94f,0.50f,0.50f,1.f)}, {"LIGHTCYAN", ImVec4(0.88f,1.00f,1.00f,1.f)},
            {"LIGHTGREEN", ImVec4(0.56f,0.93f,0.56f,1.f)}, {"LIGHTPINK", ImVec4(1.00f,0.71f,0.76f,1.f)},
            {"LIGHTYELLOW", ImVec4(1.00f,1.00f,0.88f,1.f)}, {"LIMEGREEN", ImVec4(0.20f,0.80f,0.20f,1.f)},
            {"MIDNIGHT", ImVec4(0.10f,0.10f,0.44f,1.f)}, {"MINTCREAM", ImVec4(0.96f,1.00f,0.98f,1.f)},
            {"MISTY", ImVec4(1.00f,0.89f,0.88f,1.f)}, {"MOCCASIN", ImVec4(1.00f,0.89f,0.71f,1.f)},
            {"NAVAJO", ImVec4(1.00f,0.87f,0.68f,1.f)}, {"OLDLACE", ImVec4(0.99f,0.96f,0.90f,1.f)},
            {"PALEGOLDENROD", ImVec4(0.93f,0.91f,0.67f,1.f)}, {"PALEGREEN", ImVec4(0.60f,0.98f,0.60f,1.f)},
            {"PALETURQUOISE", ImVec4(0.69f,0.93f,0.93f,1.f)}, {"PALEVIOLET", ImVec4(0.86f,0.44f,0.58f,1.f)},
            {"PAPAYA", ImVec4(1.00f,0.94f,0.84f,1.f)}, {"PEACHPUFF", ImVec4(1.00f,0.85f,0.73f,1.f)},
            {"PERU", ImVec4(0.80f,0.52f,0.25f,1.f)}, {"ROSYBROWN", ImVec4(0.74f,0.56f,0.56f,1.f)},
            {"ROYALBLUE", ImVec4(0.25f,0.41f,0.88f,1.f)}, {"SADDLEBROWN", ImVec4(0.55f,0.27f,0.07f,1.f)},
            {"SANDYBROWN", ImVec4(0.96f,0.64f,0.38f,1.f)}, {"SEAGREEN", ImVec4(0.18f,0.55f,0.34f,1.f)},
            {"SEASHELL", ImVec4(1.00f,0.96f,0.93f,1.f)}, {"SKYBLUE", ImVec4(0.53f,0.81f,0.92f,1.f)},
            {"SLATEBLUE", ImVec4(0.42f,0.35f,0.80f,1.f)}, {"SLATEGRAY", ImVec4(0.44f,0.50f,0.56f,1.f)},
            {"SPRINGGREEN", ImVec4(0.00f,1.00f,0.50f,1.f)}, {"STEELBLUE", ImVec4(0.27f,0.51f,0.71f,1.f)},
            {"TURQUOISE", ImVec4(0.25f,0.88f,0.82f,1.f)}, {"WHITE", ImVec4(1.00f,1.00f,1.00f,1.f)},
            {"YELLOWGREEN", ImVec4(0.60f,0.80f,0.20f,1.f)}, {"DARKBLUE", ImVec4(0.00f,0.00f,0.55f,1.f)},
            {"DARKCYAN", ImVec4(0.00f,0.55f,0.55f,1.f)}, {"DARKGOLDENROD", ImVec4(0.72f,0.53f,0.04f,1.f)},
            {"DARKGRAY", ImVec4(0.66f,0.66f,0.66f,1.f)}, {"DARKGREEN", ImVec4(0.00f,0.39f,0.00f,1.f)},
            {"DARKKHAKI", ImVec4(0.74f,0.72f,0.42f,1.f)}, {"DARKMAGENTA", ImVec4(0.55f,0.00f,0.55f,1.f)},
            {"DARKOLIVE", ImVec4(0.33f,0.42f,0.18f,1.f)}, {"DARKORANGE", ImVec4(1.00f,0.55f,0.00f,1.f)},
            {"DARKORCHID", ImVec4(0.60f,0.20f,0.80f,1.f)}, {"DARKRED", ImVec4(0.55f,0.00f,0.00f,1.f)},
            {"DARKSALMON", ImVec4(0.91f,0.59f,0.48f,1.f)}, {"DARKSEAGREEN", ImVec4(0.56f,0.74f,0.56f,1.f)},
            {"DARKSLATE", ImVec4(0.18f,0.31f,0.31f,1.f)}, {"DARKTURQUOISE", ImVec4(0.00f,0.81f,0.82f,1.f)},
            {"DARKVIOLET", ImVec4(0.58f,0.00f,0.83f,1.f)}, {"DEEPPINK", ImVec4(1.00f,0.08f,0.58f,1.f)},
            {"DEEPSKY", ImVec4(0.00f,0.75f,1.00f,1.f)}, {"DIMGRAY", ImVec4(0.41f,0.41f,0.41f,1.f)},
            {"FIREBRICK", ImVec4(0.70f,0.13f,0.13f,1.f)}, {"FLORAL", ImVec4(1.00f,0.98f,0.94f,1.f)},
            {"FORESTGREEN", ImVec4(0.13f,0.55f,0.13f,1.f)}, {"GAINSBORO", ImVec4(0.86f,0.86f,0.86f,1.f)},
            {"GHOST", ImVec4(0.97f,0.97f,1.00f,1.f)}, {"HONEYDEW", ImVec4(0.94f,1.00f,0.94f,1.f)},
            {"HOTPINK", ImVec4(1.00f,0.41f,0.71f,1.f)}, {"LAWNGREEN", ImVec4(0.49f,0.99f,0.00f,1.f)},
            {"LEMONCHIFFON", ImVec4(1.00f,0.98f,0.80f,1.f)}, {"LIGHTGOLDENROD", ImVec4(0.93f,0.87f,0.51f,1.f)},
            {"LIGHTGREY", ImVec4(0.83f,0.83f,0.83f,1.f)}, {"LIGHTSEAGREEN", ImVec4(0.13f,0.70f,0.67f,1.f)},
            {"LIGHTSKY", ImVec4(0.53f,0.81f,0.98f,1.f)}, {"LIGHTSLATE", ImVec4(0.47f,0.53f,0.60f,1.f)},
            {"LIGHTSTEEL", ImVec4(0.69f,0.77f,0.87f,1.f)}, {"LIMEAQUA", ImVec4(0.00f,1.00f,1.00f,1.f)},
            {"MEDIUMBLUE", ImVec4(0.00f,0.00f,0.80f,1.f)}, {"MEDIUMAQUAMARINE", ImVec4(0.40f,0.80f,0.67f,1.f)},
            {"MEDIUMORCHID", ImVec4(0.73f,0.33f,0.83f,1.f)}, {"MEDIUMPURPLE", ImVec4(0.58f,0.44f,0.86f,1.f)},
            {"MEDIUMSEAGREEN", ImVec4(0.24f,0.70f,0.44f,1.f)}, {"MEDIUMSLATE", ImVec4(0.48f,0.41f,0.80f,1.f)},
            {"MEDIUMSPRING", ImVec4(0.00f,0.98f,0.60f,1.f)}, {"MEDIUMTURQUOISE", ImVec4(0.28f,0.82f,0.80f,1.f)},
            {"MEDIUMVIOLET", ImVec4(0.78f,0.08f,0.52f,1.f)}, {"MIDNIGHTBLUE", ImVec4(0.10f,0.10f,0.44f,1.f)},
            {"MISTYROSE", ImVec4(1.00f,0.89f,0.88f,1.f)}, {"MOCCASIN", ImVec4(1.00f,0.89f,0.71f,1.f)},
            {"NAVAJOWHITE", ImVec4(1.00f,0.87f,0.68f,1.f)}, {"ORANGERED", ImVec4(1.00f,0.27f,0.00f,1.f)},
            {"ORCHID", ImVec4(0.85f,0.44f,0.84f,1.f)}, {"PALEGOLDENROD", ImVec4(0.93f,0.91f,0.67f,1.f)},
            {"PALETURQUOISE", ImVec4(0.69f,0.93f,0.93f,1.f)}, {"PALEGREEN", ImVec4(0.60f,0.98f,0.60f,1.f)},
            {"PALEVIOLETRED", ImVec4(0.86f,0.44f,0.58f,1.f)}, {"PAPAYAWHIP", ImVec4(1.00f,0.94f,0.84f,1.f)},
            {"PEACHPUFF", ImVec4(1.00f,0.85f,0.73f,1.f)}, {"PERU", ImVec4(0.80f,0.52f,0.25f,1.f)},
            {"POWDERBLUE", ImVec4(0.69f,0.88f,0.90f,1.f)}, {"ROSYBROWN", ImVec4(0.74f,0.56f,0.56f,1.f)},
            {"ROYALBLUE", ImVec4(0.25f,0.41f,0.88f,1.f)}, {"SALMON", ImVec4(0.98f,0.50f,0.45f,1.f)},
            {"SANDYBROWN", ImVec4(0.96f,0.64f,0.38f,1.f)}, {"SEAGREEN", ImVec4(0.18f,0.55f,0.34f,1.f)},
            {"SEASHELL", ImVec4(1.00f,0.96f,0.93f,1.f)}, {"SIENNA", ImVec4(0.63f,0.32f,0.18f,1.f)},
            {"SKYBLUE", ImVec4(0.53f,0.81f,0.92f,1.f)}, {"SLATEBLUE", ImVec4(0.42f,0.35f,0.80f,1.f)},
            {"SLATEGRAY", ImVec4(0.44f,0.50f,0.56f,1.f)}, {"SNOW", ImVec4(1.00f,0.98f,0.98f,1.f)},
            {"SPRINGGREEN", ImVec4(0.00f,1.00f,0.50f,1.f)}, {"STEELBLUE", ImVec4(0.27f,0.51f,0.71f,1.f)},
            {"TAN", ImVec4(0.82f,0.71f,0.55f,1.f)}, {"THISTLE", ImVec4(0.85f,0.75f,0.85f,1.f)},
            {"TOMATO", ImVec4(1.00f,0.39f,0.28f,1.f)}, {"TURQUOISE", ImVec4(0.25f,0.88f,0.82f,1.f)},
            {"VIOLET", ImVec4(0.93f,0.51f,0.93f,1.f)}, {"WHEAT", ImVec4(0.96f,0.87f,0.70f,1.f)},
            {"WHITESMOKE", ImVec4(0.96f,0.96f,0.96f,1.f)}, {"YELLOWGREEN", ImVec4(0.60f,0.80f,0.20f,1.f)},
        };
        return &colors;
    }

    inline bool parse_color_tag(const char*& p, ImVec4& out) {
        if (*p != '{') return false;
        const char* e = p + 1;

        // Try hex first: {RRGGBB}
        int hex = 0, digits = 0;
        const char* hexStart = e;
        while (*e && *e != '}' && digits < 6) {
            char c = *e++;
            hex <<= 4;
            if (c >= '0' && c <= '9') hex |= c - '0';
            else if (c >= 'A' && c <= 'F') hex |= c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') hex |= c - 'a' + 10;
            else { hex = -1; break; }
            digits++;
        }
        if (hex >= 0 && *e == '}' && digits == 6) {
            float r = ((hex >> 16) & 0xFF) / 255.f;
            float g = ((hex >>  8) & 0xFF) / 255.f;
            float b = ( hex        & 0xFF) / 255.f;
            out = ImVec4(r, g, b, 1.f);
            p = e + 1;
            return true;
        }

        // Try color name: {COLORNAME}
        e = p + 1;
        std::string name;
        while (*e && *e != '}' && name.size() < 32) {
            char c = *e++;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                name += (char)std::toupper((unsigned char)c);
            else
                break;
        }
        if (*e == '}' && !name.empty()) {
            auto it = GetColorNameMap()->find(name);
            if (it != GetColorNameMap()->end()) {
                out = it->second;
                p = e + 1;
                return true;
            }
        }
        return false;
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

    bool  showTimestamp     = true;

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
    void RenderMessageRow(const ChatLine& line, int rowIdx);
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
