// SPDX-License-Identifier: LGPL-2.1-or-later
// Candidate for maintainer review, not enabled or loaded on hardware.
// Explicit opt-in recovery for the observed ASUS 1043:88EF / Intel 8086:7F23.
// The caller MUST hold Global\Access_SMBUS.HTP.Method across this call.
// A quiet HSTS byte does NOT prove BIOS/ACPI has abandoned its semaphore.
// Thus this cannot be enabled automatically based on a timeout alone.
// No command/address/data/PCI control registers are written here.

#define I801_RECOVERY_ACK 0x4938303152454356
#define I801_RECOVERY_QUIET_MS 250

new bool:i801_recovery_attempted = false;

NTSTATUS:i801_startup_recovery(ack, &result, &last_hststs)
{
    result = 0;
    last_hststs = -1;
    if (ack != I801_RECOVERY_ACK)
        return STATUS_ACCESS_DENIED;
    if (i801_recovery_attempted)
        return STATUS_ACCESS_DENIED;

    new ids;
    new NTSTATUS:status = pci_config_read_dword(pci_addr[0], pci_addr[1], pci_addr[2], 0x00, ids);
    if (!NT_SUCCESS(status))
        return status;
    if (ids != 0x7F238086)
        return STATUS_NOT_SUPPORTED;
    status = pci_config_read_dword(pci_addr[0], pci_addr[1], pci_addr[2], 0x2C, ids);
    if (!NT_SUCCESS(status))
        return status;
    if (ids != 0x88EF1043)
        return STATUS_NOT_SUPPORTED;

    // Do not change memory decoding or power state as part of recovery.
    status = pci_config_read_word(pci_addr[0], pci_addr[1], pci_addr[2], PCICMD, ids);
    if (!NT_SUCCESS(status))
        return status;
    if (!(ids & PCICMD_MMBIT))
        return STATUS_DEVICE_NOT_READY;

    i801_recovery_attempted = true;
    new deadline = get_tick_count() + I801_RECOVERY_QUIET_MS;
    new bool:waiting = true;
    do {
        status = virtual_read_byte(SMBHSTSTS, last_hststs);
        if (!NT_SUCCESS(status))
            return status;

        if (!(last_hststs & SMBHSTSTS_INUSE_STS)) {
            // This read acquired the semaphore. Release only our own bit.
            status = virtual_write_byte(SMBHSTSTS, SMBHSTSTS_INUSE_STS);
            if (!NT_SUCCESS(status))
                return status;
            result = 1; // No forced recovery was needed.
            return STATUS_SUCCESS;
        }

        // Only INUSE may be set. Busy, pending completion, error, ALERT,
        // BYTE_DONE, inaccessible MMIO (0xFF), or changing status -> refuse.
        if (last_hststs != SMBHSTSTS_INUSE_STS)
            return STATUS_DEVICE_BUSY;
        waiting = get_tick_count() < deadline;
        if (waiting)
            microsleep(1000);
    } while (waiting);

    // Authorized forced semaphore clear, never a controller KILL/reset.
    status = virtual_write_byte(SMBHSTSTS, SMBHSTSTS_INUSE_STS);
    if (!NT_SUCCESS(status))
        return status;
    result = 2; // Clear issued; caller must still acquire normally afterwards.
    return STATUS_SUCCESS;
}

// This entry point is never called by module initialization or smbus_xfer.
// Caller must explicitly acknowledge the hardware semaphore ownership risk.
DEFINE_IOCTL_SIZED(ioctl_startup_recovery, 1, 2) {
    return i801_startup_recovery(in[0], out[0], out[1]);
}
