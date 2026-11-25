#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace username_gate {
struct Gate { size_t compare; unsigned string_register, length_register, old_limit; };
struct Check { unsigned object, length, immediate, condition; int64_t reject; };
inline std::optional<Check> DecodeWithLoad(const uint32_t* code, size_t count, size_t index,size_t load_index) {
    if (index == 0 || index + 1 >= count || load_index>=index) return {};
    const uint32_t load = code[load_index], cmp = code[index], branch = code[index+1];
    Check c{};
    if ((load & 0xFFC00000u) != 0xB9400000u || ((load >> 10) & 0xFFF) != 4 ||
        (cmp & 0xFFC0001Fu) != 0x7100001Fu || (branch & 0xFF000010u) != 0x54000000u) return {};
    c.object = (load >> 5) & 31; c.length = load & 31;
    if (((cmp >> 5) & 31) != c.length || c.object >= 29 || c.length >= 29 || c.object == 17 || c.length == 17) return {};
    c.immediate = (cmp >> 10) & 0xFFF; c.condition = branch & 15;
    const int32_t delta = static_cast<int32_t>(((branch >> 5) & 0x7FFFFu) << 13) >> 13;
    c.reject = static_cast<int64_t>(index + 1) + delta;
    if (c.object == c.length) return {};
    return c;
}
inline std::optional<Check> Decode(const uint32_t* code,size_t count,size_t index) {
    return index?DecodeWithLoad(code,count,index,index-1):std::nullopt;
}
// A match must contain the original 3-character minimum followed by exactly one
// 25-character maximum on a string length load, inside the resolved async method.
inline std::optional<Gate> Find(const uint32_t* code, size_t count) {
    std::optional<Gate> found;
    for (size_t i = 1; i + 1 < count; ++i) {
        auto upper = Decode(code,count,i);
        bool shared_minimum=false;
        // Supplied ARM64 compiler emits one LDR Wlen,[Xstring,#16], then
        // CMP #2 / B.LE and CMP #26 / B.HS using the same unchanged W register.
        // Only this exact five-instruction form may reuse the earlier load.
        if(!upper&&i>=3){
            const auto lower=Decode(code,count,i-2);
            const auto shared=DecodeWithLoad(code,count,i,i-3);
            if(lower&&shared&&lower->immediate==2&&lower->condition==13&&
               lower->object==shared->object&&lower->length==shared->length&&
               lower->reject>static_cast<int64_t>(i+1)&&lower->reject<static_cast<int64_t>(count)){
                upper=shared;shared_minimum=true;
            }
        }
        if (!upper || !((upper->immediate == 26 && upper->condition == 10) ||
                        (upper->immediate==26&&upper->condition==2) ||
                        (upper->immediate == 25 && upper->condition == 12)) ||
            upper->reject <= static_cast<int64_t>(i+1) || upper->reject >= static_cast<int64_t>(count)) continue;
        bool minimum = shared_minimum;
        const size_t start = i > 32 ? i - 32 : 1;
        for (size_t j = start; j + 1 < i; ++j) {
            const auto lower = Decode(code,count,j);
            if (lower && lower->object == upper->object && lower->length == upper->length &&
                lower->immediate == 2 && lower->condition == 13 &&
                lower->reject > static_cast<int64_t>(j+1) && lower->reject < static_cast<int64_t>(count)) minimum = true;
        }
        if (!minimum) continue;
        if (found) return {}; // Ambiguous code is never instrumented.
        found = Gate{i,upper->object,upper->length,25};
    }
    return found;
}
inline size_t ScalarCount(const uint16_t* chars, size_t units, size_t cap) {
    if (!chars) return cap + 1;
    size_t count = 0;
    for (size_t i = 0; i < units; ++i) {
        if (++count > cap) return count;
        if (chars[i] >= 0xD800 && chars[i] <= 0xDBFF && i+1 < units && chars[i+1] >= 0xDC00 && chars[i+1] <= 0xDFFF) ++i;
    }
    return count;
}
// Only the value used by this existing comparison changes. Authentication,
// trimming, the minimum, awaited profile update, and error handling stay original.
inline uint32_t ComparisonValue(uint32_t units, const uint16_t* chars, size_t limit = 1024) {
    if (units <= 25 || units > limit * 2) return units;
    return ScalarCount(chars, units, limit) <= limit ? 25u : units;
}
}
