param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDirectory
)

$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).ProviderPath
$files = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse)
$allowedExtensions = @('.dll', '.qm', '.pak', '.bin', '.dat')
$allowedExecutables = @('EZ4Connect.exe', 'zju-connect.exe', 'QtWebEngineProcess.exe')
$unexpected = @()

foreach ($file in $files) {
    $relative = [IO.Path]::GetRelativePath($packageRoot, $file.FullName).Replace('\', '/')
    $allowed = $file.Extension -in $allowedExtensions
    if ($relative -in $allowedExecutables -or $relative -eq 'qt.conf') {
        $allowed = $true
    }
    if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        $relative -match '(^|/)(profiles|logs|cache)(/|$)' -or -not $allowed) {
        $unexpected += $relative
    }
}

if ($unexpected.Count -gt 0) {
    throw ('Package contains files outside the runtime allowlist: ' + ($unexpected -join ', '))
}

foreach ($required in @('EZ4Connect.exe', 'zju-connect.exe', 'Qt6Core.dll',
                         'Qt6Widgets.dll', 'Qt6WebEngineCore.dll', 'platforms/qwindows.dll', 'qt.conf')) {
    if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $required) -PathType Leaf)) {
        throw ('Missing runtime file: ' + $required)
    }
}

$sourceConfigHash = (Get-FileHash -LiteralPath 'resource/qt.conf' -Algorithm SHA256).Hash
$packageConfigHash = (Get-FileHash -LiteralPath (Join-Path $packageRoot 'qt.conf') -Algorithm SHA256).Hash
if ($sourceConfigHash -ne $packageConfigHash) {
    throw 'Packaged qt.conf differs from the tracked Qt platform configuration.'
}

$defaults = Get-Content -LiteralPath 'application/defaultsettings.cpp' -Raw
foreach ($field in @('Username', 'Password', 'TOTPSecret')) {
    $pattern = 'settings\.setValue\("Credential/' + $field + '",\s*""\);'
    if ($defaults -notmatch $pattern) {
        throw ('Default credential must be empty: ' + $field)
    }
}

[pscustomobject]@{
    Passed = $true
    RuntimeFiles = $files.Count
    RuntimeAllowlistOnly = $true
    QtConfigurationMatchesSource = $true
    DefaultCredentialsEmpty = $true
} | ConvertTo-Json
