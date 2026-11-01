// SPDX-License-Identifier: GPL-2.0-or-later
// Compile production method bodies with a mock bus object and no real SDK.
#define main unit_main
#include "test.cpp"
#undef main
#define NULL 0
#define LOG_ERROR(...) ((void)0)
#define private public
#include "../../i2c_smbus/Windows/i2c_smbus_pawnio.h"
#undef private
using SIZE_T = unsigned long long;
using ULONG64 = unsigned long long;
constexpr int EIO = 5;
std::unordered_map<std::string, int> i2c_smbus_pawnio::using_handle;

static int closed, device_token;
static HRESULT transport_result;
static bool expect_lock;
static bool setting_present, setting_enabled;
struct json
{
    bool contains(const char* key) { return setting_present && Equal(key, "shared_smbus_access"); }
    json& operator[](const char*) { return *this; }
    template<class T> T get() { return static_cast<T>(setting_enabled); }
    operator unsigned int() { return 2; }
};
struct SettingsManager { json GetSettings(const char*) { return {}; } };
static SettingsManager settings;
struct ResourceManager
{
    static ResourceManager* get();
    SettingsManager* GetSettingsManager() { return &settings; }
};
static ResourceManager resources;
ResourceManager* ResourceManager::get() { return &resources; }
static int snprintf(char*, SIZE_T, const char*, ...) { return 0; }
static int set_sleep_mode(HANDLE, s32) { return 0; }
static int pawnio_close(HANDLE handle) { Check(handle == &device_token); return 0; }
static void* memcpy(void* dest, const void* src, SIZE_T n)
{
    auto d = static_cast<u8*>(dest);
    auto s = static_cast<const u8*>(src);
    for(SIZE_T i = 0; i < n; ++i) d[i] = s[i];
    return dest;
}
static BOOL CloseHandle(HANDLE handle)
{
    Check(handle == &mutex_token);
    // A successful wait must be released before its handle is closed.
    Check(released == ((waited && (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED)) ? 1 : 0));
    ++closed;
    return 1;
}
static HRESULT pawnio_execute(HANDLE handle, const char* command, ULONG64* input,
                              SIZE_T input_size, ULONG64* output, SIZE_T output_size,
                              SIZE_T* returned)
{
    Check(handle == &device_token);
    // Constructor metadata query is stubbed; never calls an actual driver.
    if(Equal(command, "ioctl_identity")) return -1;
    Check(!expect_lock || (waited == 1 && released == 0));
    Check(Equal(command, "ioctl_smbus_xfer") || Equal(command, "ioctl_piix4_port_sel"));
    if(Equal(command, "ioctl_smbus_xfer"))
    {
        Check(input_size == 9 && input[0] == 0x55 && input[2] == 0x81);
        Check(output_size == 5);
    }
    else
    {
        Check(input_size == 1 && input[0] == 1 && output_size == 1);
    }
    for(SIZE_T i = 0; i < output_size; ++i) output[i] = 0;
    *returned = output_size;
    ++transfers;
    return transport_result;
}
