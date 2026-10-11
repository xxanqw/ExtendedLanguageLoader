// Asset-free worker/refcount test with a captured UI backend, NOT a real popup.
#include "../src/error_dialog.hpp"
#include <cassert>
#include <iostream>

namespace {
HANDLE entered=nullptr,finish=nullptr;
void capture(const char* text) noexcept {
    assert(std::strstr(text,"Synthetic startup failure"));
    assert(std::strstr(text,"text\\languages.ini"));
    assert(std::strstr(text,"ExtendedLanguageLoader.log"));
    assert(SetEvent(entered));
    assert(WaitForSingleObject(finish,10000)==WAIT_OBJECT_0);
}
}
int main(int argc,char** argv){
    static_assert(sizeof(void*)==4,"Run as x86");
    if(argc!=2)return 2;
    const auto module=LoadLibraryA(argv[1]);assert(module);
    const auto status=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module,"ExtendedLanguageLoaderStatus"));
    assert(status && !status()); // Never unload a ready/hooked game module.
    entered=CreateEventA(nullptr,TRUE,FALSE,nullptr);finish=CreateEventA(nullptr,TRUE,FALSE,nullptr);assert(entered&&finish);
    assert(error_dialog::queue(module,"test-host.exe","Synthetic startup failure","text\\languages.ini","ExtendedLanguageLoader.log",&capture)==error_dialog::Result::NotGame);
    assert(WaitForSingleObject(entered,0)==WAIT_TIMEOUT);
    assert(error_dialog::queue(module,"GTA_SA.EXE","Synthetic startup failure","text\\languages.ini","ExtendedLanguageLoader.log",&capture)==error_dialog::Result::Queued);
    assert(WaitForSingleObject(entered,10000)==WAIT_OBJECT_0);
    assert(error_dialog::queue(module,"gta-sa.exe","Duplicate failure","","",&capture)==error_dialog::Result::AlreadyNotified);
    const std::string file=argv[1];const auto base=file.substr(file.find_last_of("/\\")==std::string::npos?0:file.find_last_of("/\\")+1);
    assert(FreeLibrary(module));assert(GetModuleHandleA(base.c_str())); // worker owns a real extra DLL reference
    assert(SetEvent(finish));
    for(unsigned n=0;n<500 && GetModuleHandleA(base.c_str());++n)Sleep(10);
    assert(!GetModuleHandleA(base.c_str())); // atomic FreeLibraryAndExitThread released it
    assert(error_dialog::queue(nullptr,"gta-sa.exe","Duplicate failure","","",&capture)==error_dialog::Result::AlreadyNotified);
    CloseHandle(entered);CloseHandle(finish);
    std::cout<<"PASS: deferred error UI capture, non-game exclusion, one process notification and worker DLL lifetime; no actual MessageBox displayed\n";
}
