# SPDX-License-Identifier: GPL-2.0-or-later
# Parse the production SDDL in memory. No mutex is created/opened or changed.
$ErrorActionPreference = 'Stop'
$header = Get-Content -Raw (Join-Path $PSScriptRoot '../../i2c_smbus/Windows/SharedSMBusMutex.h')
$match = [regex]::Match($header, '"(D:[^"]+)"')
if (!$match.Success) { throw 'Production SDDL not found' }
$sd = [System.Security.AccessControl.RawSecurityDescriptor]::new($match.Groups[1].Value)
$expected = @('S-1-5-18', 'S-1-5-32-544', 'S-1-5-11')
if ($sd.DiscretionaryAcl.Count -ne $expected.Count) { throw 'Unexpected DACL entry count' }
for ($i = 0; $i -lt $expected.Count; $i++) {
    $ace = $sd.DiscretionaryAcl[$i]
    if ($ace.SecurityIdentifier.Value -ne $expected[$i] -or $ace.AccessMask -ne 0x10000000 -or $ace.AceType.ToString() -ne 'AccessAllowed' -or $ace.AceFlags -ne 0) {
        throw "Incorrect DACL entry $i"
    }
}
if ($sd.SystemAcl.Count -ne 1) { throw 'Missing integrity label' }
$label = $sd.SystemAcl[0]
# RawSecurityDescriptor exposes SYSTEM_MANDATORY_LABEL_ACE as a CustomAce
# on some .NET versions. Check its binary layout instead of casting it.
$bytes = [byte[]]::new($label.BinaryLength)
$label.GetBinaryForm($bytes, 0)
if ($bytes[0] -ne 17 -or [BitConverter]::ToInt32($bytes, 4) -ne 1) { throw 'Incorrect mandatory policy' }
$sid = [System.Security.Principal.SecurityIdentifier]::new($bytes, 8)
if ($sid.Value -ne 'S-1-16-8192') { throw 'Expected medium integrity label' }
Write-Output 'PASS: production SDDL grants full mutex access to SYSTEM, Administrators, Authenticated Users; medium integrity with no-write-up'
