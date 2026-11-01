/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <windows.h>
#include <sddl.h>

// This name is shared with other hardware-monitoring and lighting applications.
inline HANDLE CreateSharedSMBusMutex(const char* name = "Global\\Access_SMBUS.HTP.Method")
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    // Vendor clients use CreateMutex (MUTEX_ALL_ACCESS), not just OpenMutex
    // with SYNCHRONIZE. Give authenticated users access to this coordination
    // object regardless of whether a service or a desktop app creates it first.
    // Explicit medium integrity also permits non-elevated desktop clients when
    // the creator is SYSTEM. This does not grant access to any device or driver.
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorA(
           "D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AU)S:(ML;;NW;;;ME)",
           SDDL_REVISION_1, &descriptor, nullptr))
    {
        return nullptr;
    }

    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    HANDLE handle = CreateMutexA(&attributes, FALSE, name);
    DWORD error = GetLastError();
    LocalFree(descriptor);

    // Existing objects retain their original ACL. Do not rewrite someone
    // else's ACL; try only the rights needed to participate in locking.
    if(handle == nullptr && error == ERROR_ACCESS_DENIED)
    {
        handle = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name);
        error = GetLastError();
    }
    SetLastError(error);
    return handle;
}

class SharedSMBusLock
{
public:
    SharedSMBusLock(HANDLE handle, bool required) : handle(handle), acquired(false), ready(!required)
    {
        if(required && handle != nullptr)
        {
            // Do not hang detection forever if another application holds the bus.
            const DWORD result = WaitForSingleObject(handle, 3000);
            acquired = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
            ready = acquired;
        }
    }

    ~SharedSMBusLock()
    {
        if(acquired)
        {
            ReleaseMutex(handle);
        }
    }

    bool Ready() const { return ready; }
    SharedSMBusLock(const SharedSMBusLock&) = delete;
    SharedSMBusLock& operator=(const SharedSMBusLock&) = delete;

private:
    HANDLE handle;
    bool acquired;
    bool ready;
};
