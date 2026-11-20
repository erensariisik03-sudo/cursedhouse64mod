#include "text_buffer.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
int main() {
    using namespace text_buffer;
    assert(Clean(nullptr).empty());
    assert(Clean("a\xC0\xAF\xED\xA0\x80z") == "az");
    const std::string tr = u8"çğıöşüİĞŞ";
    assert(Count(tr.c_str()) == 9);
    std::string many;
    for (size_t i = 0; i < 1025; ++i) many += u8"ş";
    auto clean = Clean(many.c_str());
    assert(Count(clean.c_str()) == 1024 && clean.size() == 2048);
    many.clear();
    for (size_t i = 0; i < 1025; ++i) many += u8"😀";
    clean = Clean(many.c_str());
    assert(clean.size() == 4096 && Count(clean.c_str()) == 1024);
    char text[kCapacity] = "";
    int cursor = 0, anchor = 0;
    assert(Replace(text, cursor, anchor, u8"aş😀z"));
    cursor = anchor = 3; // after ş
    assert(Replace(text, cursor, anchor, "X"));
    assert(std::string(text) == u8"aşX😀z");
    assert(Replace(text, cursor, anchor, "", true));
    assert(std::string(text) == u8"aş😀z");
    assert(Replace(text, cursor, anchor, "", true));
    assert(std::string(text) == u8"a😀z");
    cursor = 1; anchor = 5; // select one 4-byte emoji
    assert(Replace(text, cursor, anchor, u8"İ"));
    assert(std::string(text) == u8"aİz");
    std::string full(1024, 'a'); std::strcpy(text, full.c_str()); cursor = anchor = 1024;
    assert(!Replace(text, cursor, anchor, "b"));
    anchor = 1023; assert(Replace(text, cursor, anchor, u8"ğ"));
    assert(Count(text) == 1024 && text[1023] == char(0xC4));
    assert(PrefixBytes(u8"a😀ş", 2) == 5);
    std::cout << "UTF-8 limit, selection replacement, cursor insertion and whole-character backspace: PASS\n";
}
