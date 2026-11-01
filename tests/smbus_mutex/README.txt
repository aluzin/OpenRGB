Offline shared SMBus mutex regression tests
=========================================

Requirements: Windows, Python 3, x64 MSVC compiler/linker. No Windows SDK,
Qt, PawnIO library or installed RGB application is needed for these tests.

From the repository root:
  python tests/smbus_mutex/run_tests.py --cl "PATH\TO\Hostx64\x64\cl.exe" --mutations
  powershell -NoProfile -File tests/smbus_mutex/test_acl.ps1

The helper test compiles the actual SharedSMBusMutex.h against fake API
declarations. The integration harness compiles the actual bus header and extracts
the constructor, destructor, i2c_smbus_xfer and piix4_port_sel bodies verbatim from
the production .cpp. The rest of the application, Windows APIs and PawnIO are
stubbed. The extractor stops with an error if a function cannot be identified.
This isolates behavior without executing OpenRGB discovery or loading drivers.

The freestanding test executables have no DLL imports (both normal and delayed
import directories are checked before execution), no real mutex, and no device
APIs. The SDDL test parses the production security descriptor in memory only;
it does not create, open or modify a kernel object. These tests do not replace
a real Windows SDK/Qt build or cross-process service/client compatibility tests.

Covered behavior:
- Explicit DACL for SYSTEM, Administrators and Authenticated Users, all access.
- Explicit medium integrity label with no-write-up.
- Descriptor conversion failure: no mutex creation or transport.
- Descriptor freed and Win32 error preserved after creation.
- Access-denied fallback uses only SYNCHRONIZE | MUTEX_MODIFY_STATE.
- Non-access failures do not attempt a different mutex or bypass locking.
- Existing-object success; no attempt to alter its ACL.
- Acquired and abandoned mutex ownership: release exactly once.
- Timeout, failed or unexpected wait result: no transfer and no release.
- Driver error after acquisition: release before returning the error.
- Constructor creation failure: subsequent transfer fails closed.
- Explicit shared-access opt-out: null initialized handle, no wait or release.
- PIIX4 port selection uses the same policy and closes its temporary handle.

Nine mutation tests deliberately break the implementation and require the
compiled tests to reject each version. A compilation failure does not count as
detecting a regression. Generated binaries and mutant sources stay under build/.
The test harness only models one bus-name entry in the static handle counter;
it is not a test of the pre-existing multi-bus lifetime management.
