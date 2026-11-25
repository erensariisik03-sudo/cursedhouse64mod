#include "emoji_support.h"
#include "keyboard_catalog.h"
#include <array>
namespace emoji_support {
using namespace il2cpp;
uint64_t Probe(Api& api,Object* field,bool& query_ok) {
    query_ok=false;
    if(!field || !api.object_get_class) return 0;
    auto property=[&](Object* self,const char* name) -> Object* {
        if(!self) return nullptr;
        return api.Call(api.FindMethod(api.object_get_class(self),name,0),self);
    };
    Object* text=property(field,"get_textComponent");
    if(!text) return 0;
    Object* font=property(text,"get_font");
    Class* font_class=font?api.object_get_class(font):nullptr;
    const Method* has_text=api.FindMethod(font_class,"HasCharacters",4);
    const Method* has_scalar=api.FindMethod(font_class,"HasCharacter",1);
    const Method* has_char=api.FindMethod(font_class,"HasCharacter",3);
    // Check the field's sprite asset and the game's default sprite asset, with fallbacks.
    Object* sprite=property(text,"get_spriteAsset");
    Class* settings=api.Find("TMPro","TMP_Settings");
    Object* default_sprite=settings?api.Call(api.FindMethod(settings,"get_defaultSpriteAsset",0)):nullptr;
    std::array<Object*,2> sprites{sprite,default_sprite};
    std::array<const Method*,2> sprite_methods{};
    for(size_t i=0;i<sprites.size();++i) if(sprites[i])
        sprite_methods[i]=api.FindMethod(api.object_get_class(sprites[i]),"SearchForSpriteByUnicode",4);
    uint64_t mask=0;
    for(size_t i=0;i<keyboard_catalog::kEmojiCount;++i) {
        const auto& emoji=keyboard_catalog::kEmojis[i];
        bool supported=false,ok=false;
        if(has_text) {
            String* value=api.string_new(emoji.utf8);
            Object* missing=nullptr;
            bool fallbacks=true,try_add=true;
            void* args[]={value,&missing,&fallbacks,&try_add};
            Object* boxed=api.Call(has_text,font,args,&ok);
            if(ok && boxed) { supported=*static_cast<bool*>(api.object_unbox(boxed)); query_ok=true; }
        } else if(has_scalar) {
            uint32_t cp=emoji.codepoint; void* args[]={&cp};
            Object* boxed=api.Call(has_scalar,font,args,&ok);
            if(ok && boxed) { supported=*static_cast<bool*>(api.object_unbox(boxed)); query_ok=true; }
        }
        // Older HasCharacter(char, ...) cannot represent supplementary-plane emoji.
        if(!supported && !has_text && has_char && emoji.codepoint<=0xFFFF) {
            uint16_t cp=static_cast<uint16_t>(emoji.codepoint);
            bool fallbacks=true,try_add=true; void* args[]={&cp,&fallbacks,&try_add};
            Object* boxed=api.Call(has_char,font,args,&ok);
            if(ok && boxed) { supported=*static_cast<bool*>(api.object_unbox(boxed)); query_ok=true; }
        }
        if(!supported) for(size_t j=0;j<sprites.size();++j) if(sprites[j] && sprite_methods[j]) {
            uint32_t cp=emoji.codepoint; bool fallbacks=true; int index=-1;
            void* args[]={sprites[j],&cp,&fallbacks,&index};
            Object* found=api.Call(sprite_methods[j],nullptr,args,&ok);
            if(ok) { query_ok=true; if(found && index>=0) { supported=true; break; } }
        }
        if(supported) mask|=uint64_t{1}<<i;
    }
    return mask;
}
}
