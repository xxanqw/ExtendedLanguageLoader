#include "../src/extended_font_format.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
static std::string read(const std::string& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing fixture "+p);return std::string(std::istreambuf_iterator<char>(f),{});}
static void rejected(const std::string& j,const std::string& m){bool failed=false;try{ell::parsePackage(j,m);}catch(const std::exception&){failed=true;}assert(failed);}
int main(int argc,char** argv) {
    assert(ell::resolveFace(0)==ell::Face::Beckett);assert(ell::resolveFace(1)==ell::Face::Futura);assert(ell::resolveFace(2)==ell::Face::Bank);assert(ell::resolveFace(3)==ell::Face::Pricedown);
    for(unsigned b=0;b<256;++b)assert(ell::physicalSlot(static_cast<uint8_t>(b))==b);
    std::string manifest,metrics;
    if(argc>1){const std::string folder=argv[1];manifest=read(folder+"/extended_fonts.json");metrics=read(folder+"/extended_metrics.json");}
    else {
        std::ostringstream j,m;j<<R"({"gameSymbols":{"gothic":{"0x5D":"wanted-star"},"menu":{"0x5D":"wanted-star"}},"version":1,"atlasSize":2048,"columns":16,"rows":16,"glyphCount":256,"logicalCell":32,"quadHeight":20,"slotIsByte":true,"encoding":"test","mapping":[)";
        for(unsigned b=0;b<256;++b)j<<(b?",":"")<<(b<32?0:b);
        j<<"],\"textures\":{";
        m<<"{\"version\":1,\"faces\":{";
        for(unsigned i=0;i<4;++i){j<<(i?",":"")<<'"'<<ell::StyleKeys[i]<<"\":\""<<ell::TextureNames[i]<<'"';
            m<<(i?",":"")<<'"'<<ell::TextureNames[i]<<"\":{\"spaceWidth\":12,\"unproportional\":20,\"widths\":[";
            for(unsigned b=0;b<256;++b)m<<(b?",":"")<<((i==1||i==2)&&b==0x5D?33:12);
            m<<"]}";}
        j<<"}}";m<<"}}";manifest=j.str();metrics=m.str();
    }
    const auto p=ell::parsePackage(manifest,metrics);assert(p.mapping[32]==32);
    for(const auto& f:p.faces)assert(f.widths.size()==256&&f.space==f.widths[32]);
    auto invalid=manifest;auto pos=invalid.find("\"version\"");pos=invalid.find('1',pos);invalid[pos]='9';rejected(invalid,metrics);
    rejected("{}",metrics);rejected(manifest,"{}");rejected(manifest+"garbage",metrics);
    if(manifest.find("wanted-star")!=std::string::npos){
        auto bad=manifest;bad.replace(bad.find("wanted-star"),11,"unknown-icon");rejected(bad,metrics);
        if(argc==1){auto wrongWidth=metrics;wrongWidth.replace(wrongWidth.find("33"),2,"32");rejected(manifest,wrongWidth);
            const std::string symbols=R"("gameSymbols":{"gothic":{"0x5D":"wanted-star"},"menu":{"0x5D":"wanted-star"}},)";
            auto legacy=manifest;legacy.erase(legacy.find(symbols),symbols.size());assert(ell::parsePackage(legacy,metrics).size==2048);
        }
    }
    if(argc>1){const auto text=read(std::string(argv[1])+"/extended_fonts.txd");const std::vector<uint8_t> txd(text.begin(),text.end());ell::validateTxd(txd,p);
        assert(p.mapping[0xc0]==0x410);assert(p.mapping[0xa5]==0x490);assert(p.mapping[0xb4]==0x491);assert(p.mapping[0xaa]==0x404);assert(p.mapping[0xba]==0x454);assert(p.mapping[0xb2]==0x406);assert(p.mapping[0xb3]==0x456);assert(p.mapping[0xaf]==0x407);assert(p.mapping[0xbf]==0x457);
        auto broken=txd;broken.pop_back();bool fail=false;try{ell::validateTxd(broken,p);}catch(const std::exception&){fail=true;}assert(fail);
        std::cout<<"Generator -> C++ contract passed: four named D3D9 sheets, all 256 byte slots, four 256-width sets, Ukrainian mapping\n";
    }
    std::cout<<"ELL format rejection/style/identity tests passed\n";
}
