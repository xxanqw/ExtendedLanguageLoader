#pragma once
#include "font_runtime.hpp"
#include "extended_font_format.hpp"

namespace ellfonts {
using StyleFn=void(__cdecl*)(unsigned);
using WidthFn=float(__cdecl*)(uint8_t);
using PrintFn=void(__cdecl*)(float,float,uint8_t);
inline StyleFn originalStyle=nullptr;
inline WidthFn originalWidth=nullptr,originalScriptWidth=nullptr,originalProp=nullptr;
inline PrintFn originalPrint=nullptr;
inline fonts::LogFn logger=nullptr;
inline bool enabled=false,debug=false;
inline unsigned lastStyle=0;
struct Sprite {void* texture=nullptr;};
static_assert(sizeof(Sprite)==4,"x86 sprite ABI");
struct Resource {
    ell::Package package;std::string directory;int slot=-1;
    std::array<Sprite,4> sprites{};std::array<HANDLE,3> locks{{INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE}};
    ~Resource(){for(auto h:locks)if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
};
inline std::vector<std::unique_ptr<Resource>> resources;
inline Resource* active=nullptr;
inline void info(const std::string& text){if(logger)logger("[ELL Fonts] "+text);}
template<class T> inline T& memory(uintptr_t p){return *reinterpret_cast<T*>(p);}
inline int activeFace(unsigned textureId) noexcept {return enabled&&active&&textureId>=2&&textureId<6?int(textureId-2):-1;}
inline unsigned renderTexture() noexcept {return memory<uint16_t>(0xC71ACC);}
inline void __cdecl setStyle(unsigned style) {
    lastStyle=style;originalStyle(style);
    if(enabled&&active&&style<=3){memory<uint8_t>(0xC71A94)=uint8_t(2+unsigned(ell::resolveFace(style)));memory<uint8_t>(0xC71A95)=0;}
}
inline float width(unsigned face,uint8_t byte,bool proportional) noexcept {
    const auto& m=active->package.faces[face];return proportional?m.widths[byte]:m.unprop;
}
inline float __cdecl characterWidth(uint8_t glyphId) {
    const int face=activeFace(memory<uint8_t>(0xC71A94));if(face<0)return originalWidth(glyphId);
    const auto byte=uint8_t(glyphId+32);
    return (width(unsigned(face),byte,memory<uint8_t>(0xC71A7D)!=0)+memory<int8_t>(0xC71A9B))*memory<float>(0xC71A64);
}
inline float __cdecl propWidth(uint8_t glyphId) {
    const int face=activeFace(renderTexture());if(face<0)return originalProp(glyphId);
    return width(unsigned(face),uint8_t(glyphId+32),memory<uint8_t>(0xC71ACA)!=0);
}
inline float __cdecl scriptWidth(uint8_t glyphId) {
    if(!enabled||!active)return originalScriptWidth(glyphId);
    const uintptr_t text=0xA913E8+memory<uint16_t>(0xA44B68)*0x44;
    const auto style=memory<uint32_t>(text+0x28);if(style>3)return originalScriptWidth(glyphId);
    return (width(unsigned(ell::resolveFace(style)),uint8_t(glyphId+32),memory<uint8_t>(text+0x1C)!=0)+memory<int8_t>(text+0x22))*memory<float>(text);
}
// Two RenderFontBuffer sprite LEAs used the stock TWO-element array directly.
inline uintptr_t __cdecl bufferedSprite() noexcept {
    const int face=activeFace(renderTexture());
    return face>=0?reinterpret_cast<uintptr_t>(&active->sprites[unsigned(face)]):
        uintptr_t(intptr_t(0xC71AD0)+memory<int16_t>(0xC71ACC)*4);
}
// Called by narrow mid-function thunks. Stack points to ORIGINAL RenderFontBuffer locals.
inline bool __cdecl mapBufferedGlyph(uint8_t* stack,uint8_t byte) noexcept {
    if(activeFace(renderTexture())<0)return false;
    stack[0x10]=uint8_t(byte-32);return true; // PrintChar ABI still receives byte-minus-32; do NOT subfont-map or clamp.
}
inline bool __cdecl advanceBufferedGlyph(uint8_t* stack) noexcept {
    const int face=activeFace(renderTexture());if(face<0)return false;
    const auto byte=uint8_t(stack[0x10]+32);
    auto& x=*reinterpret_cast<float*>(stack+0x0C);
    x+=(width(unsigned(face),byte,memory<uint8_t>(0xC71ACA)!=0)+memory<int8_t>(0xC71ACE))*memory<float>(0xC71AAC);
    return true;
}
struct Rect {float left,bottom,right,top;};
struct Color {uint8_t r,g,b,a;};
inline void __cdecl printChar(float x,float y,uint8_t glyphId) {
    const int face=activeFace(renderTexture());
    // Button/control icons retain GTA's own separate sprites and draw path.
    if(face<0||memory<uint8_t>(0xC71A54)){originalPrint(x,y,glyphId);return;}
    const auto byte=uint8_t(glyphId+32);if(byte==32||!active->package.mapping[byte])return;
    if(x<0||y<0||x>memory<int32_t>(0xC17044)||y>memory<int32_t>(0xC17048))return;
    const float sx=memory<float>(0xC71AAC),sy=memory<float>(0xC71AB0);
    // CRect's memory order is left,BOTTOM,right,TOP (native PrintChar stores y at +12).
    const Rect rect{x,y+20*sy,x+32*sx,y};
    const float u=float(byte&15)/16.0f,v=float(byte>>4)/16.0f;
    const float epsilon=0.5f/float(active->package.size); // half-target-texel inset, not resolution-scaled game metrics
    reinterpret_cast<void(__cdecl*)(const Rect&,const Color&,float,float,float,float,float,float,float,float)>(0x728200)(
        rect,memory<Color>(0xC71AB4),u+epsilon,v+epsilon,u+1.0f/16-epsilon,v+epsilon,
        u+epsilon,v+1.0f/16-epsilon,u+1.0f/16-epsilon,v+1.0f/16-epsilon);
}
inline std::string textFile(const std::string& path){const auto b=fonts::readBytes(path,128*1024);return std::string(b.begin(),b.end());}
inline void configure(bool use,const std::string& root,const std::vector<ll::Language>& list,fonts::LogFn log,bool verbose) {
    logger=log;debug=verbose;enabled=use;resources.resize(list.size());if(!use)return;
    for(unsigned i=0;i<list.size();++i){const auto& language=list[i];if(language.fontPackage.empty())continue;
        try {auto r=std::make_unique<Resource>();r->directory=root+language.fontPackage;if(r->directory.back()!='\\'&&r->directory.back()!='/')r->directory+='\\';
            if(r->directory.size()>220)throw std::runtime_error("Extended package path too long");
            const std::array<std::string,3> paths{{r->directory+"extended_fonts.txd",r->directory+"extended_fonts.json",r->directory+"extended_metrics.json"}};
            for(unsigned n=0;n<3;++n){r->locks[n]=winpaths::open(paths[n].c_str(),GENERIC_READ,FILE_SHARE_READ,OPEN_EXISTING);if(r->locks[n]==INVALID_HANDLE_VALUE)throw std::runtime_error("Missing/unlockable "+paths[n]);}
            r->package=ell::parsePackage(textFile(paths[1]),textFile(paths[2]));
            if(language.fontEncoding.empty()||language.fontEncoding!=r->package.encoding)throw std::runtime_error("Font_encoding must explicitly match manifest encoding; existing GXT bytes are NOT converted");
            ell::validateTxd(fonts::readBytes(paths[0],80*1024*1024),r->package);
            if(debug)info("Validated "+language.filename+" encoding "+r->package.encoding+"; 256 byte slots per face");
            resources[i]=std::move(r);
        }catch(const std::exception& e){info(std::string("Package rejected; using vanilla/paired fonts: ")+e.what());}}
}
inline bool load(Resource& r,unsigned index) {
    if(r.slot>=0)return true;
    fonts::DirectoryScope directory;directory.enterRoot();
    const std::string name="ell_fonts_"+std::to_string(index);
    r.slot=reinterpret_cast<int(__cdecl*)(const char*)>(0x731C80)(name.c_str());if(r.slot<0)throw std::runtime_error("No TXD slot");
    if(!fonts::originalLoadTxd(r.slot,(r.directory+"extended_fonts.txd").c_str())){reinterpret_cast<void(__cdecl*)(int)>(0x731CD0)(r.slot);r.slot=-1;throw std::runtime_error("Native TXD decoder rejected extended package");}
    reinterpret_cast<void(__cdecl*)(int)>(0x731A00)(r.slot);
    reinterpret_cast<void(__cdecl*)()>(0x7316A0)();reinterpret_cast<void(__cdecl*)(int)>(0x7319C0)(r.slot);
    for(unsigned face=0;face<4;++face)reinterpret_cast<void(__thiscall*)(Sprite*,const char*)>(0x727270)(&r.sprites[face],ell::TextureNames[face]);
    reinterpret_cast<void(__cdecl*)()>(0x7316B0)();
    if(std::any_of(r.sprites.begin(),r.sprites.end(),[](const Sprite& s){return !s.texture;})) {
        for(auto& sprite:r.sprites)reinterpret_cast<void(__thiscall*)(Sprite*)>(0x727240)(&sprite);
        reinterpret_cast<void(__cdecl*)(int)>(0x731CD0)(r.slot);r.slot=-1;throw std::runtime_error("Native TXD is missing an ELL texture");}
    if(debug)for(const auto* texture:ell::TextureNames)info(std::string("Loaded ")+texture+" "+std::to_string(r.package.size)+"x"+std::to_string(r.package.size));
    return true;
}
inline void switchLanguage(unsigned index) noexcept {
    if(!enabled)return;
    Resource* next=index<resources.size()?resources[index].get():nullptr;
    if(next==active)return;
    try{if(next)load(*next,index);}catch(const std::exception& e){info(std::string("Native load failed; falling back: ")+e.what());next=nullptr;}
    if(fonts::ready)reinterpret_cast<fonts::VoidFn>(0x71A210)(); // flush OLD sprites/metrics, INCLUDING vanilla -> ELL transitions
    active=next;setStyle(lastStyle);
    // RenderFontBuffer binds its last saved texture before reading the next batch.
    // Do not leave a four-sheet ID in the native two-sprite array after fallback.
    memory<uint16_t>(0xC71ACC)=memory<uint8_t>(0xC71A94);
    memory<uint8_t>(0xC71AC9)=memory<uint8_t>(0xC71A95);
    info(active?"Extended font renderer active; "+active->package.encoding+" mapping (256 slots)":"Vanilla/paired font renderer active");
}
inline void release() noexcept {
    active=nullptr;
    if(originalStyle){originalStyle(lastStyle);memory<uint16_t>(0xC71ACC)=memory<uint8_t>(0xC71A94);memory<uint8_t>(0xC71AC9)=memory<uint8_t>(0xC71A95);}
    for(auto& r:resources)if(r&&r->slot>=0){for(auto& sprite:r->sprites)reinterpret_cast<void(__thiscall*)(Sprite*)>(0x727240)(&sprite);reinterpret_cast<void(__cdecl*)(int)>(0x731CD0)(r->slot);r->slot=-1;}
}
} // namespace ellfonts
