#pragma once
#include "core.hpp"
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>

namespace ell {
// Canonical contract: JSON version 1, mapping[byte] is a Unicode scalar (0 = unassigned).
// The renderer indexes by BYTE; it never treats this scalar as a texture coordinate.
enum class Face : unsigned { Futura, Beckett, Bank, Pricedown };
inline Face resolveFace(unsigned gtaStyle) {
    switch (gtaStyle) { case 0:return Face::Beckett; case 1:return Face::Futura; case 2:return Face::Bank; case 3:return Face::Pricedown; }
    throw std::runtime_error("Invalid GTA font style");
}
inline const std::array<const char*,4> TextureNames{{"ell_futura","ell_beckett","ell_bank","ell_pricedown"}};
inline const std::array<const char*,4> StyleKeys{{"subtitles","gothic","menu","pricedown"}};
inline uint8_t physicalSlot(uint8_t byte) noexcept { return byte; }
struct Json {
    enum Kind { Null, Number, String, Array, Object, Boolean } kind=Null;
    double number=0; std::string string; std::vector<Json> array; std::map<std::string,Json> object;
    const Json& at(const std::string& key) const { if(kind!=Object||!object.count(key))throw std::runtime_error("Missing JSON field: "+key);return object.at(key); }
    unsigned integer(unsigned max) const { if(kind!=Number||!std::isfinite(number)||number<0||number>max||std::floor(number)!=number)throw std::runtime_error("Invalid JSON integer");return static_cast<unsigned>(number); }
    std::string text() const {if(kind!=String)throw std::runtime_error("Expected JSON string");return string;}
};
class JsonReader {
    const std::string& s; size_t p=0; unsigned nodes=0;
    void whitespace(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
    char take(){if(p>=s.size())throw std::runtime_error("Truncated JSON");return s[p++];}
    std::string string(){if(take()!='"')throw std::runtime_error("Expected JSON string");std::string out;
        for(;;){char c=take();if(c=='"')return out;if(static_cast<unsigned char>(c)<32)throw std::runtime_error("JSON control character");
            if(c=='\\'){c=take();if(c=='"'||c=='\\'||c=='/')out+=c;else if(c=='n')out+='\n';else if(c=='r')out+='\r';else if(c=='t')out+='\t';else throw std::runtime_error("Unsupported JSON escape in metadata");}else out+=c;}}
    Json value(unsigned depth){if(depth>12||++nodes>6000)throw std::runtime_error("JSON complexity limit");whitespace();if(p>=s.size())throw std::runtime_error("Truncated JSON");Json v;
        if(s[p]=='{'){++p;v.kind=Json::Object;whitespace();if(p<s.size()&&s[p]=='}'){++p;return v;}for(;;){whitespace();auto key=string();whitespace();if(take()!=':')throw std::runtime_error("JSON colon");if(v.object.count(key))throw std::runtime_error("Duplicate JSON key");v.object.emplace(key,value(depth+1));whitespace();char c=take();if(c=='}')return v;if(c!=',')throw std::runtime_error("JSON object separator");}}
        if(s[p]=='['){++p;v.kind=Json::Array;whitespace();if(p<s.size()&&s[p]==']'){++p;return v;}for(;;){v.array.push_back(value(depth+1));whitespace();char c=take();if(c==']')return v;if(c!=',')throw std::runtime_error("JSON array separator");}}
        if(s[p]=='"'){v.kind=Json::String;v.string=string();return v;}
        for(const auto& lit:{std::string("true"),std::string("false"),std::string("null")})if(s.compare(p,lit.size(),lit)==0){p+=lit.size();v.kind=lit=="null"?Json::Null:Json::Boolean;v.number=lit=="true";return v;}
        const size_t begin=p;if(s[p]=='-')++p;if(p>=s.size()||s[p]<'0'||s[p]>'9')throw std::runtime_error("JSON number");
        if(s[p]=='0')++p;else while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;
        if(p<s.size()&&s[p]=='.'){++p;const size_t first=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==first)throw std::runtime_error("JSON fraction");}
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;const size_t first=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==first)throw std::runtime_error("JSON exponent");}
        v.kind=Json::Number;v.number=std::strtod(s.substr(begin,p-begin).c_str(),nullptr);if(!std::isfinite(v.number))throw std::runtime_error("JSON non-finite number");return v;
    }
