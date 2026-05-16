#pragma once
#include <string>
#include <cstdint>

// TIS-620 (Thai Industrial Standard 620) to UTF-8 converter
// TIS-620 maps 0xA0–0xFF to Thai Unicode block U+0E00–U+0E7F
// Byte 0xA0 = U+00A0 (NBSP), 0xA1–0xFB = Thai chars U+0E01–U+0E5B

namespace tis620 {

    // TIS-620 byte -> Unicode codepoint lookup table (index = TIS-620 byte)
    // 0x00-0x7F are ASCII (same), 0x80-0x9F are undefined/control
    // 0xA0 = U+00A0 NBSP, 0xA1-0xDA = Thai consonants/vowels
    static const uint32_t tis620_to_unicode[256] = {
        // 0x00 - 0x7F  ASCII passthrough
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
        48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
        80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
        112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        // 0x80 - 0x9F  undefined -> replacement char U+FFFD
        0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
        0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
        0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
        0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,0xFFFD,
        // 0xA0 = U+00A0 NBSP
        0x00A0,
        // 0xA1 - 0xDA  Thai consonants & vowels (U+0E01 - U+0E3A)
        0x0E01,0x0E02,0x0E03,0x0E04,0x0E05,0x0E06,0x0E07,0x0E08,
        0x0E09,0x0E0A,0x0E0B,0x0E0C,0x0E0D,0x0E0E,0x0E0F,0x0E10,
        0x0E11,0x0E12,0x0E13,0x0E14,0x0E15,0x0E16,0x0E17,0x0E18,
        0x0E19,0x0E1A,0x0E1B,0x0E1C,0x0E1D,0x0E1E,0x0E1F,0x0E20,
        0x0E21,0x0E22,0x0E23,0x0E24,0x0E25,0x0E26,0x0E27,0x0E28,
        0x0E29,0x0E2A,0x0E2B,0x0E2C,0x0E2D,0x0E2E,0x0E2F,0x0E30,
        0x0E31,0x0E32,0x0E33,0x0E34,0x0E35,0x0E36,0x0E37,0x0E38,
        0x0E39,0x0E3A,
        // 0xDB - 0xDE  undefined
        0xFFFD,0xFFFD,0xFFFD,0xFFFD,
        // 0xDF = U+0E3F BAHT SIGN
        0x0E3F,
        // 0xE0 - 0xFB  Thai digits & symbols (U+0E40 - U+0E5B)
        0x0E40,0x0E41,0x0E42,0x0E43,0x0E44,0x0E45,0x0E46,0x0E47,
        0x0E48,0x0E49,0x0E4A,0x0E4B,0x0E4C,0x0E4D,0x0E4E,0x0E4F,
        0x0E50,0x0E51,0x0E52,0x0E53,0x0E54,0x0E55,0x0E56,0x0E57,
        0x0E58,0x0E59,0x0E5A,0x0E5B,
        // 0xFC - 0xFF  undefined
        0xFFFD,0xFFFD,0xFFFD,0xFFFD
    };

    // Encode a single Unicode codepoint to UTF-8, appending to out
    inline void encode_utf8(uint32_t cp, std::string& out) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            // U+FFFD replacement for out-of-range
            out += '\xEF'; out += '\xBF'; out += '\xBD';
        }
    }

    // Convert a TIS-620 encoded string to UTF-8
    // Input: raw TIS-620 bytes (e.g. from SAMP chat buffer)
    // Output: UTF-8 string safe for ImGui::Text() with Thai font loaded
    inline std::string to_utf8(const char* tis, size_t len = SIZE_MAX) {
        std::string result;
        result.reserve(len == SIZE_MAX ? 64 : len * 3);
        for (size_t i = 0; i < len && tis[i] != '\0'; ++i) {
            uint8_t byte = static_cast<uint8_t>(tis[i]);
            uint32_t cp = tis620_to_unicode[byte];
            if (cp == 0xFFFD) {
                // Skip undefined bytes silently (or use replacement char below)
                // encode_utf8(0xFFFD, result);
                continue;
            }
            encode_utf8(cp, result);
        }
        return result;
    }

    inline std::string to_utf8(const std::string& tis) {
        return to_utf8(tis.c_str(), tis.size());
    }

    // Convert a UTF-8 encoded string back to TIS-620
    // Note: Only handles basic TIS-620 mapping (U+0000-007F and U+0E01-0E5B)
    inline std::string from_utf8(const char* utf8) {
        std::string result;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(utf8);
        while (*p) {
            if (*p < 0x80) {
                result += static_cast<char>(*p++);
            } else if ((*p & 0xE0) == 0xC0) {
                // 2-byte UTF-8
                uint32_t cp = ((*p++ & 0x1F) << 6);
                cp |= (*p++ & 0x3F);
                // Check if it's in TIS-620 range (U+00A0)
                if (cp == 0x00A0) result += static_cast<char>(0xA0);
                else result += '?';
            } else if ((*p & 0xF0) == 0xE0) {
                // 3-byte UTF-8 (Thai characters are here)
                uint32_t cp = ((*p++ & 0x0F) << 12);
                cp |= ((*p++ & 0x3F) << 6);
                cp |= (*p++ & 0x3F);
                
                // U+0E01 -> 0xA1, etc.
                if (cp >= 0x0E01 && cp <= 0x0E5B) {
                    result += static_cast<char>(cp - 0x0E00 + 0xA0);
                } else if (cp == 0x0E3F) {
                    result += static_cast<char>(0xDF);
                } else {
                    result += '?';
                }
            } else if ((*p & 0xF8) == 0xF0) {
                // 4-byte UTF-8
                p += 4;
                result += '?';
            } else {
                p++;
                result += '?';
            }
        }
        return result;
    }

    inline std::string from_utf8(const std::string& utf8) {
        return from_utf8(utf8.c_str());
    }

    // Check if a string contains any high bytes (TIS-620 range)
    inline bool has_thai(const char* s) {
        for (; *s; ++s)
            if (static_cast<uint8_t>(*s) >= 0xA1) return true;
        return false;
    }

} // namespace tis620
