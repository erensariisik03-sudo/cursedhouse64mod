#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

namespace text_buffer {
constexpr size_t kLimit = 1024; // Unicode scalar values, not UTF-8 bytes.
constexpr size_t kCapacity = kLimit * 4 + 1;
// Return the length of one well-formed scalar, excluding overlong encodings,
// surrogate code points and values above U+10FFFF. 0 means invalid/incomplete.
inline size_t Sequence(const unsigned char* p, size_t n) {
    if (!n) return 0;
    if (p[0] < 0x80) return p[0] ? 1 : 0;
    size_t k = p[0] >= 0xC2 && p[0] <= 0xDF ? 2 :
               p[0] >= 0xE0 && p[0] <= 0xEF ? 3 :
               p[0] >= 0xF0 && p[0] <= 0xF4 ? 4 : 0;
    if (!k || n < k) return 0;
    for (size_t i = 1; i < k; ++i) if ((p[i] & 0xC0) != 0x80) return 0;
    if ((p[0] == 0xE0 && p[1] < 0xA0) || (p[0] == 0xED && p[1] >= 0xA0) ||
        (p[0] == 0xF0 && p[1] < 0x90) || (p[0] == 0xF4 && p[1] > 0x8F)) return 0;
    return k;
}
inline std::string Clean(const char* text) {
    if (!text) return {};
    std::string result;
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    size_t left = std::strlen(text), count = 0;
    while (left && count < kLimit) {
        const size_t k = Sequence(p, left);
        if (!k) { ++p; --left; continue; }
        result.append(reinterpret_cast<const char*>(p), k);
        p += k; left -= k; ++count;
    }
    return result;
}
inline size_t Count(const char* text) {
    size_t count = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
        if ((*p & 0xC0) != 0x80) ++count;
    return count;
}
inline size_t PrefixBytes(const char* text, size_t maxCount) {
    size_t i = 0, n = std::strlen(text), count = 0;
    while (i < n && count < maxCount) {
        const size_t k = Sequence(reinterpret_cast<const unsigned char*>(text + i), n - i);
        if (!k) break;
        i += k; ++count;
    }
    return i;
}
inline int Boundary(const char* text, int index) {
    index = std::clamp(index, 0, static_cast<int>(std::strlen(text)));
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) --index;
    return index;
}
inline bool Replace(char* text, int& cursor, int& anchor, const char* insert, bool backspace = false) {
    cursor = Boundary(text, cursor); anchor = Boundary(text, anchor);
    int lo = std::min(cursor, anchor), hi = std::max(cursor, anchor);
    if (backspace && lo == hi && lo > 0) lo = Boundary(text, lo - 1);
    std::string value(text);
    const std::string addition = Clean(insert);
    const std::string candidate = value.substr(0, lo) + addition + value.substr(hi);
    if (Count(candidate.c_str()) > kLimit) return false;
    std::memcpy(text, candidate.c_str(), candidate.size() + 1);
    cursor = anchor = lo + static_cast<int>(addition.size());
    return candidate != value;
}
}
