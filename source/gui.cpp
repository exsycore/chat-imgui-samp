#include "gui.h"
#include <ctime>
#include <cstdio>

static std::string GetCurrentTimestamp() {
    auto now = std::time(nullptr);
    auto tm  = std::localtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S ", tm);
    return std::string(buf);
}

// Reorder color tags: if a {RRGGBB} or {NAME} tag appears mid-word,
// move it forward to the next whitespace boundary so colors apply to whole words.
static std::string ReorderColorTags(const std::string& input) {
    if (input.empty()) return input;

    // Tokenize into segments: text chunks + color tags
    struct Token { bool isTag; std::string data; };
    std::vector<Token> tokens;
    const char* p = input.c_str();
    std::string buffer;
    while (*p) {
        if (*p == '{') {
            const char* start = p;
            ImVec4 dummy;
            if (ChatColor::parse_color_tag(p, dummy)) {
                if (!buffer.empty()) { tokens.push_back({false, buffer}); buffer.clear(); }
                tokens.push_back({true, std::string(start, p - start)});
                continue;
            }
        }
        buffer += *p++;
    }
    if (!buffer.empty()) tokens.push_back({false, buffer});

    // For each tag, check if it sits mid-word (prev ends with non-space,
    // next begins with non-space). If so, move the tag forward to the next
    // whitespace boundary.
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!tokens[i].isTag) continue;

        bool prevNonSpace = false;
        if (i > 0 && !tokens[i-1].isTag && !tokens[i-1].data.empty()) {
            unsigned char lastByte = static_cast<unsigned char>(tokens[i-1].data.back());
            if (lastByte != ' ' && lastByte != '\t' && lastByte != '\n' && lastByte != '\r') {
                prevNonSpace = true;
            }
        }
        if (!prevNonSpace) continue;

        if (i + 1 >= tokens.size() || tokens[i+1].isTag) continue;
        std::string& next = tokens[i+1].data;
        if (next.empty()) continue;
        unsigned char firstByte = static_cast<unsigned char>(next[0]);
        if (firstByte == ' ' || firstByte == '\t' || firstByte == '\n' || firstByte == '\r') continue;

        unsigned char prevLast = static_cast<unsigned char>(tokens[i-1].data.back());
        bool prevIsAscii = (prevLast < 0x80);

        size_t splitPos = 0;
        if (prevIsAscii) {
            while (splitPos < next.size()) {
                unsigned char b = static_cast<unsigned char>(next[splitPos]);
                if (b >= 0x80) break;
                splitPos++;
            }
            while (splitPos > 0) {
                unsigned char b = static_cast<unsigned char>(next[splitPos - 1]);
                if (b == ' ' || b == '\t' || b == '\n' || b == '\r') splitPos--;
                else break;
            }
        } else {
            while (splitPos < next.size()) {
                unsigned char b = static_cast<unsigned char>(next[splitPos]);
                if (b == ' ' || b == '\t' || b == '\n' || b == '\r') break;
                if (b >= 0xC0) {
                    int extra = 0;
                    if ((b & 0xE0) == 0xC0) extra = 1;
                    else if ((b & 0xF0) == 0xE0) extra = 2;
                    else if ((b & 0xF8) == 0xF0) extra = 3;
                    splitPos += 1 + extra;
                } else {
                    splitPos++;
                }
            }
        }
        if (splitPos == 0 || splitPos >= next.size()) continue;

        std::string moved = next.substr(0, splitPos);
        next.erase(0, splitPos);
        tokens[i-1].data += moved;
    }

    // Reassemble
    std::string out;
    for (auto& t : tokens) out += t.data;
    return out;
}

void SAMPChatImGui::Init(HWND hwnd, IDirect3DDevice9* device) {
    if (m_initialized) return;
    m_hwnd = hwnd;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(device);

    BuildFonts();
    ApplyTheme();
    SnapshotHistory();

    m_alpha     = fade.minAlpha;
    m_fadeState = FadeState::Idle;
    m_lastTick  = GetTickCount64();
    m_initialized = true;
}

void SAMPChatImGui::Shutdown() {
    if (!m_initialized) return;
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void SAMPChatImGui::BuildFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    static const ImWchar thai_ranges[] = {
        0x0020, 0x00FF,
        0x0E00, 0x0E7F,
        0,
    };

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;

    std::string windir = getenv("WINDIR") ? getenv("WINDIR") : "C:\\Windows";
    std::string tahomabd = windir + "\\Fonts\\Tahomabd.ttf";
    std::string tahoma   = windir + "\\Fonts\\Tahoma.ttf";

    if (GetFileAttributesA(tahomabd.c_str()) != INVALID_FILE_ATTRIBUTES)
        m_chatFont = io.Fonts->AddFontFromFileTTF(tahomabd.c_str(), fontSize, &cfg, thai_ranges);
    else
        m_chatFont = io.Fonts->AddFontFromFileTTF(tahoma.c_str(), fontSize, &cfg, thai_ranges);

    io.Fonts->Build();
    m_fontsBuilt = true;
}

