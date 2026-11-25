#pragma once
#include "text_buffer.h"
#include <cstdint>
#include <stdexcept>
#include <string>

// Test fixtures need UTF-8/UTF-16 conversion without deprecated codecvt APIs.
namespace test_utf {
inline std::u16string FromUtf8(const std::string& input) {
    std::u16string output;
    for(size_t i=0;i<input.size();) {
        const auto* bytes=reinterpret_cast<const unsigned char*>(input.data()+i);
        const size_t n=bytes[0]==0?1:text_buffer::Sequence(bytes,input.size()-i);
        if(!n)throw std::range_error("Invalid UTF-8 fixture");
        uint32_t cp=n==1?bytes[0]:(bytes[0]&((1u<<(7-n))-1));
        for(size_t j=1;j<n;++j)cp=(cp<<6)|(bytes[j]&0x3F);
        if(cp<=0xFFFF)output+=static_cast<char16_t>(cp);
        else {
            cp-=0x10000;
            output+=static_cast<char16_t>(0xD800+(cp>>10));
            output+=static_cast<char16_t>(0xDC00+(cp&0x3FF));
        }
        i+=n;
    }
    return output;
}
inline std::string ToUtf8(const std::u16string& input) {
    std::string output;
    for(size_t i=0;i<input.size();++i) {
        uint32_t cp=input[i];
        if(cp>=0xD800&&cp<=0xDBFF) {
            if(i+1>=input.size()||input[i+1]<0xDC00||input[i+1]>0xDFFF)
                throw std::range_error("Invalid UTF-16 fixture");
            const uint32_t lo=input[++i];
            cp=0x10000+((cp-0xD800)<<10)+(lo-0xDC00);
        }else if(cp>=0xDC00&&cp<=0xDFFF)throw std::range_error("Invalid UTF-16 fixture");
        if(cp<0x80)output+=static_cast<char>(cp);
        else if(cp<0x800) {
            output+=static_cast<char>(0xC0|(cp>>6));output+=static_cast<char>(0x80|(cp&0x3F));
        }else if(cp<0x10000) {
            output+=static_cast<char>(0xE0|(cp>>12));output+=static_cast<char>(0x80|((cp>>6)&0x3F));output+=static_cast<char>(0x80|(cp&0x3F));
        }else {
            output+=static_cast<char>(0xF0|(cp>>18));output+=static_cast<char>(0x80|((cp>>12)&0x3F));
            output+=static_cast<char>(0x80|((cp>>6)&0x3F));output+=static_cast<char>(0x80|(cp&0x3F));
        }
    }
    return output;
}
}
