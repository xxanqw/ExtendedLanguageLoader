// DOES NOT launch GTA. Executes actual installed x86 hooks against copied EXE code.
// RenderWare loading, sprite textures and GPU calls are FAKE. Not in-game acceptance.
#define main existingSmokeMain
#include "win32_smoke.cpp"
#undef main
#include "../src/extended_font_format.hpp"
#include <cmath>
#undef near
struct EllSprite {void* texture;};
struct EllRect {float left,bottom,right,top;};
struct EllColor {uint8_t r,g,b,a;};
static unsigned ellLoads=0,ellDeletes=0,ellDraws=0;
static int ellSlot=40;
static bool ellFailLoad=false,ellMissingTexture=false;
struct EllVertex {float x,y,z,rhw;uint32_t color;float u,v;};
static_assert(sizeof(EllVertex)==28,"RenderWare D3D9 vertex ABI");
static std::array<EllVertex,4> ellVertices{};
static EllRect ellRect{};static EllColor ellColor{};static std::array<float,8> ellUv{};
template<class T>static T& mem(uintptr_t p){return *reinterpret_cast<T*>(p);}
static bool __cdecl ellLoad(int,const char* filename){++ellLoads;assert(std::string(filename).find("extended_fonts.txd")!=std::string::npos);return !ellFailLoad;}
static int __cdecl ellAdd(const char* name){assert(std::string(name).find("ell_fonts_")==0);return ++ellSlot;}
static void __cdecl ellRemove(int){++ellDeletes;}
static void __cdecl ellNothing(){}
static void __cdecl ellSetCurrent(int){}
static void __thiscall ellTexture(EllSprite* sprite,const char* name){unsigned i=0;for(;i<4&&std::strcmp(name,ell::TextureNames[i]);++i){}assert(i<4);sprite->texture=ellMissingTexture&&i==2?nullptr:reinterpret_cast<void*>(0x1000+i);}
static void __thiscall ellDelete(EllSprite* sprite){sprite->texture=nullptr;}
static void __cdecl ellBuffer(const EllRect& rect,const EllColor& color,float u1,float v1,float u2,float v2,float u3,float v3,float u4,float v4){++ellDraws;ellRect=rect;ellColor=color;ellUv={{u1,v1,u2,v2,u3,v3,u4,v4}};
    // Execute the actual supported EXE's vertex constructor, not a copied C++ approximation.
    reinterpret_cast<void(__cdecl*)(EllVertex*,const EllRect&,const EllColor&,const EllColor&,const EllColor&,const EllColor&,float,float,float,float,float,float,float,float)>(0x727A00)(ellVertices.data(),rect,color,color,color,color,u1,v1,u2,v2,u3,v3,u4,v4);
}
static std::string ellRead(const std::string& p){std::ifstream f(p,std::ios::binary);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});}
static unsigned mapGlyph(unsigned byte){unsigned out=byte;const uintptr_t target=0x7199AD;
    asm volatile("subl $48, %%esp; call *%%edx; addl $48, %%esp":"+a"(out):"d"(target):"ecx","memory","cc");return out;}
static float advance(unsigned glyph){float out;const uintptr_t target=0x719A6B;
    asm volatile("subl $48, %%esp; movl $0x40e00000, 8(%%esp); movl %%ecx, 12(%%esp); call *%%edx; addl $48, %%esp":"=t"(out):"d"(target),"c"(glyph):"eax","memory","cc");return out;}