void SAMPChatImGui::ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 0.f;
    s.ChildRounding     = 0.f;
    s.FrameRounding     = 4.f;
    s.ScrollbarRounding = 0.f;
    s.WindowBorderSize  = 0.f;
    s.WindowPadding     = {6.f, 4.f};
    s.ItemSpacing       = {4.f, 2.f};
    s.FramePadding      = {8.f, 6.f};
    s.ScrollbarSize     = 0.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_Text]                 = ImVec4(1.f, 1.f, 1.f, 1.f);
    c[ImGuiCol_Border]               = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_Separator]            = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.10f, 0.10f, 0.80f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.14f, 0.14f, 0.14f, 0.90f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
}

void SAMPChatImGui::SnapshotHistory() {
    namespace samp = SAMP_VERSION_NS;
    auto* pChat = samp::RefChat();
    if (!pChat) return;

    m_lines.clear();

    for (int i = 0; i < 100; ++i) {
        auto& entry = pChat->m_entry[i];
        if (entry.m_szText[0] == '\0') continue;

        ChatLine line;
        line.type = entry.m_nType;
        line.prefix = tis620::to_utf8(entry.m_szPrefix);
        line.text   = ReorderColorTags(tis620::to_utf8(entry.m_szText));
        line.prefixColor = ChatColor::from_samp(entry.m_prefixColor);
        line.textColor   = ChatColor::from_samp(entry.m_textColor);
        if (showTimestamp)
            line.timestamp = GetCurrentTimestamp();
        m_lines.push_back(std::move(line));
    }
    m_scrollToBot = true;
}

// Draw text with outline, supporting word-wrap. Updates x/y to end of drawn text.
static void DrawOutlinedTextWrapped(const char* text, const ImVec4& color,
                                     float& x, float& y, float wrapStartX, float wrapWidth,
                                     float lineHeight) {
    if (!text || text[0] == '\0') return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = const_cast<ImFont*>(ImGui::GetFont());
    float fontSize = ImGui::GetFontSize();
    float alpha = color.w;
    ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
    ImU32 outline = IM_COL32(0, 0, 0, (int)(alpha * 255));

    int totalLen = (int)strlen(text);
    int offset = 0;

    while (offset < totalLen) {
        float remaining = wrapWidth - (x - wrapStartX);
        if (remaining < 5.f) {
            x = wrapStartX;
            y += lineHeight + 2.f;
            remaining = wrapWidth;
        }

        // Word-wrap: measure words until we exceed remaining width
        int fit = 0;
        float accW = 0.f;
        const char* s = text + offset;
        const char* end = text + totalLen;

        while (s < end) {
            const char* wordStart = s;
            while (s < end && *s != ' ') s++;
            const char* wordEnd = s;
            if (s < end) s++; // include trailing space

            float wordW = ImGui::CalcTextSize(wordStart, s, false, 0.0f).x;
            if (accW + wordW > remaining && fit > 0) {
                s = wordStart; // rewind to before this word
                break;
            }
            accW += wordW;
            fit = (int)(s - (text + offset));
        }

        if (fit == 0) {
            // Force at least one UTF-8 character
            fit = 1;
            unsigned char c = (unsigned char)text[offset];
            if ((c & 0xF0) == 0xF0) fit = 4;
            else if ((c & 0xE0) == 0xE0) fit = 3;
            else if ((c & 0xC0) == 0xC0) fit = 2;
        }

        // Draw outline (8 directions)
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;
                dl->AddText(font, fontSize, ImVec2(x + dx, y + dy), outline,
                            text + offset, text + offset + fit);
            }
        }

        // Draw main text
        dl->AddText(font, fontSize, ImVec2(x, y), col,
                    text + offset, text + offset + fit);

        x += ImGui::CalcTextSize(text + offset, text + offset + fit, false, 0.0f).x;
        offset += fit;

        if (offset < totalLen) {
            x = wrapStartX;
            y += lineHeight + 2.f;
        }
    }
}

