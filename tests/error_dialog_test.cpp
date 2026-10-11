#include "../src/error_dialog.hpp"
#include <cassert>
#include <iostream>

int main(){
    for(const auto* path:{"gta_sa.exe","gta-sa.exe","S:\\GTA SA\\GTA-SA.EXE","/game/GTA_SA.EXE"})
        assert(error_dialog::isGameHost(path));
    for(const auto* path:{"","gta_sa.exe.old","notgta_sa.exe","windows_exports_test.exe","/gta_sa.exe/helper.exe","gta_sa.ex","gta_sa.exe/"})
        assert(!error_dialog::isGameHost(path));
    const auto text=error_dialog::message("Missing/unlockable GXT: rus.gxt","S:\\bundle\\text\\languages.ini","S:\\bundle\\ExtendedLanguageLoader.log");
    assert(text.find("has been disabled")!=std::string::npos);
    assert(text.find("Missing/unlockable GXT: rus.gxt")!=std::string::npos);
    assert(text.find("S:\\bundle\\text\\languages.ini")!=std::string::npos);
    assert(text.find("S:\\bundle\\ExtendedLanguageLoader.log")!=std::string::npos);
    assert(text.find("Close GTA")!=std::string::npos);
    const auto early=error_dialog::message("Unsupported executable","","");
    assert(early.find("not selected")!=std::string::npos && early.find("if available")!=std::string::npos);
    std::cout<<"Error dialogs passed: both game names, non-game exclusion, failure/config/log text and early-error fallback\n";
}
