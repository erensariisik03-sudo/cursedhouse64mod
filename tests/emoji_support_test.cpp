#include "emoji_support.h"
#include "keyboard_catalog.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace il2cpp {
struct Class { int id; };
struct Object { Class* klass; bool value=false; };
struct String { std::string text; };
struct Method { int id; };
}
namespace {
using namespace il2cpp;
Class field_class{1},text_class{2},font_class{3},sprite_class{4};
Object field{&field_class},text{&text_class},font{&font_class},sprite{&sprite_class},yes{nullptr,true},no{nullptr,false},error{};
Method get_text{1},get_font{2},get_sprite{3},has_text{4},has_scalar{5},search_sprite{6};
bool full_method=true,font_available=true,throw_all=false;
std::vector<std::unique_ptr<String>> strings;
Class* GetClass(Object* o) { return o->klass; }
Class* Parent(Class*) { return nullptr; }
Domain* DomainGet() { return nullptr; }
const Method* FindMethod(Class* c,const char* name,int count) {
 if(c==&field_class && std::string(name)=="get_textComponent") return &get_text;
 if(c==&text_class && std::string(name)=="get_font") return &get_font;
 if(c==&text_class && std::string(name)=="get_spriteAsset") return &get_sprite;
 if(c==&font_class && std::string(name)=="HasCharacters" && count==4 && full_method) return &has_text;
 if(c==&font_class && std::string(name)=="HasCharacter" && count==1) return &has_scalar;
 if(c==&sprite_class && std::string(name)=="SearchForSpriteByUnicode" && count==4) return &search_sprite;
 return nullptr;
}
String* NewString(const char* value) { strings.push_back(std::make_unique<String>(String{value}));return strings.back().get(); }
void* Unbox(Object* boxed) { return &boxed->value; }
Object* Invoke(const Method* method,void* self,void** args,Object** exception) {
 *exception=nullptr;
 if(throw_all) { *exception=&error;return nullptr; }
 switch(method->id) {
 case 1: assert(self==&field);return &text;
 case 2: assert(self==&text);return font_available?&font:nullptr;
 case 3: assert(self==&text);return &sprite;
 case 4: {
  assert(self==&font && *static_cast<bool*>(args[2]) && *static_cast<bool*>(args[3]));
  assert(args[0]);*static_cast<Object**>(args[1])=nullptr;
  const auto value=static_cast<String*>(args[0])->text;
  return value==u8"☺" || value==u8"😀" ? &yes:&no;
 }
 case 5: {
  assert(self==&font);const auto cp=*static_cast<uint32_t*>(args[0]);
  return cp==0x263A || cp==0x1F600 ? &yes:&no;
 }
 case 6: {
  assert(self==nullptr && args[0]==&sprite && *static_cast<bool*>(args[2]));
  const auto cp=*static_cast<uint32_t*>(args[1]);
  *static_cast<int*>(args[3])=cp==0x1F47B?0:-1;
  return cp==0x1F47B ? &sprite:nullptr;
 }
 }
 std::abort();
}
}
extern "C" int __android_log_print(int,const char*,const char*,...) { return 0; }
int main() {
 il2cpp::Api api{};api.domain_get=DomainGet;api.object_get_class=GetClass;api.class_get_parent=Parent;
 api.class_get_method_from_name=FindMethod;api.runtime_invoke=Invoke;api.object_unbox=Unbox;api.string_new=NewString;
 bool ok=false;const uint64_t expected=(uint64_t{1}<<0)|(uint64_t{1}<<26)|(uint64_t{1}<<44);
 assert(emoji_support::Probe(api,&field,ok)==expected && ok);
 full_method=false;ok=false;assert(emoji_support::Probe(api,&field,ok)==expected && ok);
 font_available=false;ok=false;assert(emoji_support::Probe(api,&field,ok)==(uint64_t{1}<<44) && ok);
 throw_all=true;ok=true;assert(emoji_support::Probe(api,&field,ok)==0 && !ok);
 throw_all=false;api.object_get_class=nullptr;ok=true;assert(emoji_support::Probe(api,&field,ok)==0 && !ok);
 std::cout<<"Live emoji probe: UTF-8 font/fallback query, full Unicode scalar, sprite fallback and unsupported/error filtering: PASS\n";
}
