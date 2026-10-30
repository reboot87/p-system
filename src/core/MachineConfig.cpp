#include "MachineConfig.h"
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace machine
{
    std::wstring ConfigDir()
    {
        wchar_t* env = nullptr;
        size_t len = 0;
        std::wstring dir;
        if (!_wdupenv_s(&env, &len, L"PROGRAMDATA") && env) {
            dir = std::wstring(env) + L"\\AireRecord";
            free(env);
        } else {
            dir = L"C:\\ProgramData\\AireRecord";
        }
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }

    std::wstring IniPath()
    {
        return ConfigDir() + L"\\service.ini";
    }

    std::wstring Get(const wchar_t* section, const wchar_t* key,
                     const wchar_t* def)
    {
        wchar_t buf[512] = {};
        std::wstring ini = IniPath();
        GetPrivateProfileStringW(section, key, def, buf, 512, ini.c_str());
        return buf;
    }

    void Set(const wchar_t* section, const wchar_t* key,
             const wchar_t* value)
    {
        WritePrivateProfileStringW(section, key, value, IniPath().c_str());
    }

    bool Exists()
    {
        std::wstring ini = IniPath();
        return GetFileAttributesW(ini.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
}