void SAMPChatImGui::RenderMessageRow(const ChatLine& line, int /*rowIdx*/) {
    const float alpha = m_alpha;

    float availW = ImGui::GetContentRegionAvail().x;
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float startX = startPos.x;
    float startY = startPos.y;
    float currentX = startX;
    float currentY = startY;
    float lineHeight = ImGui::GetTextLineHeight();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Timestamp pill
    float tsW = 0.f;
    if (showTimestamp && !line.timestamp.empty()) {
        ImVec4 tsColor(0.92f, 0.94f, 0.96f, alpha);
        ImVec2 textSize = ImGui::CalcTextSize(line.timestamp.c_str());
        float padX = 5.f;
        float padY = 2.f;
        float rectW = textSize.x + padX * 2.f;
        float rectH = textSize.y + padY * 2.f;
        float rounding = rectH * 0.5f;

        dl->AddRectFilled(
            ImVec2(startX, startY),
            ImVec2(startX + rectW, startY + rectH),
            IM_COL32(0, 0, 0, (int)(alpha * 200)),
            rounding
        );

        float textX = startX + padX;
        float textY = startY + padY;
        ImU32 outline = IM_COL32(0, 0, 0, (int)(alpha * 255));
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;
                dl->AddText(ImVec2(textX + dx, textY + dy), outline, line.timestamp.c_str());
            }
        }
        dl->AddText(ImVec2(textX, textY), ImGui::ColorConvertFloat4ToU32(tsColor), line.timestamp.c_str());

        tsW = rectW + 4.f;
        currentX = startX + tsW;
    }

    float wrapW = availW - tsW;
    if (wrapW < 50.f) wrapW = availW;
    float wrapStartX = startX + tsW;

    // Prefix (player name)
    if (!line.prefix.empty()) {
        ImVec4 pc = line.prefixColor; pc.w = alpha;
        DrawOutlinedTextWrapped(line.prefix.c_str(), pc,
                                currentX, currentY, wrapStartX, wrapW, lineHeight);

        // Space after prefix
        float spaceW = ImGui::CalcTextSize(" ").x;
        if (currentX + spaceW > wrapStartX + wrapW - 2.f) {
            currentX = wrapStartX;
            currentY += lineHeight + 2.f;
        } else {
            currentX += spaceW;
        }

        // Colon
        ImVec4 colonColor(0.75f, 0.78f, 0.85f, alpha);
        DrawOutlinedTextWrapped(": ", colonColor,
                                currentX, currentY, wrapStartX, wrapW, lineHeight);
    }

    // Message text with inline {RRGGBB} color tags
    const char* p = line.text.c_str();
    ImVec4 cur = line.textColor; cur.w = alpha;
    std::string segment;

    auto flush = [&]() {
        if (!segment.empty()) {
            DrawOutlinedTextWrapped(segment.c_str(), cur,
                                    currentX, currentY, wrapStartX, wrapW, lineHeight);
            segment.clear();
        }
    };

    while (*p) {
        ImVec4 nextColor;
        if (ChatColor::parse_color_tag(p, nextColor)) {
            flush();
            nextColor.w = alpha;
            cur = nextColor;
        } else {
            segment += *p++;
        }
    }
    flush();

    // Advance ImGui cursor to end of this row
    float totalH = (currentY - startY) + lineHeight + 4.f;
    ImGui::Dummy(ImVec2(availW, totalH));
}

void SAMPChatImGui::OnNewEntry(int type, const char* szText, const char* szPrefix,
                                DWORD textColor, DWORD prefixColor) {
    ChatLine line;
    line.type        = type;
    line.text        = ReorderColorTags(tis620::to_utf8(szText   ? szText   : ""));
    line.prefix      = tis620::to_utf8(szPrefix ? szPrefix : "");
    line.textColor   = ChatColor::from_samp(textColor);
    line.prefixColor = ChatColor::from_samp(prefixColor);
    if (showTimestamp)
        line.timestamp = GetCurrentTimestamp();

    if ((int)m_lines.size() >= MAX_LINES)
        m_lines.pop_front();
    m_lines.push_back(std::move(line));

    m_scrollToBot = true;

    if (m_fadeState == FadeState::Idle || m_fadeState == FadeState::FadingOut)
        m_fadeState = FadeState::FadingIn;
    else if (m_fadeState == FadeState::Holding) {
        m_fadeState  = FadeState::Holding;
        m_holdTimer  = fade.holdSeconds;
    }
}

void SAMPChatImGui::OnInputOpen() {
    m_inputOpen  = true;
    m_holdTimer  = fade.chatOpenSeconds;
    m_fadeState  = FadeState::FadingIn;
    m_focusInput = true;
    memset(m_inputBuf, 0, sizeof(m_inputBuf));
    m_nHistoryIndex = -1;
    memset(m_szSavedInput, 0, sizeof(m_szSavedInput));
    m_scrollDelta = 0;

    ImGuiIO& io = ImGui::GetIO();
    io.ClearInputKeys();

    namespace samp = SAMP_VERSION_NS;
    auto* pGame = samp::RefGame();
    if (pGame) pGame->SetCursorMode(samp::CURSOR_LOCKCAM, FALSE);
}

