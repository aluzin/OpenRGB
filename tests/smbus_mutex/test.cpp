// SPDX-License-Identifier: GPL-2.0-or-later
// Freestanding: no CRT, Windows API imports, drivers, threads or real mutexes.
#include "../../i2c_smbus/Windows/SharedSMBusMutex.h"

static int failures, checks;
static int created, opened, freed, waited, released, transfers;
static int descriptor_token, mutex_token;
static bool convert_ok, create_ok, open_ok;
static DWORD last_error, create_error, open_error, wait_result;

static void Check(bool value) { ++checks; if(!value) ++failures; }
static bool Equal(const char* a, const char* b)
{
    while(*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static void Reset()
{
    created = opened = freed = waited = released = transfers = 0;
    convert_ok = create_ok = open_ok = true;
    last_error = create_error = open_error = 0;
    wait_result = WAIT_OBJECT_0;
}

BOOL ConvertStringSecurityDescriptorToSecurityDescriptorA(const char* sddl, DWORD revision, void** out, DWORD*)
{
    Check(Equal(sddl, "D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AU)S:(ML;;NW;;;ME)"));
    Check(revision == SDDL_REVISION_1);
    if(!convert_ok) { last_error = 87; return FALSE; }
    *out = &descriptor_token;
    return 1;
}
HANDLE CreateMutexA(SECURITY_ATTRIBUTES* attributes, BOOL owner, const char* name)
{
    ++created;
    Check(attributes && attributes->lpSecurityDescriptor == &descriptor_token);
    Check(attributes && attributes->nLength == sizeof(SECURITY_ATTRIBUTES));
    Check(attributes && !attributes->bInheritHandle && !owner);
    Check(Equal(name, "Global\\Access_SMBUS.HTP.Method"));
    last_error = create_error;
    return create_ok ? &mutex_token : nullptr;
}
HANDLE OpenMutexA(DWORD rights, BOOL inherit, const char* name)
{
    ++opened;
    Check(rights == (SYNCHRONIZE | MUTEX_MODIFY_STATE));
    Check(!inherit && Equal(name, "Global\\Access_SMBUS.HTP.Method"));
    last_error = open_error;
    return open_ok ? &mutex_token : nullptr;
}
DWORD GetLastError() { return last_error; }
void SetLastError(DWORD error) { last_error = error; }
void* LocalFree(void* p)
{
    Check(p == &descriptor_token);
    ++freed;
    last_error = 999; // Verify cleanup cannot overwrite the diagnostic error.
    return nullptr;
}
DWORD WaitForSingleObject(HANDLE handle, DWORD timeout)
{
    Check(handle == &mutex_token && timeout == 3000);
    ++waited;
    return wait_result;
}
BOOL ReleaseMutex(HANDLE handle)
{
    Check(handle == &mutex_token);
    ++released;
    return 1;
}

// Simulate a transport callback, using the production guard. No bus API exists
// in this executable; verify short-circuit and release on an early error return.
static bool Transfer(HANDLE handle, bool required, bool transport_success)
{
    SharedSMBusLock lock(handle, required);
    if(!lock.Ready()) return false;
    ++transfers;
    return transport_success;
}

extern "C" int main()
{
    Reset();
    Check(CreateSharedSMBusMutex() == &mutex_token);
    Check(created == 1 && freed == 1 && opened == 0 && last_error == 0);

    Reset(); convert_ok = false;
    Check(CreateSharedSMBusMutex() == nullptr);
    Check(created == 0 && opened == 0 && freed == 0 && last_error == 87);

    Reset(); create_ok = false; create_error = ERROR_ACCESS_DENIED;
    Check(CreateSharedSMBusMutex() == &mutex_token);
    Check(opened == 1 && freed == 1);

    Reset(); create_ok = false; create_error = ERROR_ACCESS_DENIED;
    open_ok = false; open_error = ERROR_ACCESS_DENIED;
    Check(CreateSharedSMBusMutex() == nullptr);
    Check(opened == 1 && freed == 1 && last_error == ERROR_ACCESS_DENIED);

    Reset(); create_ok = false; create_error = ERROR_INVALID_HANDLE;
    Check(CreateSharedSMBusMutex() == nullptr);
    Check(opened == 0 && freed == 1 && last_error == ERROR_INVALID_HANDLE);

    Reset(); create_error = 183; // Already-existing object: no ACL rewrite.
    Check(CreateSharedSMBusMutex() == &mutex_token);
    Check(last_error == 183 && opened == 0);

    const DWORD results[] = {WAIT_OBJECT_0, WAIT_ABANDONED, WAIT_TIMEOUT, WAIT_FAILED, 42};
    for(unsigned i = 0; i < 5; ++i)
    {
        Reset(); wait_result = results[i];
        const bool owns = i < 2;
        Check(Transfer(&mutex_token, true, true) == owns);
        Check(waited == 1 && transfers == (owns ? 1 : 0));
        Check(released == (owns ? 1 : 0));
    }
    Reset();
    Check(!Transfer(&mutex_token, true, false));
    Check(transfers == 1 && released == 1);

    Reset();
    Check(!Transfer(nullptr, true, true));
    Check(waited == 0 && transfers == 0 && released == 0);

    Reset();
    Check(Transfer(nullptr, false, true));
    Check(waited == 0 && transfers == 1 && released == 0);

    Check(checks >= 70);
    return failures;
}
