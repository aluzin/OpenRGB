// SPDX-License-Identifier: GPL-2.0-or-later
extern "C" int main()
{
    unit_main();
    const DWORD results[] = {WAIT_OBJECT_0, WAIT_ABANDONED, WAIT_TIMEOUT, WAIT_FAILED, 42};
    for(unsigned i = 0; i < 5; ++i)
    {
        Reset(); closed = 0; transport_result = 0; expect_lock = true;
        wait_result = results[i];
        i2c_smbus_pawnio bus(&device_token, "test", 0);
        i2c_smbus_data data = {};
        Check(bus.i2c_smbus_xfer(0x55, 1, 0x81, 1, &data) == (i < 2 ? 0 : -EIO));
        Check(transfers == (i < 2 ? 1 : 0) && released == (i < 2 ? 1 : 0));
        Check(waited == 1 && closed == 0);

        Reset(); closed = 0; wait_result = results[i];
        Check(piix4_port_sel(&device_token, 1) == (i < 2 ? 0 : -EIO));
        Check(transfers == (i < 2 ? 1 : 0) && released == (i < 2 ? 1 : 0));
        Check(closed == 1 && freed == 1);
        // Reset counters to the separate, still-owned bus handle before teardown.
        Reset();
    }
    Reset(); closed = 0; transport_result = (HRESULT)0x8007001F;
    {
        i2c_smbus_pawnio failed(&device_token, "test", 0);
        Check(failed.i2c_smbus_xfer(0x55, 1, 0x81, 1, nullptr) == -1);
        Check(transfer_errors == 1 && logged_hresult == 0x8007001FUL);
        Check(transfers == 1 && released == 1);
    }
    Reset(); closed = 0; transport_result = -1;
    i2c_smbus_pawnio bus(&device_token, "test", 0);
    Check(bus.i2c_smbus_xfer(0x55, 1, 0x81, 1, nullptr) == -1);
    Check(transfers == 1 && released == 1);

    Reset(); closed = 0;
    Check(piix4_port_sel(&device_token, 1) == -EIO);
    Check(transfers == 1 && released == 1 && closed == 1);

    Reset(); closed = 0; transport_result = 0;
    bus.global_smbus_access_handle = nullptr;
    Check(bus.i2c_smbus_xfer(0x55, 1, 0x81, 1, nullptr) == -EIO);
    Check(transfers == 0 && waited == 0 && released == 0);

    Reset(); expect_lock = false;
    bus.shared_smbus_access = false;
    Check(bus.i2c_smbus_xfer(0x55, 1, 0x81, 1, nullptr) == 0);
    Check(transfers == 1 && waited == 0 && released == 0);

    Reset(); closed = 0; create_ok = false; create_error = ERROR_INVALID_HANDLE;
    Check(piix4_port_sel(&device_token, 1) == -EIO);
    Check(transfers == 0 && waited == 0 && released == 0 && closed == 0);

    Reset(); closed = 0; create_ok = false; create_error = ERROR_ACCESS_DENIED;
    open_ok = false; open_error = ERROR_ACCESS_DENIED;
    Check(piix4_port_sel(&device_token, 1) == -EIO);
    Check(transfers == 0 && waited == 0 && released == 0 && closed == 0);

    Reset(); closed = 0; expect_lock = true;
    create_ok = false; create_error = ERROR_ACCESS_DENIED;
    Check(piix4_port_sel(&device_token, 1) == 0);
    Check(transfers == 1 && released == 1 && closed == 1 && opened == 1);

    Reset(); setting_present = true; setting_enabled = false;
    {
        i2c_smbus_pawnio disabled(&device_token, "disabled", 0);
        Check(!disabled.shared_smbus_access && disabled.global_smbus_access_handle == nullptr);
        Check(created == 0 && opened == 0);
        expect_lock = false;
        Check(disabled.i2c_smbus_xfer(0x55, 1, 0x81, 1, nullptr) == 0);
        Check(transfers == 1 && waited == 0 && released == 0);
    }
    Reset(); closed = 0; setting_enabled = true;
    create_ok = false; create_error = ERROR_ACCESS_DENIED; open_ok = false;
    {
        i2c_smbus_pawnio denied(&device_token, "denied", 0);
        Check(denied.shared_smbus_access && denied.global_smbus_access_handle == nullptr);
        Check(denied.i2c_smbus_xfer(0x55, 1, 0x81, 1, nullptr) == -EIO);
        Check(transfers == 0 && waited == 0 && released == 0);
    }
    Check(closed == 0);
    setting_present = false;
    Check(checks >= 200);
    return failures;
}