void SAMPChatImGui::OnInputClose() {
    m_inputOpen = false;
    m_fadeState = FadeState::Holding;
    m_holdTimer = fade.holdSeconds;
    m_scrollDelta = 0;

    namespace samp = SAMP_VERSION_NS;
    auto* pGame = samp::RefGame();
    if (pGame) pGame->SetCursorMode(samp::CURSOR_NONE, FALSE);
}

void SAMPChatImGui::UpdateFade(float dt) {
    switch (m_fadeState) {
    case FadeState::FadingIn:
        m_alpha += fade.fadeInSpeed * dt;
        if (m_alpha >= fade.maxAlpha) {
            m_alpha     = fade.maxAlpha;
            m_fadeState = FadeState::Holding;
            m_holdTimer = m_inputOpen ? fade.chatOpenSeconds : fade.holdSeconds;
        }
        break;
    case FadeState::Holding:
        if (!m_inputOpen) {
            m_holdTimer -= dt;
            if (m_holdTimer <= 0.f)
                m_fadeState = FadeState::FadingOut;
        }
        break;
    case FadeState::FadingOut:
        m_alpha -= fade.fadeOutSpeed * dt;
        if (m_alpha <= fade.minAlpha) {
            m_alpha     = fade.minAlpha;
            m_fadeState = FadeState::Idle;
        }
        break;
    case FadeState::Idle:
        break;
    }
}

// Render text with black outline (same alpha as text)
static void TextOutlined(const char* text, const ImVec4& color) {
    if (!text || text[0] == '\0') return;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float alpha = color.w;
    ImU32 outline = IM_COL32(0, 0, 0, (int)(alpha * 255));

    // 8 directions for outline
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            dl->AddText(ImVec2(pos.x + dx, pos.y + dy), outline, text);
        }
    }
    dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(color), text);

    // Advance cursor by text size
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImGui::Dummy(sz);
}

void SAMPChatImGui::RenderColoredLine(const ChatLine& line, int /*rowIdx*/) {
    const float alpha = m_alpha;

    ImGui::PushTextWrapPos(0.0f);

    // Timestamp
    if (!line.timestamp.empty()) {
        ImVec4 tsColor(0.45f, 0.48f, 0.55f, alpha);
        TextOutlined(line.timestamp.c_str(), tsColor);
        ImGui::SameLine(0.f, 2.f);
    }

    // Prefix (player name)
    if (!line.prefix.empty()) {
        ImVec4 pc = line.prefixColor; pc.w = alpha;
        TextOutlined(line.prefix.c_str(), pc);
        ImGui::SameLine(0.f, 0.f);
        ImVec4 colon(0.55f, 0.58f, 0.65f, alpha);
        TextOutlined(": ", colon);
        ImGui::SameLine(0.f, 0.f);
    }

    // Text with inline {RRGGBB} color tags
    const char* p = line.text.c_str();
    ImVec4 cur = line.textColor; cur.w = alpha;
    std::string segment;

    auto flush = [&]() {
        if (!segment.empty()) {
            TextOutlined(segment.c_str(), cur);
            segment.clear();
            ImGui::SameLine(0.f, 0.f);
        }
    };

    while (*p) {
        ImVec4 nextColor;
        if (ChatColor::parse_color_tag(p, nextColor)) {
            flush();
            nextColor.w = alpha;
            cur = nextColor;
        } else {
            segment += *p++;
        }
    }
    if (!segment.empty()) {
        TextOutlined(segment.c_str(), cur);
    } else {
        ImGui::NewLine();
    }

    ImGui::PopTextWrapPos();
}

void SAMPChatImGui::RenderChatWindow() {
    if (m_alpha < 0.01f && !m_inputOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    float scrW = io.DisplaySize.x;
    float scrH = io.DisplaySize.y;
    float winW = scrW * (widthPct / 100.f);
    float winH = scrH * (heightPct / 100.f);

    ImGui::SetNextWindowPos(ImVec2(windowX, windowY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 20.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.f, 2.f));
    ImGui::PushFont(m_chatFont);

    ImGui::Begin("##SAMPChat", nullptr, flags);

    // Child region (scrollbar hidden, mouse wheel still works)
    ImGui::BeginChild("##ChatLines", ImVec2(0, 0), false);

    // Render chat lines
    int rowIdx = 0;
    for (const auto& line : m_lines)
        RenderMessageRow(line, rowIdx++);

    // Handle PageUp/PageDown
    if (m_scrollDelta != 0) {
        float lineH = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetScrollY(ImGui::GetScrollY() - m_scrollDelta * lineH);
        m_scrollDelta = 0;
    }

    // Auto-scroll to bottom on new message (deferred, runs at frame end)
    if (m_scrollToBot) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBot = false;
    }

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleVar(2);
}

