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
