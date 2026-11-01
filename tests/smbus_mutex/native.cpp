// SPDX-License-Identifier: GPL-2.0-or-later
// Real Win32 synchronization only. No OpenRGB detector, PawnIO or device API.
#include "../../i2c_smbus/Windows/SharedSMBusMutex.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <stdexcept>

static void Require(bool value, const char* operation)
{
    if(!value) throw std::runtime_error(std::string(operation) + " win32=" + std::to_string(GetLastError()));
}
static void Marker(const std::string& path)
{
    std::ofstream file(path); file << "ready\n"; file.close();
    Require(!file.fail(), "write marker");
}
static void Await(const std::string& path)
{
    const ULONGLONG deadline = GetTickCount64() + 30000;
    while(!std::filesystem::exists(path))
    {
        Require(GetTickCount64() < deadline, "marker timeout");
        Sleep(25);
    }
}
static void MediumIdentity()
{
    HANDLE original = nullptr, restricted = nullptr;
    Require(OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &original) != 0, "process token");
    BYTE sid[SECURITY_MAX_SID_SIZE]; DWORD size = sizeof(sid);
    Require(CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid, &size) != 0, "admin SID");
    SID_AND_ATTRIBUTES deny = {sid, 0};
    Require(CreateRestrictedToken(original, DISABLE_MAX_PRIVILEGE, 1, &deny, 0, nullptr, 0, nullptr, &restricted) != 0, "restricted token");
    CloseHandle(original);
    PSID medium = nullptr;
    Require(ConvertStringSidToSidA("S-1-16-8192", &medium) != 0, "medium SID");
    TOKEN_MANDATORY_LABEL label = {{medium, SE_GROUP_INTEGRITY}};
    Require(SetTokenInformation(restricted, TokenIntegrityLevel, &label,
            static_cast<DWORD>(sizeof(label) + GetLengthSid(medium))) != 0, "medium label");
    LocalFree(medium);
    Require(ImpersonateLoggedOnUser(restricted) != 0, "impersonate restricted caller");
    CloseHandle(restricted);
    BOOL admin = TRUE;
    Require(CheckTokenMembership(nullptr, sid, &admin) != 0 && !admin, "caller must not be admin");
    HANDLE current = nullptr;
    Require(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &current) != 0, "thread token");
    DWORD needed = 0;
    GetTokenInformation(current, TokenIntegrityLevel, nullptr, 0, &needed);
    std::vector<BYTE> buffer(needed);
    Require(GetTokenInformation(current, TokenIntegrityLevel, buffer.data(), needed, &needed) != 0, "verify integrity");
    auto il = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data())->Label.Sid;
    Require(*GetSidSubAuthority(il, *GetSidSubAuthorityCount(il) - 1) == SECURITY_MANDATORY_MEDIUM_RID, "not medium integrity");
    CloseHandle(current);
}
static void SystemIdentity()
{
    HANDLE token = nullptr;
    Require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != 0, "system token");
    DWORD needed = 0; GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    std::vector<BYTE> buffer(needed);
    Require(GetTokenInformation(token, TokenUser, buffer.data(), needed, &needed) != 0, "system user");
    Require(IsWellKnownSid(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, WinLocalSystemSid) != 0, "caller must be SYSTEM");
    CloseHandle(token);
}

int main(int argc, char** argv)
{
    try
    {
        char ci[16] = {};
        Require(GetEnvironmentVariableA("GITHUB_ACTIONS", ci, sizeof(ci)) && std::string(ci) == "true", "cloud runner only");
        Require(argc == 7, "arguments: mode role mutex ready stop result");
        const std::string mode = argv[1], role = argv[2], name = argv[3];
        Require(name.rfind("Global\\OpenRGB.MutexTests.", 0) == 0 && name.size() > 55, "isolated test name required");
        if(role == "medium") MediumIdentity();
        else if(role == "system") SystemIdentity();
        else Require(role == "runner", "unknown role");

        HANDLE mutex = nullptr;
        if(mode == "legacy")
        {
            PSECURITY_DESCRIPTOR sd = nullptr;
            Require(ConvertStringSecurityDescriptorToSecurityDescriptorA(
                "D:(A;;GA;;;SY)(A;;0x120001;;;BA)S:(ML;;NW;;;ME)", 1, &sd, nullptr) != 0, "legacy descriptor");
            SECURITY_ATTRIBUTES attributes = {sizeof(attributes), sd, FALSE};
            mutex = CreateMutexA(&attributes, FALSE, name.c_str());
            LocalFree(sd);
        }
        else mutex = CreateSharedSMBusMutex(name.c_str());

        if(mode == "denied")
        {
            Require(mutex == nullptr && GetLastError() == ERROR_ACCESS_DENIED, "expected access denial");
        }
        else
        {
            Require(mutex != nullptr, "create/open isolated mutex");
            if(mode == "hold" || mode == "legacy")
            {
                Marker(argv[4]); Await(argv[5]);
            }
            else if(mode == "lock")
            {
                SharedSMBusLock lock(mutex, true);
                Require(lock.Ready(), "owner acquisition");
                Marker(argv[4]); Await(argv[5]);
            }
            else if(mode == "probe" || mode == "timeout")
            {
                const ULONGLONG start = GetTickCount64();
                SharedSMBusLock lock(mutex, true);
                Require(lock.Ready() == (mode == "probe"), "probe result");
                if(mode == "timeout") Require(GetTickCount64() - start >= 2500, "bounded wait too short");
            }
            else if(mode == "abandoned")
            {
                DWORD acquired = WAIT_FAILED;
                std::thread owner([&] { acquired = WaitForSingleObject(mutex, 3000); });
                owner.join(); // Owner exits while holding the mutex.
                Require(acquired == WAIT_OBJECT_0, "abandoned setup");
                { SharedSMBusLock lock(mutex, true); Require(lock.Ready(), "abandoned guard"); }
                DWORD after = WAIT_FAILED;
                std::thread next([&] {
                    after = WaitForSingleObject(mutex, 0);
                    if(after == WAIT_OBJECT_0 || after == WAIT_ABANDONED) ReleaseMutex(mutex);
                });
                next.join(); Require(after == WAIT_OBJECT_0, "ownership released after abandonment");
            }
            else throw std::runtime_error("unknown mode");
            CloseHandle(mutex);
        }
        RevertToSelf();
        std::ofstream result(argv[6]); result << "PASS " << mode << " " << role << "\n";
        Require(result.good(), "result write");
        return 0;
    }
    catch(const std::exception& error)
    {
        RevertToSelf();
        std::cerr << error.what() << '\n';
        if(argc == 7) { std::ofstream result(argv[6]); result << "FAIL " << error.what() << '\n'; }
        return 1;
    }
}
