param([Parameter(Mandatory=$true)][string]$SDLRoot, [switch]$AllowNoGPU)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '../..')
$SDLRoot = (Resolve-Path $SDLRoot).Path
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('exult-scalers-' + [Guid]::NewGuid())
New-Item -ItemType Directory $testRoot | Out-Null
$originalPath = $env:PATH
$originalVideo = $env:SDL_VIDEODRIVER
function Compile-Test([string[]]$Arguments) {
    & cl.exe /nologo /std:c++17 /EHsc /O2 /I. /Iheaders "/I$SDLRoot/include" @Arguments
    if ($LASTEXITCODE -ne 0) { throw "MSVC compilation failed ($LASTEXITCODE)" }
}
function Run-Test([string]$File) {
    & $File
    $status = $LASTEXITCODE
    if ($status -eq 77 -and $AllowNoGPU) {
        Write-Host "SKIP: $File : no native GPU device on this runner"
    } elseif ($status -ne 0) { throw "Scaler test failed ($status): $File" }
}
try {
    $env:PATH = "$SDLRoot/bin;$env:PATH"
    $lib = Join-Path $SDLRoot 'lib/SDL3.lib'
    Compile-Test @('imagewin/tests/test_scalefx.cc', "/Fe:$testRoot/cpu.exe", "/Fo:$testRoot/cpu.obj")
    Run-Test "$testRoot/cpu.exe"
    Compile-Test @('imagewin/tests/test_gpu_fallback.cc', "/Fe:$testRoot/fallback.exe", "/Fo:$testRoot/fallback.obj", $lib)
    $env:SDL_VIDEODRIVER = 'dummy'
    Run-Test "$testRoot/fallback.exe"
    $env:SDL_VIDEODRIVER = $originalVideo
    $references = @()
    foreach ($factor in 2,3,4) {
        $object = "$testRoot/hq$factor.obj"
        Compile-Test @('/c', "/DTEST_HQ_FACTOR=$factor", 'imagewin/tests/hq_reference.cc', "/Fo:$object")
        $references += $object
    }
    Compile-Test @('/c', 'imagewin/tests/scaler_reference.cc', "/Fo:$testRoot/reference.obj")
    $references += "$testRoot/reference.obj"
    foreach ($test in 'test_scalefx_gpu','test_scalefx_gpu_ui','test_gpu_scalers') {
        $arguments = @("imagewin/tests/$test.cc", "/Fe:$testRoot/$test.exe", "/Fo:$testRoot/$test.obj")
        if ($test -ne 'test_scalefx_gpu') { $arguments += $references }
        Compile-Test ($arguments + @($lib))
        Run-Test "$testRoot/$test.exe"
    }
} finally {
    $env:PATH = $originalPath
    $env:SDL_VIDEODRIVER = $originalVideo
    Remove-Item -Recurse -Force $testRoot
}