public: explicit JsonReader(const std::string& text):s(text){} Json parse(){if(s.size()>128*1024)throw std::runtime_error("JSON size limit");auto v=value(0);whitespace();if(p!=s.size())throw std::runtime_error("JSON trailing data");return v;}
};
struct Metrics { std::array<uint8_t,256> widths{}; uint8_t space=0,unprop=0; };
struct Package {unsigned size=0;std::string encoding;std::array<uint32_t,256> mapping{};std::array<Metrics,4> faces{};};
inline Package parsePackage(const std::string& manifest,const std::string& metrics) {
    const auto j=JsonReader(manifest).parse(),m=JsonReader(metrics).parse();Package out;
    if(j.at("version").integer(65535)!=1||m.at("version").integer(65535)!=1||j.at("columns").integer(256)!=16||j.at("rows").integer(256)!=16||j.at("glyphCount").integer(256)!=256||j.at("logicalCell").integer(256)!=32||j.at("quadHeight").integer(256)!=20||j.at("slotIsByte").kind!=Json::Boolean||j.at("slotIsByte").number!=1)throw std::runtime_error("Unsupported extended font contract");
    out.size=j.at("atlasSize").integer(4096);if(out.size!=512&&out.size!=1024&&out.size!=2048&&out.size!=4096)throw std::runtime_error("Invalid ELL atlas size");
    out.encoding=j.at("encoding").text();if(out.encoding.empty()||out.encoding.size()>80||!ll::ascii(out.encoding))throw std::runtime_error("Invalid ELL encoding identifier");
    const auto& map=j.at("mapping");if(map.kind!=Json::Array||map.array.size()!=256)throw std::runtime_error("ELL mapping must have 256 entries");
    for(unsigned byte=0;byte<256;++byte){const auto cp=map.array[byte].integer(0x10ffff);if(cp>=0xd800&&cp<=0xdfff)throw std::runtime_error("Surrogate in ELL map");out.mapping[byte]=cp;}
    if(out.mapping[32]!=32)throw std::runtime_error("ELL space byte must be 0x20");
    if(out.mapping[126]&&out.mapping[126]!=126)throw std::runtime_error("ELL 0x7E remains the GTA formatting delimiter");
    for(unsigned byte=0;byte<32;++byte)if(out.mapping[byte])throw std::runtime_error("ELL control bytes must remain unmapped");
    const auto& textures=j.at("textures");const auto& faces=m.at("faces");if(textures.object.size()!=4||faces.object.size()!=4)throw std::runtime_error("ELL needs exactly four faces");
    for(unsigned i=0;i<4;++i){if(textures.at(StyleKeys[i]).text()!=TextureNames[i])throw std::runtime_error("Wrong ELL style texture");const auto& f=faces.at(TextureNames[i]);const auto& widths=f.at("widths");
        if(widths.kind!=Json::Array||widths.array.size()!=256)throw std::runtime_error("ELL requires 256 widths per face");
        for(unsigned b=0;b<256;++b){out.faces[i].widths[b]=static_cast<uint8_t>(widths.array[b].integer(255));if(!out.faces[i].widths[b])throw std::runtime_error("Zero ELL width: missing glyph exports are prohibited");}
        out.faces[i].space=static_cast<uint8_t>(f.at("spaceWidth").integer(255));out.faces[i].unprop=static_cast<uint8_t>(f.at("unproportional").integer(255));
        if(out.faces[i].space!=out.faces[i].widths[32]||!out.faces[i].unprop)throw std::runtime_error("Invalid ELL space/UNPROP");}
    // Optional v1 extension: geometry is already rasterized into the byte cells;
    // metadata declares the native font2 symbol aliases, not a Unicode decoder.
    const auto symbols=j.object.find("gameSymbols");
    if(symbols!=j.object.end()){
        const auto& s=symbols->second;
        if(s.kind!=Json::Object||s.object.size()!=2||out.mapping[0x5D]!=0x5D)throw std::runtime_error("Invalid GTA game-symbol contract");
        for(const auto* key:{"gothic","menu"}){
            const auto& face=s.at(key);
            if(face.kind!=Json::Object||face.object.size()!=1||face.at("0x5D").text()!="wanted-star")throw std::runtime_error("Unknown GTA game symbol");
        }
        if(out.faces[1].widths[0x5D]!=33||out.faces[2].widths[0x5D]!=33)throw std::runtime_error("Wanted stars require native 33-unit advances");
    }
    return out;
}
inline void validateTxd(const std::vector<uint8_t>& b,const Package& package) {
    if(b.size()<28||ll::u32(b,0)!=0x16||ll::u32(b,4)!=b.size()-12||ll::u32(b,12)!=1||ll::u32(b,16)!=4||b[24]!=4||b[25]||b[26]!=2||b[27])throw std::runtime_error("Expected four-texture D3D9 dictionary");
    size_t p=28;std::set<std::string> names;
    for(unsigned i=0;i<4;++i){if(p+24>b.size()||ll::u32(b,p)!=0x15)throw std::runtime_error("Missing ELL native chunk");
        const size_t nativeLength=ll::u32(b,p+4);if(nativeLength>b.size()-p-12)throw std::runtime_error("Truncated ELL native chunk");
        const size_t end=p+12+nativeLength;p+=12;
        if(end>b.size()||ll::u32(b,p)!=1)throw std::runtime_error("Invalid ELL native chunk");
        const size_t size=ll::u32(b,p+4),a=p+12;if(end<a||end-a<12||size!=end-a-12||size<92||ll::u32(b,a)!=9||!std::memchr(b.data()+a+8,0,32))throw std::runtime_error("Invalid ELL native struct");
        const std::string name(reinterpret_cast<const char*>(b.data()+a+8));if(std::find(TextureNames.begin(),TextureNames.end(),name)==TextureNames.end()||!names.insert(name).second)throw std::runtime_error("Missing/duplicate ELL face name");
        const unsigned w=b[a+80]|unsigned(b[a+81])<<8,h=b[a+82]|unsigned(b[a+83])<<8;
        if(w!=package.size||h!=w||ll::u32(b,a+4)!=0x1102||ll::u32(b,a+72)!=0x0300||ll::u32(b,a+76)!=0x33545844||b[a+84]!=16||b[a+85]!=1||b[a+86]!=4||b[a+87]!=9||ll::u32(b,a+88)!=w*h||size!=92+w*h)throw std::runtime_error("Invalid ELL DXT3 raster");
        p=a+size;if(ll::u32(b,p)!=3||ll::u32(b,p+4)!=0)throw std::runtime_error("Invalid ELL native extension");p=end;}
    if(p+12!=b.size()||ll::u32(b,p)!=3||ll::u32(b,p+4)!=0)throw std::runtime_error("Invalid ELL dictionary extension");
}
} // namespace ell