void SAMPChatImGui::RenderInputWindow() {
    if (!m_inputOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    float scrW = io.DisplaySize.x;
    float scrH = io.DisplaySize.y;
    float winW = scrW * (widthPct / 100.f);
    float winH = scrH * (heightPct / 100.f);

    // Position below the chat window
    ImGui::SetNextWindowPos(ImVec2(windowX, windowY + winH + 4.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowFocus();

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 8.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.f, 2.f));
    ImGui::PushFont(m_chatFont);

    ImGui::Begin("##SAMPInput", nullptr, flags);

    ImGui::PushItemWidth(-1);
    if (m_focusInput) {
        ImGui::SetKeyboardFocusHere();
        m_focusInput = false;
    }

    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        auto& self = SAMPChatImGui::Get();
        namespace samp = SAMP_VERSION_NS;
        auto* pInput = samp::RefInputBox();
        if (!pInput) return 0;

        if (data->EventKey == ImGuiKey_UpArrow) {
            if (pInput->m_nTotalRecall <= 0) return 0;
            if (self.m_nHistoryIndex == -1) {
                strncpy(self.m_szSavedInput, data->Buf, sizeof(self.m_szSavedInput) - 1);
                self.m_nHistoryIndex = pInput->m_nTotalRecall - 1;
            } else if (self.m_nHistoryIndex > 0) {
                self.m_nHistoryIndex--;
            }
            int idx = self.m_nHistoryIndex;
            if (idx >= 0 && idx < pInput->m_nTotalRecall) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, pInput->m_szRecallBufffer[idx]);
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (self.m_nHistoryIndex == -1) return 0;
            self.m_nHistoryIndex++;
            if (self.m_nHistoryIndex >= pInput->m_nTotalRecall) {
                self.m_nHistoryIndex = -1;
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, self.m_szSavedInput);
            } else {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, pInput->m_szRecallBufffer[self.m_nHistoryIndex]);
            }
        }
        return 0;
    };

    ImGuiInputTextFlags inputFlags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputTextWithHint("##ChatInput", "Type a message...",
                                 m_inputBuf, sizeof(m_inputBuf),
                                 inputFlags, callback))
    {
        namespace samp = SAMP_VERSION_NS;
        if (m_inputBuf[0] != '\0') {
            std::string tisText = tis620::from_utf8(m_inputBuf);
            if (tisText[0] == '/') {
                if (tisText == "/q" || tisText == "/quit") {
                    samp::Commands::Quit("");
                } else if (tisText == "/timestamp") {
                    showTimestamp = !showTimestamp;
                } else {
                    auto* pInput = samp::RefInputBox();
                    if (pInput) {
                        strncpy(pInput->m_szInput, tisText.c_str(), sizeof(pInput->m_szInput) - 1);
                        pInput->m_szInput[sizeof(pInput->m_szInput) - 1] = '\0';
                        pInput->Send(tisText.c_str());
                    }
                }
            } else {
                auto* pNetGame = samp::RefNetGame();
                if (pNetGame) {
                    auto* pPool = pNetGame->GetPlayerPool();
                    if (pPool) {
                        auto* pLocal = pPool->GetLocalPlayer();
                        if (pLocal) pLocal->Chat(tisText.c_str());
                    }
                }
            }
        }
        OnInputClose();
        auto* pInput = samp::RefInputBox();
        if (pInput) {
            pInput->Close();
            memset(pInput->m_szInput, 0, sizeof(pInput->m_szInput));
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        namespace samp = SAMP_VERSION_NS;
        OnInputClose();
        auto* pInput = samp::RefInputBox();
        if (pInput) {
            pInput->Close();
            memset(pInput->m_szInput, 0, sizeof(pInput->m_szInput));
        }
    }

    ImGui::PopItemWidth();
    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleVar(2);
}

void SAMPChatImGui::Tick(IDirect3DDevice9* /*device*/) {
    if (!m_initialized) return;

    DWORD64 now = GetTickCount64();
    float   dt  = (now - m_lastTick) / 1000.f;
    m_lastTick  = now;
    if (dt > 0.1f) dt = 0.1f;

    UpdateFade(dt);

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderChatWindow();
    RenderInputWindow();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
