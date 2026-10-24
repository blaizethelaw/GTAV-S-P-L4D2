// Development diagnostic only. Not a runtime dependency or release component.
// Only nativeInit (address lookup) is called. No nativeCall or game handler is invoked.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <winver.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include "flow.hpp"

using RegisterFn = void (__cdecl*)(HMODULE, void(*)());
using WaitFn = void (__cdecl*)(DWORD);
using InitFn = void (__cdecl*)(unsigned long long);
struct Module { HMODULE handle{}; uintptr_t base{}; uint32_t size{}; uint32_t timestamp{}; std::wstring path; };
static HMODULE g_self{};
static Module g_game{}, g_shv{};
static WaitFn g_wait{};
static InitFn g_init{};
static std::wstring g_run;
static std::string g_hostHash, g_shvHash, g_probeHash, g_error, g_version;
static volatile LONG g_started=0, g_state=0;
static size_t g_resolved=0, g_memoryChecks=0;
static FILE* g_csv{};

static std::wstring join(const std::wstring& dir, const wchar_t* name) { return dir + L"\\" + name; }
static FILE* openRun(const wchar_t* name, const wchar_t* mode) { return _wfopen(join(g_run, name).c_str(), mode); }
static void logmsg(const char* format, ...) {
    char text[1024]; va_list args; va_start(args, format); vsnprintf(text, sizeof(text), format, args); va_end(args);
    SYSTEMTIME now{}; GetSystemTime(&now);
    FILE* f = g_run.empty() ? nullptr : openRun(L"log.txt", L"ab");
    if (f) { fprintf(f, "%02u:%02u:%02u.%03u %s\n", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, text); fclose(f); }
    OutputDebugStringA(text);
}
static std::string escaped(const std::string& input) {
    std::string result;
    for (char c : input) { if (c == '\\' || c == '"') result += '\\'; if (c >= ' ') result += c; }
    return result;
}
static bool manifest(const char* status) {
    FILE* f = openRun(L"manifest.tmp", L"wb"); if (!f) return false;
    int result = fprintf(f,
        "{\n  \"schemaVersion\": 1,\n  \"probeId\": \"%s\",\n  \"status\": \"%s\",\n"
        "  \"hostVersion\": \"%s\",\n  \"hostSha256\": \"%s\",\n  \"hostTimestamp\": %u,\n  \"hostImageSize\": %u,\n"
        "  \"shvSha256\": \"%s\",\n  \"probeSha256\": \"%s\",\n  \"nativeCount\": %zu,\n  \"resolvedCount\": %zu,\n"
        "  \"executableMemoryChecks\": %zu,\n  \"launchGuard\": \"-nobattleye required\",\n  \"error\": \"%s\"\n}\n",
        setting("probe_id"), status, g_version.c_str(), g_hostHash.c_str(), g_game.timestamp, g_game.size,
        g_shvHash.c_str(), g_probeHash.c_str(), kNativeCount, g_resolved, g_memoryChecks, escaped(g_error).c_str());
    bool ok = result > 0 && fflush(f) == 0; if (fclose(f) != 0) ok = false;
    return ok && MoveFileExW(join(g_run, L"manifest.tmp").c_str(), join(g_run, L"manifest.json").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
static void fail(const char* message, const char* detail="") {
    g_error = std::string(message) + (detail[0] ? ": " : "") + detail;
    logmsg("FAILED: %s (Win32=%lu)", g_error.c_str(), GetLastError());
    if (g_csv) { fclose(g_csv); g_csv=nullptr; }
    manifest("failed"); InterlockedExchange(&g_state, 3);
}
static bool readMemory(uintptr_t address, void* out, size_t bytes) {
    SIZE_T count=0; return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, bytes, &count) && count == bytes;
}
static bool moduleInfo(HMODULE handle, Module& result) {
    if (!handle) return false;
    result.handle=handle; result.base=reinterpret_cast<uintptr_t>(handle);
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if (!readMemory(result.base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x100000) return false;
    if (!readMemory(result.base + static_cast<uint32_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    result.size=nt.OptionalHeader.SizeOfImage; result.timestamp=nt.FileHeader.TimeDateStamp;
    wchar_t path[32768]; DWORD length=GetModuleFileNameW(handle,path,32768);
    if (!length || length >= 32768 || result.size < 4096) return false;
    result.path.assign(path,length); return true;
}
static bool region(const Module& mod, uintptr_t address, size_t bytes, bool executable, bool writable) {
    if (address < mod.base || bytes > mod.size || address-mod.base > mod.size-bytes) return false;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &m, sizeof(m)) || m.State != MEM_COMMIT || m.Type != MEM_IMAGE || m.AllocationBase != mod.handle || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    uintptr_t end=reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
    if (address > end || bytes > end-address) return false;
    DWORD p=m.Protect & 0xFF;
    if (executable && p != PAGE_EXECUTE && p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY) return false;
    if (writable && p != PAGE_READWRITE && p != PAGE_WRITECOPY && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY) return false;
    return true;
}
static bool sha256(const std::wstring& path, std::string& out) {
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_HASH_HANDLE hash{}; DWORD size=0, got=0; bool ok=false;
    FILE* file=_wfopen(path.c_str(),L"rb"); if (!file) return false;
    std::vector<unsigned char> object;
    unsigned char buffer[65536], digest[32];
    if (BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0) >= 0 && BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&got,0) >= 0 && size < 1048576) {
        object.resize(size);
        if (BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0) >= 0) {
            ok=true;
            for (;;) { size_t n=fread(buffer,1,sizeof(buffer),file); if (n && BCryptHashData(hash,buffer,static_cast<ULONG>(n),0) < 0) { ok=false; break; } if (n < sizeof(buffer)) { if (ferror(file)) ok=false; break; } }
            if (ok) ok=BCryptFinishHash(hash,digest,sizeof(digest),0) >= 0;
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg,0);
    fclose(file);
    if (ok) { char hex[65]; for (size_t i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]); out=hex; }
    return ok;
}
static bool version(const std::wstring& path, std::string& result) {
    DWORD ignored=0, size=GetFileVersionInfoSizeW(path.c_str(),&ignored); if (!size || size > 2097152) return false;
    std::vector<unsigned char> data(size); if (!GetFileVersionInfoW(path.c_str(),0,size,data.data())) return false;
    VS_FIXEDFILEINFO* info{}; UINT bytes=0;
    if (!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&bytes) || bytes < sizeof(*info) || info->dwSignature != 0xFEEF04BD) return false;
    char text[80]; snprintf(text,sizeof(text),"%u.%u.%u.%u",HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS)); result=text; return true;
}
static bool hasRequiredToken() {
    int count=0; LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count); if (!args) return false;
    bool found=false; for (int i=1;i<count;++i) if (_wcsicmp(args[i],L"-nobattleye")==0) found=true;
    LocalFree(args); return found;
}
static bool setupOutput() {
    wchar_t local[32768]; DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768); if (!length || length>=32768) return false;
    std::wstring root=join(local,L"NativeDump"), runs=join(root,L"runs");
    if (!CreateDirectoryW(root.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
    if (!CreateDirectoryW(runs.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
    SYSTEMTIME now{}; GetSystemTime(&now); wchar_t leaf[96];
    swprintf(leaf,96,L"%04u%02u%02uT%02u%02u%02u%03uZ-pid%lu",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,GetCurrentProcessId());
    g_run=join(runs,leaf); if (!CreateDirectoryW(g_run.c_str(),nullptr)) return false;
    FILE* marker=_wfopen(join(root,L"latest-run.tmp").c_str(),L"wb"); if (!marker) return false;
    char utf8[32768]; int n=WideCharToMultiByte(CP_UTF8,0,g_run.c_str(),-1,utf8,sizeof(utf8),nullptr,nullptr);
    bool ok=n>0 && fprintf(marker,"%s\n",utf8)>0; if (fclose(marker)!=0) ok=false;
    return ok && MoveFileExW(join(root,L"latest-run.tmp").c_str(),join(root,L"latest-run.txt").c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
struct CaptureApi {
    void wait(unsigned ms) { g_wait(ms); }
    void progress(const char* stage,size_t count) { logmsg("%s %zu/%zu",stage,count,kNativeCount); }
    void fail(const char* message,const char* detail) { ::fail(message,detail); }
    bool resolve(const NativeSpec& spec,uint32_t& rva) {
        uintptr_t slot=g_shv.base + strtoull(setting("last_native_rva"),nullptr,0);
        if (!region(g_shv,slot,sizeof(uint64_t),false,true)) { logmsg("Invalid SHV resolver slot permissions"); return false; }
        uint64_t zero=0, address=0; SIZE_T written=0;
        if (!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(slot),&zero,sizeof(zero),&written) || written!=sizeof(zero)) return false;
        g_init(spec.hash);
        if (!readMemory(slot,&address,sizeof(address)) || !address || !region(g_game,address,1,true,false)) { logmsg("Rejected null/out-of-image/non-executable resolver result for %s",spec.name); return false; }
        ++g_memoryChecks; rva=static_cast<uint32_t>(address-g_game.base);
        if (spec.expectedRva && rva!=spec.expectedRva) { logmsg("Sentinel %s expected=%08X actual=%08X",spec.name,spec.expectedRva,rva); return false; }
        return true;
    }
    bool write(const NativeSpec& spec,uint32_t rva) {
        if (!g_csv || fprintf(g_csv,"%s,%016llX,%016llX,ok\n",spec.name,static_cast<unsigned long long>(spec.hash),static_cast<unsigned long long>(rva))<0 || fflush(g_csv)!=0) return false;
        ++g_resolved; return true;
    }
    bool finish() {
        if (!g_csv) return false;
        bool ok=fclose(g_csv)==0; g_csv=nullptr;
        if (!ok || !manifest("passed")) { ::fail("Cannot finalize capture manifest"); return false; }
        FILE* f=openRun(L"done.txt",L"wb"); if (!f) { ::fail("Cannot write completion marker"); return false; }
        ok=fprintf(f,"probe=%s\nresolved=%zu\nexecutableMemoryChecks=%zu\n",setting("probe_id"),g_resolved,g_memoryChecks)>0;
        if (fclose(f)!=0) ok=false;
        if (!ok) { ::fail("Cannot flush completion marker"); return false; }
        logmsg("PASSED: %zu address records, %zu executable-memory checks; no game handlers invoked",g_resolved,g_memoryChecks);
        InterlockedExchange(&g_state,2); return true;
    }
};
static void ScriptMain() {
    InterlockedExchange(&g_state,1); logmsg("ScriptMain entered; yielding for warmup frames");
    g_csv=openRun(L"natives.csv",L"wb");
    if (!g_csv || fprintf(g_csv,"name,hash,rva,status\n")<0) fail("Cannot open address inventory");
    else { CaptureApi api; captureInventory(api,static_cast<unsigned>(strtoul(setting("warmup_frames"),nullptr,10))); }
    // SHV invokes the entry point once. Remain on its script fiber and yield.
    for (;;) g_wait(1000);
}
static DWORD WINAPI worker(LPVOID) {
    if (!setupOutput()) { OutputDebugStringA("NativeDump v2: cannot initialize diagnostic directory"); return 0; }
    logmsg("Starting %s",setting("probe_id")); manifest("starting");
    if (!moduleInfo(GetModuleHandleW(nullptr),g_game) || !version(g_game.path,g_version)) { fail("Cannot inspect host module/version"); return 0; }
    if (g_version!=setting("game_version") || g_game.timestamp!=strtoul(setting("game_timestamp"),nullptr,10) || g_game.size!=strtoul(setting("game_image_size"),nullptr,10)) { fail("Unsupported GTA executable fingerprint"); return 0; }
    auto sep=g_game.path.find_last_of(L"\\/"); std::wstring basename=g_game.path.substr(sep==std::wstring::npos ? 0 : sep+1);
    if (_wcsicmp(basename.c_str(),L"GTA5.exe")!=0 || !hasRequiredToken()) { fail("Legacy GTA5.exe with explicit -nobattleye launch token required"); return 0; }
    Module self{};
    if (!sha256(g_game.path,g_hostHash) || !moduleInfo(g_self,self) || !sha256(self.path,g_probeHash)) { fail("Cannot fingerprint host/probe files"); return 0; }
    // Validate the dev-only helper on disk before loading it or calling any export.
    std::wstring helperPath=g_game.path.substr(0,sep+1) + L"ScriptHookV.dll";
    if (!sha256(helperPath,g_shvHash) || g_shvHash!=setting("shv_sha256")) { fail("ScriptHookV helper fingerprint mismatch"); return 0; }
    HMODULE helper=GetModuleHandleW(helperPath.c_str()); if (!helper) helper=LoadLibraryW(helperPath.c_str());
    if (!moduleInfo(helper,g_shv) || g_shv.timestamp!=strtoul(setting("shv_timestamp"),nullptr,10) || g_shv.size!=strtoul(setting("shv_image_size"),nullptr,10)) { fail("Loaded ScriptHookV image fingerprint mismatch"); return 0; }
    std::string loadedHash;
    if (!sha256(g_shv.path,loadedHash) || loadedHash!=g_shvHash) { fail("Loaded ScriptHookV path fingerprint mismatch"); return 0; }
    FARPROC reg=GetProcAddress(helper,setting("register_export")), wait=GetProcAddress(helper,setting("wait_export")), init=GetProcAddress(helper,setting("init_export"));
    if (!region(g_shv,reinterpret_cast<uintptr_t>(reg),1,true,false) || !region(g_shv,reinterpret_cast<uintptr_t>(wait),1,true,false) || !region(g_shv,reinterpret_cast<uintptr_t>(init),1,true,false)) { fail("Required ScriptHookV exports absent/outside executable module memory"); return 0; }
    // memcpy preserves function pointer type without compiler cast-function-type warnings.
    RegisterFn registerScript{}; static_assert(sizeof(registerScript)==sizeof(reg));
    memcpy(&registerScript,&reg,sizeof(reg)); memcpy(&g_wait,&wait,sizeof(wait)); memcpy(&g_init,&init,sizeof(init));
    manifest("registered"); logmsg("Registering script; waiting for Story Mode script scheduler"); registerScript(g_self,ScriptMain);
    for (unsigned seconds=0;seconds<600;seconds+=5) { Sleep(5000); LONG state=InterlockedCompareExchange(&g_state,0,0); if (state>=2) return 0; if (seconds%30==0) logmsg("Awaiting script progress: state=%ld elapsed=%u seconds",state,seconds+5); }
    logmsg("Script has not completed after 600 seconds; leave game running or inspect log. No automatic game close."); return 0;
}
static void startOnce() {
    if (InterlockedCompareExchange(&g_started,1,0)!=0) return;
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
    if (thread) CloseHandle(thread); else { OutputDebugStringA("NativeDump v2: worker creation failed"); InterlockedExchange(&g_started,0); }
}
extern "C" __declspec(dllexport) void InitializeASI() { startOnce(); }
BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { g_self=module; DisableThreadLibraryCalls(module); startOnce(); }
    return TRUE;
}
