#include <windows.h>
#include <psapi.h>
#include <fstream>
#include <string>
#include <algorithm>

#define KURO_AOB "\x75\x44\x48\x8B\x5C\x24\x28"  // Kuro AOB
#define KURO_MASK "xxxxxxx"
#define KURO_OFFSET 0

#define KURO2_AOB "\xED\x75\x59\xC7\x87\xC4\x22\x40\x00\x0A\x00\x00\x00"  // Kuro 2 AOB
#define KURO2_MASK "xxxxxxxxxxxx"
#define KURO2_OFFSET 1

std::string game = "kuro";  // Default: kuro
bool disableDLCCheck = false;  // Default: false

// Convert string to lowercase
std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// Function to read INI file
void ReadConfig() {
    std::ifstream iniFile("kuro_dlc_patch.ini");
    bool iniExists = iniFile.is_open();
    
    if (iniExists) {
        std::string line;
        while (std::getline(iniFile, line)) {
            size_t pos = line.find("=");
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = ToLower(line.substr(pos + 1));

                if (key == "Game") {
                    if (value == "kuro" || value == "kuro2") {
                        game = value;
                    }
                } else if (key == "DisableDLCDependencyCheck") {
                    disableDLCCheck = (value == "true");
                }
            }
        }
        iniFile.close();
    }

    // If INI does not exist, create one with correct format
    if (!iniExists) {
        std::ofstream outFile("kuro_dlc_patch.ini");
        outFile << "# Set to 'kuro' for Trails through Daybreak or 'kuro2' for Trails through Daybreak II\n";
        outFile << "Game=kuro\n\n";
        outFile << "# When set to 'true', disables the check for purchased DLC when loading a save file with DLC redeemed.\n";
        outFile << "DisableDLCDependencyCheck=false\n";
        outFile.close();
    }
}

// Function to find pattern in memory
DWORD_PTR FindPattern(BYTE* pattern, const char* mask, DWORD_PTR base, DWORD size) {
    for (DWORD i = 0; i < size; i++) {
        bool found = true;
        for (DWORD j = 0; mask[j]; j++) {
            if (mask[j] != '?' && pattern[j] != *(BYTE*)(base + i + j)) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return 0;
}

// Patch function
void PatchMemory() {
    ReadConfig();
    if (!disableDLCCheck) return;  // Do nothing if disabled

    const char* targetExe = (game == "kuro2") ? "kuro2.exe" : "kuro.exe";
    HMODULE hModule = GetModuleHandleA(targetExe);
    if (!hModule) return;

    MODULEINFO modInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
    DWORD_PTR baseAddr = (DWORD_PTR)modInfo.lpBaseOfDll;
    DWORD size = modInfo.SizeOfImage;

    // Set correct AOB & offset
    BYTE* aob = (game == "kuro2") ? (BYTE*)KURO2_AOB : (BYTE*)KURO_AOB;
    const char* mask = (game == "kuro2") ? KURO2_MASK : KURO_MASK;
    int offset = (game == "kuro2") ? KURO2_OFFSET : KURO_OFFSET;

    DWORD_PTR address = FindPattern(aob, mask, baseAddr, size);
    if (address) {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(address + offset), 2, PAGE_EXECUTE_READWRITE, &oldProtect);
        *(BYTE*)(address + offset) = 0xEB;  // Modify instruction
        VirtualProtect((LPVOID)(address + offset), 2, oldProtect, &oldProtect);
    }
}

// Entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)PatchMemory, 0, 0, 0);
    }
    return TRUE;
}
