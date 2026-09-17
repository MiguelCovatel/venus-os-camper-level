param([string]$Zig = "")
$ErrorActionPreference = "Stop"
$firmwareDir = Split-Path -Parent $PSScriptRoot
$previousGlobalCache = $env:ZIG_GLOBAL_CACHE_DIR
$previousLocalCache = $env:ZIG_LOCAL_CACHE_DIR
Push-Location -LiteralPath $firmwareDir
try {
    if (-not $Zig) {
        $bundled = "../../../.tools/ziglang/ziglang/zig.exe"
        if (Test-Path -LiteralPath $bundled) { $Zig = $bundled }
        else {
            $globalZig = Get-Command zig -ErrorAction SilentlyContinue
            if ($globalZig) { $Zig = $globalZig.Source }
        }
    }
    if (-not $Zig -or -not (Test-Path -LiteralPath $Zig)) {
        throw "Zig compiler not found. Run pio tests on a host with gcc/g++, or pass -Zig <path>."
    }
    $compiler = (Resolve-Path -LiteralPath $Zig).Path
    $unityDir = ".pio/libdeps/native/Unity/src"
    if (-not (Test-Path "$unityDir/unity.c")) { throw "Run 'pio pkg install -e native' first." }
    New-Item -ItemType Directory -Force -Path ".test-build" | Out-Null
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $firmwareDir ".test-build/zig-cache"
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $firmwareDir ".test-build/zig-local-cache"
    $sources = @("src/level/level_calculator.cpp", "src/level/stability_detector.cpp", "src/communication/retry_backoff.cpp")
    foreach ($suite in @("test_level_calculator", "test_stability_detector")) {
        $binary = ".test-build/$suite.exe"
        & $compiler c++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated -Isrc "-I$unityDir" -x c++ @sources "$unityDir/unity.c" "test/$suite/test_main.cpp" -o $binary
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $suite" }
        & (Join-Path $firmwareDir $binary)
        if ($LASTEXITCODE -ne 0) { throw "Tests failed: $suite" }
    }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $previousGlobalCache
    $env:ZIG_LOCAL_CACHE_DIR = $previousLocalCache
    Pop-Location
}
