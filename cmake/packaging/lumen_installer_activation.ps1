[CmdletBinding()]
param(
    [switch] $SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$activationSecret = '358A10FD7DE42CBB1D9C8C16C44307FF8E87B25D16671F3CE23CBF9ABC79C138'

function Get-Sha256Hex {
    param([Parameter(Mandatory)] [string] $Value)

    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value))
    }
    finally {
        $sha256.Dispose()
    }

    return -join ($hash | ForEach-Object { $_.ToString('X2') })
}

function Format-LumenCode {
    param([Parameter(Mandatory)] [string] $Hex)

    $raw = $Hex.Substring(0, 24)
    return '{0}-{1}-{2}-{3}' -f $raw.Substring(0, 6), $raw.Substring(6, 6), $raw.Substring(12, 6), $raw.Substring(18, 6)
}

function New-LumenMachineCode {
    param([Parameter(Mandatory)] [object[]] $HardwareValues)

    $ignoredValues = '^(0+|F+|TOBEFILLEDBYOEM|DEFAULTSTRING|SYSTEMSERIALNUMBER|NONE|UNKNOWN|NOTSPECIFIED|NA)$'
    $identifiers = @(
        $HardwareValues |
            ForEach-Object { if ($null -ne $_) { ([string] $_).Trim().ToUpperInvariant() -replace '[^0-9A-Z]', '' } } |
            Where-Object { $_ -and $_.Length -ge 6 -and $_ -notmatch $ignoredValues } |
            Sort-Object -Unique
    )

    if ($identifiers.Count -eq 0) {
        throw 'No usable physical hardware identifier was reported by Windows.'
    }

    return Format-LumenCode (Get-Sha256Hex ('LUMEN-HARDWARE-V1|' + ($identifiers -join '|')))
}

function New-LumenActivationCode {
    param([Parameter(Mandatory)] [string] $MachineCode)

    $normalized = $MachineCode.ToUpperInvariant() -replace '[^0-9A-F]', ''
    if ($normalized.Length -ne 24) {
        throw 'Invalid Lumen machine code.'
    }

    return Format-LumenCode (Get-Sha256Hex "LUMEN-ACTIVATION-V1|$activationSecret|$normalized")
}

if ($SelfTest) {
    $first = New-LumenMachineCode @('ABCDEF12-3456-7890-ABCD-EF1234567890', '00:11:22:33:44:55')
    $same = New-LumenMachineCode @('00-11-22-33-44-55', 'abcdef12-3456-7890-abcd-ef1234567890')
    $different = New-LumenMachineCode @('ABCDEF12-3456-7890-ABCD-EF1234567890', '00:11:22:33:44:56')
    if ($first -ne $same -or $first -eq $different) {
        throw 'Machine-code stability/uniqueness self-test failed.'
    }

    if ((New-LumenActivationCode '012345-6789AB-CDEF01-234567') -ne '0C6E8A-80BF73-5871DB-30647A') {
        throw 'Activation-code self-test failed.'
    }

    [Console]::Write('OK')
    return
}

$hardwareValues = @()

try {
    $systemProduct = Get-CimInstance -ClassName Win32_ComputerSystemProduct
    $hardwareValues += $systemProduct.UUID
    $hardwareValues += $systemProduct.IdentifyingNumber
}
catch {}

try {
    $hardwareValues += (Get-CimInstance -ClassName Win32_BaseBoard).SerialNumber
}
catch {}

try {
    $hardwareValues += (Get-CimInstance -ClassName Win32_BIOS).SerialNumber
}
catch {}

try {
    $hardwareValues += Get-CimInstance -ClassName Win32_Processor | ForEach-Object { $_.ProcessorId }
}
catch {}

try {
    $hardwareValues += Get-CimInstance -ClassName Win32_NetworkAdapter |
        Where-Object { $_.PhysicalAdapter -eq $true -and $_.MACAddress } |
        ForEach-Object { $_.MACAddress }
}
catch {}

$machineCode = New-LumenMachineCode $hardwareValues
$activationCode = New-LumenActivationCode $machineCode
[Console]::Write("$machineCode|$activationCode")
