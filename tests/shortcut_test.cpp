#include "shortcut_editor.h"
#include "keyboard_catalog.h"
#include <cassert>
#include <iostream>
using shortcut_editor::Editor;
using shortcut_editor::Kind;
int main() {
 char text[text_buffer::kCapacity]="abc"; int cursor=2,anchor=2; Editor editor;
 assert(editor.Begin(Kind::Size,text,cursor,anchor)); assert(std::string(text)=="ab<size=xxx>c");
 for(char c:std::string("9999")) assert(editor.SizeDigit(c,text,cursor,anchor));
 assert(std::string(text)=="ab<size=9999>c");
 assert(editor.SizeBackspace(text,cursor,anchor)); assert(std::string(text)=="ab<size=999>c");
 assert(editor.CommitSize(cursor,anchor)); assert(!editor.Active());
 assert(text_buffer::Replace(text,cursor,anchor,"z")); assert(std::string(text)=="ab<size=999>zc");
 assert(editor.Begin(Kind::Color,text,cursor,anchor)); assert(std::string(text)=="ab<size=999>z<xxxx>c");
 editor.Cancel(text,cursor,anchor); assert(std::string(text)=="ab<size=999>zc");
 cursor=0; anchor=2;
 assert(editor.Begin(Kind::Color,text,cursor,anchor)); assert(editor.PickColor("F00F",text,cursor,anchor));
 assert(std::string(text)=="<#F00F><size=999>zc");
 std::strcpy(text,"test"); cursor=anchor=4;
 assert(editor.Begin(Kind::Size,text,cursor,anchor)); assert(!editor.CommitSize(cursor,anchor));
 assert(!editor.SizeDigit('x',text,cursor,anchor)); assert(editor.SizeDigit('0',text,cursor,anchor));
 assert(editor.SizeBackspace(text,cursor,anchor)); assert(std::string(text)=="test<size=xxx>");
 editor.Cancel(text,cursor,anchor); assert(std::string(text)=="test");
 for(const auto& color:keyboard_catalog::kColors) {
  text[0]=0; cursor=anchor=0;
  assert(editor.Begin(Kind::Color,text,cursor,anchor));
  assert(editor.PickColor(color.rgba,text,cursor,anchor));
  assert(std::string(text)==std::string("<#")+color.rgba+">");
 }
 std::string full(1024,'x'); std::strcpy(text,full.c_str()); cursor=anchor=1024;
 assert(!editor.Begin(Kind::Size,text,cursor,anchor)); assert(std::string(text)==full);
 assert(!editor.Begin(Kind::Color,text,cursor,anchor)); assert(std::string(text)==full);
 std::string almost(1017,'x'); std::strcpy(text,almost.c_str()); cursor=anchor=1017;
 assert(editor.Begin(Kind::Color,text,cursor,anchor)); assert(editor.PickColor("FFFF",text,cursor,anchor));
 assert(text_buffer::Count(text)==1024);
 std::strcpy(text,"abc");cursor=anchor=3;
 assert(editor.Begin(Kind::Size,text,cursor,anchor));
 text[0]=0; assert(!editor.SizeDigit('9',text,cursor,anchor)); editor.Drop();
 std::cout<<"Size/color templates, arbitrary digit length, selection, cancel and atomic 1024 limit: PASS\n";
}
