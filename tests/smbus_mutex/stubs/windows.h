// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only API declarations. No Windows import library or device API is used.
#pragma once
using DWORD = unsigned long;
using BOOL = int;
using HANDLE = void*;
using PHANDLE = HANDLE*;
using HRESULT = long;
using PSECURITY_DESCRIPTOR = void*;
struct SECURITY_ATTRIBUTES { DWORD nLength; void* lpSecurityDescriptor; BOOL bInheritHandle; };
constexpr BOOL FALSE = 0;
constexpr DWORD ERROR_ACCESS_DENIED = 5;
constexpr DWORD ERROR_INVALID_HANDLE = 6;
constexpr DWORD SYNCHRONIZE = 0x100000;
constexpr DWORD MUTEX_MODIFY_STATE = 1;
constexpr DWORD WAIT_OBJECT_0 = 0;
constexpr DWORD WAIT_ABANDONED = 0x80;
constexpr DWORD WAIT_TIMEOUT = 258;
constexpr DWORD WAIT_FAILED = 0xffffffff;
BOOL ConvertStringSecurityDescriptorToSecurityDescriptorA(const char*, DWORD, void**, DWORD*);
HANDLE CreateMutexA(SECURITY_ATTRIBUTES*, BOOL, const char*);
HANDLE OpenMutexA(DWORD, BOOL, const char*);
DWORD GetLastError();
void SetLastError(DWORD);
void* LocalFree(void*);
DWORD WaitForSingleObject(HANDLE, DWORD);
BOOL ReleaseMutex(HANDLE);
