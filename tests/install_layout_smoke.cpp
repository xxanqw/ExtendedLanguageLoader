// Real checked x86 thunks + real CRT GXT opens; fake graphics, NOT a GTA run.
#define main originalSmokeMain
#include "win32_smoke.cpp"
#undef main
#include "../src/install_layout.hpp"
#include <map>
static unsigned privateLoads=0,translatedCalls=0,patchedImports=0,notifications=0;
static DWORD WINAPI wrongAttributes(LPCSTR){++translatedCalls;return FILE_ATTRIBUTE_DIRECTORY;}
static DWORD WINAPI wrongModuleName(HMODULE,LPSTR,DWORD){++translatedCalls;return 0;}
static DWORD WINAPI wrongSetting(LPCSTR,LPCSTR,LPCSTR,LPSTR,DWORD,LPCSTR){++translatedCalls;return 0;}
static HANDLE WINAPI wrongCreateFile(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE){++translatedCalls;return INVALID_HANDLE_VALUE;}
static BOOL WINAPI wrongDirectory(LPCWSTR){++translatedCalls;return FALSE;}
struct UString {USHORT length,maximum;PWSTR buffer;};
struct Notification {ULONG flags;const UString* full;const UString* name;PVOID base;ULONG size;};
static void CALLBACK poisonImports(ULONG reason,const Notification* data,PVOID){
 if(reason!=1||std::wstring(data->name->buffer,data->name->length/2)!=L"ExtendedLanguageLoader.asi")return;
 ++notifications;
 auto* base=static_cast<uint8_t*>(data->base);auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
 auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
 for(auto* d=imports;d->Name;++d){if(!d->OriginalFirstThunk)continue;auto* names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+d->OriginalFirstThunk);auto* slots=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+d->FirstThunk);
  for(;names->u1.AddressOfData;++names,++slots){if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))continue;const auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData)->Name;uintptr_t replacement=0;
   if(!std::strcmp(reinterpret_cast<const char*>(name),"GetFileAttributesA"))replacement=reinterpret_cast<uintptr_t>(&wrongAttributes);
   if(!std::strcmp(reinterpret_cast<const char*>(name),"GetModuleFileNameA"))replacement=reinterpret_cast<uintptr_t>(&wrongModuleName);
   if(!std::strcmp(reinterpret_cast<const char*>(name),"GetPrivateProfileStringA"))replacement=reinterpret_cast<uintptr_t>(&wrongSetting);
   if(!std::strcmp(reinterpret_cast<const char*>(name),"CreateFileA"))replacement=reinterpret_cast<uintptr_t>(&wrongCreateFile);
   if(!std::strcmp(reinterpret_cast<const char*>(name),"SetCurrentDirectoryW"))replacement=reinterpret_cast<uintptr_t>(&wrongDirectory);
   if(replacement){DWORD old=0;assert(VirtualProtect(&slots->u1.Function,4,PAGE_READWRITE,&old));slots->u1.Function=static_cast<DWORD>(replacement);DWORD ignored=0;assert(VirtualProtect(&slots->u1.Function,4,old,&ignored));++patchedImports;}
  }
 }
}
struct PrivateSprite {void* texture;};
static bool __cdecl privateLoad(int,const char* path){textureCall=path;++privateLoads;assert(std::ifstream(path,std::ios::binary));return true;}
static int __cdecl privateSlot(const char*){return 40+int(privateLoads);}
static void __cdecl nothing(){}
static void __cdecl slotNothing(int){}
static void __thiscall texture(PrivateSprite* s,const char*){s->texture=reinterpret_cast<void*>(1);}
static void __thiscall deleteTexture(PrivateSprite* s){s->texture=nullptr;}
static FILE* __cdecl actualOpen(const char* path,const char* mode){fileCall=path;return std::fopen(path,mode);}
static HANDLE dialogShown=nullptr;
static int WINAPI capturedMessageBox(HWND owner,LPCSTR text,LPCSTR title,UINT flags){
 assert(!owner && (flags&MB_ICONERROR));assert(std::strstr(title,"ExtendedLanguageLoader"));
 assert(std::strstr(text,"Missing/unlockable GXT"));assert(std::strstr(text,"languages.ini"));assert(std::strstr(text,"ExtendedLanguageLoader.log"));
 assert(SetEvent(dialogShown));return IDOK;
}
static unsigned noticeDraws=0;
static void __cdecl noticePrint(float x,float y,const char* text){
 ++noticeDraws;assert(x>0&&y>0);assert(std::strstr(text,"CLEO support is experimental"));
 assert(std::strstr(text,"bugs may occur") && std::strstr(text,"github.com/xxanqw/ExtendedLanguageLoader/issues"));
 assert(*reinterpret_cast<uint8_t*>(0xC71A79)==1);assert(*reinterpret_cast<uint8_t*>(0xC71A7B)==0);
 assert(*reinterpret_cast<uint8_t*>(0xC71A7D)==1);assert(*reinterpret_cast<uint8_t*>(0xC71A96)==0);
 assert(*reinterpret_cast<uint8_t*>(0xC71A9B)==0);
}
static std::array<uint8_t,5> beforeFxt{};
static const char* __fastcall nativeText(void*,int,const char*){return "native-text";}
static const char* __fastcall fakeFxt(void* self,int,const char* key){
 if(!std::strcmp(key,"WEAPON"))return "fxt-weapon";
 std::memcpy(reinterpret_cast<void*>(0x6A0050),beforeFxt.data(),5);
 const auto result=reinterpret_cast<const char*(__thiscall*)(void*,const char*)>(0x6A0050)(self,key);
 jumpTo(0x6A0050,reinterpret_cast<uintptr_t>(&fakeFxt));
 return result;
}
static std::vector<uint8_t> readFile(const std::string& path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 assert(argc>=4);mapGame(argv[1]);
 char host[MAX_PATH]{};assert(GetModuleFileNameA(nullptr,host,sizeof(host)));gameRoot=ll::parentDirectory(host);
 const std::string modulePath=argv[2],assets=ll::normalizedDirectory(argv[3]);
 const bool bundle=ll::pathKey(assets)!=ll::pathKey(gameRoot);
 const std::string mode=argc>4?argv[4]:"normal";
 HMODULE silentPatch=nullptr;
 const bool lateMods=mode=="late-mods",unitsBad=mode=="units-body-conflict"||mode=="units-rva-conflict";
 std::array<uint8_t,5> unitsHook{},textHook{};
 if(lateMods||unitsBad){
  assert(argc>5);silentPatch=LoadLibraryExA(argv[5],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(silentPatch);
  const auto entry=reinterpret_cast<uintptr_t>(silentPatch)+0xDE50;
  if(mode=="units-body-conflict"){DWORD old=0,ignored=0;assert(VirtualProtect(reinterpret_cast<void*>(entry),1,PAGE_EXECUTE_READWRITE,&old));bytes(entry,{0x90});assert(VirtualProtect(reinterpret_cast<void*>(entry),1,old,&ignored));}
  jumpTo(0x56D220,entry+(mode=="units-rva-conflict"?1:0));
  std::memcpy(unitsHook.data(),reinterpret_cast<void*>(0x56D220),5);
 }
 if(lateMods){
  jumpTo(0x6A0050,reinterpret_cast<uintptr_t>(&nativeText));std::memcpy(beforeFxt.data(),reinterpret_cast<void*>(0x6A0050),5);
  jumpTo(0x6A0050,reinterpret_cast<uintptr_t>(&fakeFxt));std::memcpy(textHook.data(),reinterpret_cast<void*>(0x6A0050),5);
 }
 if(mode=="conflict")bytes(0x538900,{0x90});
 if(mode=="menu-label-conflict")bytes(0x579D73,{0x90});
 if(mode=="notice-conflict")bytes(0x57E44E,{0x90});
 std::array<uint8_t,12> untouched{};std::memcpy(untouched.data(),reinterpret_cast<void*>(0x6A01D2),untouched.size());
 const auto fontInitOriginal=*reinterpret_cast<uint8_t*>(0x5BA690);
 void* cookie=nullptr;
 using Register=LONG(NTAPI*)(ULONG,void(CALLBACK*)(ULONG,const Notification*,PVOID),PVOID,PVOID*);
 using Unregister=LONG(NTAPI*)(PVOID);
 Register reg=nullptr;Unregister unreg=nullptr;
 const auto rp=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"LdrRegisterDllNotification"),up=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"LdrUnregisterDllNotification");
 std::memcpy(&reg,&rp,sizeof(reg));std::memcpy(&unreg,&up,sizeof(unreg));
 if(mode=="iat"){assert(reg&&unreg);assert(reg(0,&poisonImports,nullptr,&cookie)==0);}
 if(bundle)assert(SetCurrentDirectoryA(ll::parentDirectory(modulePath).c_str()));
 if(mode=="dialog-error"){
  dialogShown=CreateEventA(nullptr,TRUE,FALSE,nullptr);assert(dialogShown);
  const auto user32=LoadLibraryA("user32.dll");assert(user32);
  const auto entry=reinterpret_cast<uintptr_t>(GetProcAddress(user32,"MessageBoxA"));assert(entry);
  DWORD old=0;assert(VirtualProtect(reinterpret_cast<void*>(entry),5,PAGE_EXECUTE_READWRITE,&old));
  jumpTo(entry,reinterpret_cast<uintptr_t>(&capturedMessageBox));
  assert(VirtualProtect(reinterpret_cast<void*>(entry),5,old,&old));
 }
 HMODULE module=LoadLibraryA(modulePath.c_str());assert(module);
 if(cookie){assert(unreg(cookie)==0);assert(notifications==1&&translatedCalls==0);}
 const auto status=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module,"ExtendedLanguageLoaderStatus"));assert(status);
 if(mode=="reject"||mode=="conflict"||mode=="menu-label-conflict"||unitsBad||mode=="dialog-error"){
  if(mode=="dialog-error"){assert(WaitForSingleObject(dialogShown,10000)==WAIT_OBJECT_0);CloseHandle(dialogShown);}
  assert(!status());assert(!std::memcmp(untouched.data(),reinterpret_cast<void*>(0x6A01D2),untouched.size()));
  std::cout<<"PASS: invalid/conflicting bundle refused before any text hook commit.\n";return 0;
 }
 assert(status());const bool textOnly=mode=="text-only";
 if(bundle){
  // Execute the checked native renderer CALL with a minimal thiscall wrapper.
  for(const uintptr_t site:{0x579D73u,0x579688u,0x579824u}){
   bytes(site-10,{0xFF,0x74,0x24,0x04,0x90,0x90,0x90,0x90,0x90,0x90});bytes(site+5,{0xC2,0x04,0x00});
  }
  if(!lateMods)jumpTo(0x6A0050,reinterpret_cast<uintptr_t>(&nativeText));
  auto label=reinterpret_cast<const char*(__thiscall*)(void*,const char*)>(0x579D69);
  for(unsigned i=0;i<11;++i){const auto key=ll::labelKey(i);assert(ll::labelIndex(key.c_str())==int(i));assert(std::strlen(label(reinterpret_cast<void*>(0xC1B340),key.c_str()))>0);}
  if(lateMods)for(unsigned n=0;n<100;++n){
   assert(!std::strcmp(label(reinterpret_cast<void*>(0xC1B340),"OTHER"),"native-text"));
   assert(!std::strcmp(label(reinterpret_cast<void*>(0xC1B340),"WEAPON"),"fxt-weapon"));
   assert(!std::strcmp(label(reinterpret_cast<void*>(0xC1B340),"LLT0000"),"English (ELL)"));
   assert(!std::memcmp(textHook.data(),reinterpret_cast<void*>(0x6A0050),5));
   assert(!std::memcmp(unitsHook.data(),reinterpret_cast<void*>(0x56D220),5));
   assert(*reinterpret_cast<uint8_t*>(0x579D73)==0xE8);
  }
 }
 // Title-driven footer: does not depend on any helper lookup/draw taking place.
 // Capture native queueing, exercise actual font setters, and require exact
 // restoration of current details while deliberately retaining queued output.
 *reinterpret_cast<int32_t*>(0xC17044)=1920;*reinterpret_cast<int32_t*>(0xC17048)=1080;
 jumpTo(0x71A820,reinterpret_cast<uintptr_t>(&noticePrint));
 if(mode=="notice-conflict")assert(*reinterpret_cast<uint8_t*>(0x57E44E)==0x90);
 else {const uint8_t untouchedHelper[]{0xE8,0xFD,0x1B,0x12,0x00};assert(!std::memcmp(reinterpret_cast<void*>(0x57E44E),untouchedHelper,5));}
 auto titleLookup=reinterpret_cast<const char*(__thiscall*)(void*,const char*)>(bundle?0x57967E:0x6A0050);
 if(!bundle){bytes(0x6A0055,{0x5F,0x5E,0x83,0xC4,0x20});jumpTo(0x6A005A,reinterpret_cast<uintptr_t>(&nativeText));}
 const auto oldScreen=*reinterpret_cast<uint8_t*>(0xBA6748+0x15D);
 for(unsigned screen=0;screen<45;++screen){
  *reinterpret_cast<uint8_t*>(0xBA6748+0x15D)=uint8_t(screen);
  std::array<uint8_t,0x4C> before{};std::memcpy(before.data(),reinterpret_cast<void*>(0xC71A54),before.size());
  const auto draws=noticeDraws;
  assert(!std::strcmp(titleLookup(reinterpret_cast<void*>(0xC1B340),"FEH_LAN"),"native-text"));
  assert(noticeDraws==draws+unsigned(screen==28));
  assert(!std::memcmp(before.data(),reinterpret_cast<void*>(0xC71A54),before.size()));
  assert(!std::strcmp(titleLookup(reinterpret_cast<void*>(0xC1B340),"FET_MIG"),"native-text"));
  assert(noticeDraws==draws+unsigned(screen==28));
 }
 *reinterpret_cast<uint8_t*>(0xBA6748+0x15D)=oldScreen;
 if(textOnly)assert(*reinterpret_cast<uint8_t*>(0x5BA690)==fontInitOriginal);
 bytes(0x6A01E3,{0x58,0xC3});bytes(0x69FD06,{0x58,0xC3});
 // ModLoader std.text owns the native OpenFile CALL sites, not this entry. Model
 // its forwarding by passing a different path with the same selected basename.
 jumpTo(0x8232D8,reinterpret_cast<uintptr_t>(&actualOpen));
 jumpTo(0x5387D0,reinterpret_cast<uintptr_t>(&mockSetDir));
 if(!textOnly){
  bytes(0x5BA697,{0x83,0xC4,0x04,0x5E,0x83,0xC4,0x04,0xC3});
  jumpTo(0x7320B5,reinterpret_cast<uintptr_t>(&privateLoad));jumpTo(0x71A210,reinterpret_cast<uintptr_t>(&nothing));
  jumpTo(0x731C80,reinterpret_cast<uintptr_t>(&privateSlot));jumpTo(0x731CD0,reinterpret_cast<uintptr_t>(&slotNothing));
  jumpTo(0x7316A0,reinterpret_cast<uintptr_t>(&nothing));jumpTo(0x7316B0,reinterpret_cast<uintptr_t>(&nothing));
  jumpTo(0x7319C0,reinterpret_cast<uintptr_t>(&slotNothing));jumpTo(0x731A00,reinterpret_cast<uintptr_t>(&slotNothing));
  jumpTo(0x727270,reinterpret_cast<uintptr_t>(&texture));jumpTo(0x727240,reinterpret_cast<uintptr_t>(&deleteTexture));
 }
 auto readSetting=[&](unsigned slot,const char* key){char b[512]{};assert(GetPrivateProfileStringA(std::to_string(slot).c_str(),key,"",b,sizeof(b),(assets+"text\\languages.ini").c_str()));return std::string(b);};
 const auto mainFile=reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2),missionFile=reinterpret_cast<const char*(__cdecl*)()>(0x69FCF5);
 const auto open=reinterpret_cast<FILE*(__cdecl*)(const char*,const char*)>(0x538900);
 for(unsigned i=0;i<11;++i){
  *reinterpret_cast<uint8_t*>(0xBA67CC)=uint8_t(i);assert(mockSetDir("TEXT")==0);const auto before=cwd();
  std::array<char,128> nativeBefore{};std::memcpy(nativeBefore.data(),reinterpret_cast<void*>(0xB71A60),128);
  const auto filename=readSetting(i+1,"GXT_file");assert(filename==mainFile());
  if(!textOnly&&i==0)reinterpret_cast<void(__cdecl*)()>(0x5BA690)();
  assert(cwd()==before && !std::memcmp(nativeBefore.data(),reinterpret_cast<void*>(0xB71A60),128));
  const auto expected=readFile(assets+"text\\"+filename);
  // Direct native call and a std.text-style forwarded filename hit the same validated bundle file.
  std::string upper=filename;for(auto& c:upper)if(c>='a'&&c<='z')c+='A'-'a';
  for(const auto& request:std::vector<std::string>{filename,bundle?gameRoot+"modloader\\Other pack\\"+filename:filename,upper}){
   FILE* f=open(request.c_str(),"rb");assert(f);std::vector<uint8_t> got(expected.size());assert(std::fread(got.data(),1,got.size(),f)==got.size());std::fclose(f);assert(got==expected);
   if(bundle)assert(fileCall==assets+"text\\"+filename);else assert(fileCall==request);
  }
  assert(filename==missionFile());FILE* f=open(filename.c_str(),"rb");assert(f);
  // Seek using an actual mission table offset from the bundled GXT, proving MAIN
  // and mission loads refer to the same physical file without rewriting offsets.
  const uint32_t tableBytes=ll::u32(expected,8);bool found=false;
  for(size_t p=12;p<12+tableBytes;p+=12)if(std::memcmp(expected.data()+p,"MAIN",4)){
   uint32_t offset=ll::u32(expected,p+8);assert(std::fseek(f,long(offset),SEEK_SET)==0);char name[8]{};assert(std::fread(name,1,8,f)==8);assert(!std::memcmp(name,expected.data()+p,8));found=true;break;
  }
  assert(found);std::fclose(f);assert(cwd()==before);
  if(!textOnly){assert(*reinterpret_cast<uint8_t*>(0xC71A94)>=2);assert(textureCall.find(assets)==0);}
  *reinterpret_cast<uint8_t*>(0xBA6748+0x15D)=28;
  std::array<uint8_t,0x4C> fontBefore{};std::memcpy(fontBefore.data(),reinterpret_cast<void*>(0xC71A54),fontBefore.size());
  const auto drawsBefore=noticeDraws;
  assert(!std::strcmp(titleLookup(reinterpret_cast<void*>(0xC1B340),"FEH_LAN"),"native-text"));
  assert(noticeDraws==drawsBefore+1 && !std::memcmp(fontBefore.data(),reinterpret_cast<void*>(0xC71A54),fontBefore.size()));
  *reinterpret_cast<uint8_t*>(0xBA6748+0x15D)=oldScreen;
  if(lateMods)assert(!std::memcmp(unitsHook.data(),reinterpret_cast<void*>(0x56D220),5));
  else assert(reinterpret_cast<unsigned(__cdecl*)()>(0x56D220)()==unsigned(std::stoi(readSetting(i+1,"Is_system_metric"))));
 }
 if(bundle){
  const auto patchOperand=[](uintptr_t site,uintptr_t pointer){const auto value=static_cast<uint32_t>(pointer);std::memcpy(reinterpret_cast<void*>(site),&value,4);};
  constexpr size_t tableSize=0x3000;
  auto* moved=static_cast<uint8_t*>(VirtualAlloc(nullptr,tableSize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));assert(moved);
  std::memcpy(moved,reinterpret_cast<void*>(0x8CE008),tableSize);
  auto* visible=reinterpret_cast<ll::Menu*>(moved+10+28*0xE2);*visible=ll::makeMenu(11,0);
  for(const auto& ref:menu_binding::references)patchOperand(ref.operand(),reinterpret_cast<uintptr_t>(moved)+10+ref.field);
  *reinterpret_cast<uint8_t*>(0xBA6748+0x15D)=28;
  auto title=reinterpret_cast<const char*(__thiscall*)(void*,const char*)>(0x57967E);
  assert(!std::strcmp(title(reinterpret_cast<void*>(0xC1B340),"FEH_LAN"),"native-text"));
  assert((*visible)[0].key==ll::labelKey(9)); // title refreshed page BEFORE drawing any row
  auto selectedLabel=reinterpret_cast<const char*(__thiscall*)(void*,const char*)>(0x57981A);
  assert(!std::strcmp(selectedLabel(reinterpret_cast<void*>(0xC1B340),(*visible)[0].key),"Indonesian (ELL)"));
  for(const auto& ref:menu_binding::references)patchOperand(ref.operand(),0x8CE012+ref.field);
  *reinterpret_cast<uint8_t*>(0xBA6748+0x15D)=0;VirtualFree(moved,0,MEM_RELEASE);
 }
 // Do not hijack another GXT, a metrics file or a write-mode request.
 for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{gameRoot+"text\\unrelated.gxt","rb"},{gameRoot+"text\\ordinary.dat","rb"},{gameRoot+"text\\"+readSetting(11,"GXT_file"),"ab"}}){
  FILE* f=open(pair.first.c_str(),pair.second.c_str());if(pair.second=="rb")assert(f);if(f)std::fclose(f);assert(fileCall==pair.first);
 }
 if(mode=="iat")assert(translatedCalls==0);
 if(mode=="duplicate"){ 
  const auto current=*reinterpret_cast<uint8_t*>(0x6A01D2);
  HMODULE second=LoadLibraryA((ll::parentDirectory(modulePath)+"ELL-copy.asi").c_str());assert(second);
  auto secondStatus=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(second,"ExtendedLanguageLoaderStatus"));assert(secondStatus && !secondStatus());assert(*reinterpret_cast<uint8_t*>(0x6A01D2)==current);
 }
 std::cout<<"PASS: "<<(bundle?"ASI-local bundle":"root")<<"; all eleven MAIN/mission GXT opens and real mission seeks, scoped read-only routing, native CWD preservation, fonts from selected installation, duplicate/refusal controls. FAKE graphics; NOT actual ModLoader/GTA acceptance.\n";
}
