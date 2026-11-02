ENE 6802 offline tests
=====================
python tests/ene6802/run_tests.py --cl cl.exe --mutations

MSVC x64 required. No SDK required for these isolated tests. Actual controller
and Windows adapter sources compile against fake transport/Windows functions.
Test EXEs are checked for absence of normal and delayed OS imports before run.
They cannot load a driver, enumerate a device or perform SMBus I/O.

Coverage:
- Full identification sequence, exact 6802/D5/config gate; no lighting writes.
- Full Static trace at both sample addresses, mixed colors and subsequent frame.
- Wrong chips, every changed firmware/config byte, disallowed addresses.
- Inject failure at every identification and update transaction; no subsequent
  calls after failure, no apply after earlier failure, require re-identification.
- Lock acquisition failure, sequence release on every early return.
- Windows adapter: raw transfer types, standard SMBus block length/data, word
  order, recursive shared-lock behavior modeled on the synchronous PawnIO path.
- Timeout/failed/abandoned waits refuse I/O; abandoned ownership is released.
- Eight compile-success mutations must fail tests, not merely fail to compile.

The test stack is committed to 64 KiB to run a small freestanding executable
without CRT stack-probe imports. RTTI is disabled only for these test binaries.
Full OpenRGB compilation uses normal SDK/runtime/RTTI settings in CI.

These tests do not prove physical hardware safety, color channel semantics of
the vendor COM caller, or behavior of other applications ignoring the mutex.
See Documentation/ENE6802-DDR5.txt for remaining hardware validation limits.

Detector and RGB-wrapper integration (Linux CI, hardware/API test doubles):
python3 tests/ene6802/integration/run.py
Actual four production CPP files, real JSON/OpenRGB color types, fake Windows,
fake bus and base UI services. ASan/UBSan.83 cases/1319 assertions plus7 detector
mutations, including complete-configuration validation before any transport is
created, default-disabled registration, cleanup, no color writes in constructor
or destructor, invalid UI selections, independent modules and failure latch.
The actual IsAnyDimmDetectorEnabled body is compiled verbatim in isolation;
the complete DetectionManager, Qt UI and base update scheduler are not executed.

Prepared-configuration verification (file/data operations only):
python tests/ene6802/test_prepare_trial.py
Five test groups: valid scopes, enabling each other known detector, missing
DIMM gates, altered fingerprint/address scope, auto-profile/unlocked mistakes.
prepare_trial.py generates immutable first-use configs from a saved Windows
inventory plus source registrations. See Documentation/ENE6802-HARDWARE-TRIAL.txt.
