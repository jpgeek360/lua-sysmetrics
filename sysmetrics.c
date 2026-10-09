#define LUA_BUILD_AS_DLL
#include <windows.h>
#include <lua.h>
#include <lauxlib.h>

// Variáveis estáticas para cálculo de variação (delta) da CPU
static ULONGLONG prev_idle = 0;
static ULONGLONG prev_kernel = 0;
static ULONGLONG prev_user = 0;

static ULONGLONG filetime_to_uint64(const FILETIME *ft) {
    return (((ULONGLONG)ft->dwHighDateTime) << 32) | ft->dwLowDateTime;
}

// 1. Métricas de CPU (Win32 API: GetSystemTimes)
static int l_get_cpu_usage(lua_State *L) {
    FILETIME idle_ft, kernel_ft, user_ft;
    
    if (!GetSystemTimes(&idle_ft, &kernel_ft, &user_ft)) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    ULONGLONG idle = filetime_to_uint64(&idle_ft);
    ULONGLONG kernel = filetime_to_uint64(&kernel_ft);
    ULONGLONG user = filetime_to_uint64(&user_ft);

    ULONGLONG idle_diff = idle - prev_idle;
    ULONGLONG kernel_diff = kernel - prev_kernel;
    ULONGLONG user_diff = user - prev_user;

    prev_idle = idle;
    prev_kernel = kernel;
    prev_user = user;

    ULONGLONG total_sys = kernel_diff + user_diff;
    if (total_sys == 0) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    double cpu_usage = ((double)(total_sys - idle_diff) / (double)total_sys) * 100.0;
    if (cpu_usage < 0.0) cpu_usage = 0.0;
    if (cpu_usage > 100.0) cpu_usage = 100.0;

    lua_pushnumber(L, cpu_usage);
    return 1;
}

// 2. Métricas de RAM (Win32 API: GlobalMemoryStatusEx)
static int l_get_ram_usage(lua_State *L) {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);

    if (GlobalMemoryStatusEx(&statex)) {
        lua_pushnumber(L, (double)statex.dwMemoryLoad);
    } else {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}

// 3. Métricas de HD/Disco (Win32 API: GetDiskFreeSpaceExW)
static int l_get_disk_usage(lua_State *L) {
    const char *drive = luaL_optstring(L, 1, "C:\\");
    
    // Converte string UTF-8 do Lua para Wide Char (UTF-16) da Win32 API
    wchar_t wDrive[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, drive, -1, wDrive, MAX_PATH);

    ULARGE_INTEGER free_bytes, total_bytes, total_free;

    if (GetDiskFreeSpaceExW(wDrive, &free_bytes, &total_bytes, &total_free)) {
        if (total_bytes.QuadPart > 0) {
            double used = (double)(total_bytes.QuadPart - free_bytes.QuadPart);
            double usage = (used / (double)total_bytes.QuadPart) * 100.0;
            lua_pushnumber(L, usage);
            return 1;
        }
    }
    lua_pushnumber(L, 0.0);
    return 1;
}

// Tabela de registro de funções expostas para o Lua
static const struct luaL_Reg sysmetrics[] = {
    {"get_cpu_usage", l_get_cpu_usage},
    {"get_ram_usage", l_get_ram_usage},
    {"get_disk_usage", l_get_disk_usage},
    {NULL, NULL}
};

// Ponto de entrada exportado para o Lua (luaopen_<nome_da_dll>)
__declspec(dllexport) int luaopen_sysmetrics(lua_State *L) {
    luaL_newlib(L, sysmetrics);
    return 1;
}