static void near(float a,float b){if(std::fabs(a-b)>.001f){std::cerr<<"Float mismatch "<<a<<" vs "<<b<<'\n';assert(false);}}
int main(int argc,char** argv) {
    assert(argc>=2);
    if(argc>2&&!std::strcmp(argv[2],"baseline"))return existingSmokeMain(argc,argv);
    mapGame(argv[1]);
    char hostExe[MAX_PATH]{};assert(GetModuleFileNameA(nullptr,hostExe,sizeof(hostExe)));gameRoot=hostExe;gameRoot.resize(gameRoot.find_last_of("\\/")+1);
    HMODULE module=LoadLibraryA(argc>2?argv[2]:"ExtendedLanguageLoader.asi");assert(module);
    char moduleFile[MAX_PATH]{};assert(GetModuleFileNameA(module,moduleFile,sizeof(moduleFile)));
    std::string assetRoot=moduleFile;assetRoot.resize(assetRoot.find_last_of("\\/")+1);
    const auto package=ell::parsePackage(ellRead(assetRoot+"ExtendedFonts/extended_fonts.json"),ellRead(assetRoot+"ExtendedFonts/extended_metrics.json"));
    const auto status=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module,"ExtendedLanguageLoaderStatus"));assert(status&&status());
    // Only replace dangerous continuations after ASI's signature validation.
    bytes(0x6A01E3,{0x58,0xC3});
    bytes(0x5BA697,{0x83,0xC4,0x04,0x5E,0x83,0xC4,0x04,0xC3});
    bytes(0x7189B7,{0x5F,0x5E,0xC3});
    jumpTo(0x7320B5,reinterpret_cast<uintptr_t>(&ellLoad));
    jumpTo(0x5387D0,reinterpret_cast<uintptr_t>(&mockSetDir));
    jumpTo(0x71A210,reinterpret_cast<uintptr_t>(&mockFlush));
    jumpTo(0x731C80,reinterpret_cast<uintptr_t>(&ellAdd));jumpTo(0x731CD0,reinterpret_cast<uintptr_t>(&ellRemove));
    jumpTo(0x7316A0,reinterpret_cast<uintptr_t>(&ellNothing));jumpTo(0x7316B0,reinterpret_cast<uintptr_t>(&ellNothing));jumpTo(0x7319C0,reinterpret_cast<uintptr_t>(&ellSetCurrent));jumpTo(0x731A00,reinterpret_cast<uintptr_t>(&ellSetCurrent));
    jumpTo(0x727270,reinterpret_cast<uintptr_t>(&ellTexture));jumpTo(0x727240,reinterpret_cast<uintptr_t>(&ellDelete));jumpTo(0x728200,reinterpret_cast<uintptr_t>(&ellBuffer));
    bytes(0x719862,{0x89,0xC8,0xC3});bytes(0x719B0C,{0x89,0xC8,0xC3});
    bytes(0x7199E7,{0x0F,0xB6,0x44,0x24,0x10,0xC3});bytes(0x719ADC,{0xD9,0x44,0x24,0x0C,0xC3});
    mem<uint8_t>(0xBA67CC)=0;mockSetDir("");std::memcpy(reinterpret_cast<void*>(0xB71AE0),gameRoot.c_str(),gameRoot.size()+1);
    const auto mainFile=reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2);
    const auto init=reinterpret_cast<void(__cdecl*)()>(0x5BA690),shutdown=reinterpret_cast<void(__cdecl*)()>(0x7189B0);
    mainFile();init();assert(ellLoads==1);const unsigned startupFlushes=flushes;
    const auto style=reinterpret_cast<void(__cdecl*)(unsigned)>(0x719490);
    const auto character=reinterpret_cast<float(__cdecl*)(uint8_t)>(0x719750),prop=reinterpret_cast<float(__cdecl*)(uint8_t)>(0x718770),script=reinterpret_cast<float(__cdecl*)(uint8_t)>(0x719670);
    const auto print=reinterpret_cast<void(__cdecl*)(float,float,uint8_t)>(0x718A10);
    mem<int32_t>(0xC17044)=640;mem<int32_t>(0xC17048)=448;
    mem<float>(0xC71A64)=2;mem<uint8_t>(0xC71A7D)=1;mem<int8_t>(0xC71A9B)=1;
    mem<float>(0xC71AAC)=2;mem<float>(0xC71AB0)=3;mem<uint8_t>(0xC71ACA)=1;mem<int8_t>(0xC71ACE)=1;mem<uint8_t>(0xC71A54)=0;
    mem<uint16_t>(0xA44B68)=0;mem<float>(0xA913E8)=1.5f;mem<uint8_t>(0xA91404)=1;mem<int8_t>(0xA9140A)=2;
    std::memset(reinterpret_cast<void*>(0xC718B0),9,420); // two packed 210-byte tFontData records; PS2Symbol follows
    for(unsigned s=0;s<4;++s){style(s);const unsigned face=unsigned(ell::resolveFace(s));assert(mem<uint8_t>(0xC71A94)==2+face&&mem<uint8_t>(0xC71A95)==0);mem<uint16_t>(0xC71ACC)=uint16_t(2+face);mem<uint32_t>(0xA91410)=s;
        for(uintptr_t site:{uintptr_t(0x71985B),uintptr_t(0x719B05)}){auto* sprite=reinterpret_cast<EllSprite*(__cdecl*)()>(site)();assert(sprite->texture==reinterpret_cast<void*>(0x1000+face));}
        for(unsigned b=0;b<256;++b){const auto glyph=uint8_t(b-32);near(character(glyph),(package.faces[face].widths[b]+1)*2.f);near(prop(glyph),package.faces[face].widths[b]);near(script(glyph),(package.faces[face].widths[b]+2)*1.5f);assert(mapGlyph(b)==glyph);near(advance(glyph),7+(package.faces[face].widths[b]+1)*2.f);}
        mem<uint8_t>(0xC71ACA)=0;near(advance(uint8_t(0xC0-32)),7+(package.faces[face].unprop+1)*2.f);near(prop(uint8_t(0xC0-32)),package.faces[face].unprop);mem<uint8_t>(0xC71ACA)=1;
        const uint8_t byte=0xC0;mem<EllColor>(0xC71AB4)={11,22,33,44};const auto draws=ellDraws;print(10,20,uint8_t(byte-32));assert(ellDraws==draws+1);near(ellRect.left,10);near(ellRect.bottom,80);near(ellRect.right,74);near(ellRect.top,20);assert(ellColor.r==11&&ellColor.a==44);
        const float eps=.5f/package.size;near(ellUv[0],eps);near(ellUv[1],12.f/16+eps);near(ellUv[2],1.f/16-eps);near(ellUv[7],13.f/16-eps);
        near(ellVertices[0].x,10);near(ellVertices[0].y,20);near(ellVertices[0].v,12.f/16+eps);
        near(ellVertices[3].x,10);near(ellVertices[3].y,80);near(ellVertices[3].v,13.f/16-eps);
        assert((ellVertices[0].color>>24)==44&&((ellVertices[0].color>>16)&255)==11);
    }
    style(1);mem<uint8_t>(0xBA67CC)=1;mainFile();assert(mem<uint8_t>(0xC71A94)==1&&mem<uint8_t>(0xC71A95)==0);near(character(uint8_t('A'-32)),20);assert(flushes==startupFlushes+1);
    mem<uint16_t>(0xC71ACC)=1;near(prop(uint8_t('A'-32)),9);assert(mapGlyph(0xC0)==0); // stock clamp, not extended slot
    ellFailLoad=true;mem<uint8_t>(0xBA67CC)=2;mainFile();assert(ellLoads==2&&mem<uint8_t>(0xC71A94)==1);near(character(uint8_t('A'-32)),20);ellFailLoad=false;
    mem<uint8_t>(0xBA67CC)=0;mainFile();assert(ellLoads==2&&mem<uint8_t>(0xC71A94)==2);
    ellMissingTexture=true;mem<uint8_t>(0xBA67CC)=3;mainFile();assert(ellLoads==3&&mem<uint8_t>(0xC71A94)==1);near(character(uint8_t('A'-32)),20);ellMissingTexture=false;
    mem<uint8_t>(0xBA67CC)=0;mainFile();assert(ellLoads==3&&mem<uint8_t>(0xC71A94)==2);shutdown();assert(ellDeletes==3);
    std::cout<<"PASS: real x86 hooks; four style/sprite selectors; all 256 proportional, script, buffer widths and direct slots; nonproportional advance; normalized HD UV/quad/RGBA; missing-package vanilla fallback; old-buffer flush; cached reload/shutdown. FAKE RenderWare/GPU; NOT a GTA run.\n";
}
