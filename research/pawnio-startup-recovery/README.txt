REVIEW CANDIDATE ONLY. NOT AN INSTALLED FIX.

This experiment adds one explicit, opt-in ioctl to PawnIO.Modules 0.2.11
(52a7e536dff3e53c96917a28caac5e0fa6510696). It is NOT linked into OpenRGB,
called at module initialization, or called automatically after a timeout.
The ordinary i801 transfer/acquire/release behavior is unchanged.

Why this exists
--------------
Intel HSTS bit 6 is a read-to-acquire, write-one-to-clear software semaphore.
It is distinct from the physical HOST_BUSY bit. A persistent bit 6 prevents
the current i801 module from reaching RGB identification transfers.
Merely observing the status has a side effect: a read returning INUSE=0
acquires it. That case must release our own semaphore before returning.

This candidate is gated to 8086:7F23 / 1043:88EF, requires MMIO decoding
already enabled, explicit caller acknowledgement, and the caller holding
Global\Access_SMBUS.HTP.Method. It permits one attempt per module instance.
It refuses forced clearing unless HSTS remains exactly 0x40 for 250 ms;
it never writes KILL, 0xFF, address/data registers or PCI control registers.

Unresolved safety limitation
---------------------------
Quiet status DOES NOT prove that BIOS/ACPI or another driver abandoned
its semaphore. A non-cooperating owner can race the final status check.
The acknowledgement value is an API contract, not an authorization boundary.
The module cannot verify that the caller holds the Windows global mutex.
Reloading a module resets its attempt limit. This is not a once-per-boot guard.
Consequently this candidate must not be enabled as unattended boot recovery
without a separately reviewed ownership/coexistence policy.

Validation
----------
run_tests.py translates the actual helper body into C++ with fake registers.
It exercises 21 cases and eight intentionally broken guard variants, checks
that the executable has no DLL imports, and runs only that offline model.
It does not execute Pawn bytecode or prove real firmware coexistence.

build_module.py builds the complete upstream i801 source plus this include
using the upstream Pawn compiler and 64-bit cell flags. It generates a diff,
compiler log and result.json. It never loads the generated AMX file.
The CI workflow performs both checks on GitHub-hosted virtual machines.

Deployment is blocked pending a reviewed, signed module. A local unsigned
AMX is not a replacement for the installed signed SmbusI801.bin. Do not
change Secure Boot, test signing or PawnIO enforcement to load it.
No RGB commands, SPD reads, bus scans, services or autostart settings are
part of this experiment. No private host logs are published with it.

References
----------
https://github.com/namazso/PawnIO.Modules/blob/0.2.11/SmbusI801.p
https://edc.intel.com/content/www/us/en/design/publications/core-ultra-p200s-series-processors-soc-i-o-registers/host-status-register-address-hsts-offset-0/
