#include "../src/install_layout.hpp"
#include <cassert>
#include <iostream>
#include <set>
int main() {
    const std::string root="C:\\Games\\SA\\", mod=root+"modloader\\ExtendedLanguageLoader\\";
    std::set<std::string> present{ll::pathKey(root+"text\\languages.ini")};
    auto exists=[&](const std::string& p){return present.count(ll::pathKey(p))!=0;};
    auto r=ll::selectLayout(root,root+"ExtendedLanguageLoader.asi",exists);
    assert(!r.bundled && r.assetRoot==root && r.config==root+"text\\languages.ini");
    r=ll::selectLayout(root,root+"scripts\\ExtendedLanguageLoader.asi",exists);
    assert(!r.bundled && r.assetRoot==root);
    bool rejected=false;try{ll::selectLayout(root,mod+"ExtendedLanguageLoader.asi",exists);}catch(const std::exception&){rejected=true;}assert(rejected);
    present.insert(ll::pathKey(mod+"text\\languages.ini"));
    r=ll::selectLayout(root,mod+"ExtendedLanguageLoader.asi",exists);
    assert(r.bundled && r.assetRoot==mod && r.config==mod+"text\\languages.ini");
    present.erase(ll::pathKey(root+"text\\languages.ini"));
    r=ll::selectLayout(root,mod+"ExtendedLanguageLoader.asi",exists);assert(r.bundled);
    rejected=false;try{ll::selectLayout(root,root+"ExtendedLanguageLoader.asi",exists);}catch(const std::exception&){rejected=true;}assert(rejected);
    // Similar prefixes are not mistaken for ModLoader. Mixed case/slashes work.
    present.insert(ll::pathKey(root+"text\\languages.ini"));
    r=ll::selectLayout(root,root+"modloader-old\\ELL.asi",exists);assert(!r.bundled);
    r=ll::selectLayout("c:/games/sa/","C:/GAMES/SA/ModLoader/ExtendedLanguageLoader/ELL.asi",exists);assert(r.bundled);
    assert(ll::pathKey(r.assetRoot)==ll::pathKey(mod));
    present.insert(ll::pathKey(root+"scripts\\text\\languages.ini"));
    r=ll::selectLayout(root,root+"scripts\\ELL.asi",exists);assert(r.bundled && r.assetRoot==root+"scripts\\");
    rejected=false;try{ll::parentDirectory("ELL.asi");}catch(const std::exception&){rejected=true;}assert(rejected);
    std::cout<<"Installation layout passed: root/scripts fallback, colocated bundle priority, fail-closed ModLoader config, case/slash handling.\n";
}
