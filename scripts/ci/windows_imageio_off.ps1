$ErrorActionPreference = "Stop"
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$Prefix = Join-Path $env:RUNNER_TEMP "io-prefix"
$corePrefix = $env:CARBON_CORE_PREFIX
if (-not $corePrefix) { throw "CARBON_CORE_PREFIX required" }

$cmakePrefix = $corePrefix
if ($env:CMAKE_PREFIX_PATH) { $cmakePrefix = $env:CMAKE_PREFIX_PATH }
$prefixBins = @()
foreach ($entry in ($cmakePrefix -split ';')) {
    if ($entry) { $prefixBins += (Join-Path $entry "bin") }
}
$env:PATH = (($prefixBins + @($env:PATH)) -join ';')

cmake -S $Root -B (Join-Path $env:RUNNER_TEMP "io-build") -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_INSTALL_PREFIX=$Prefix" `
  "-DCMAKE_PREFIX_PATH=$cmakePrefix" `
  -DCMAKE_CXX_STANDARD=17 `
  -DCARBON_IMAGEIO_WITH_NANOVDB=OFF `
  -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "imageio configure failed ($LASTEXITCODE)" }
cmake --build (Join-Path $env:RUNNER_TEMP "io-build") --target install
if ($LASTEXITCODE -ne 0) { throw "imageio install failed ($LASTEXITCODE)" }
ctest --test-dir (Join-Path $env:RUNNER_TEMP "io-build") --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "imageio tests failed ($LASTEXITCODE)" }

$config = Join-Path $Prefix "share\carbon-imageio\carbon-imageioConfig.cmake"
$text = Get-Content $config -Raw
if ($text -match '(?s)if\(ON\).*find_dependency\(OpenVDB') { throw "OFF config must not enable OpenVDB" }
if ($text -notmatch '(?s)if\(OFF\).*find_dependency\(OpenVDB') { throw "OFF config must gate OpenVDB behind if(OFF)" }
if ($text -match "zlib-ng") { throw "OFF config must not require zlib-ng" }

$consumer = Join-Path $Root "examples\jpeg_png_consumer"
cmake -S $consumer -B (Join-Path $env:RUNNER_TEMP "consumer-build") -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_PREFIX_PATH=$cmakePrefix;$Prefix"
if ($LASTEXITCODE -ne 0) { throw "consumer configure failed ($LASTEXITCODE)" }
cmake --build (Join-Path $env:RUNNER_TEMP "consumer-build")
if ($LASTEXITCODE -ne 0) { throw "consumer link failed ($LASTEXITCODE)" }
$consumerExe = Join-Path $env:RUNNER_TEMP "consumer-build\imageio_jpeg_png_consumer.exe"
$consumerOutput = & $consumerExe
if ($LASTEXITCODE -ne 0) { throw "consumer execution failed ($LASTEXITCODE): $consumerOutput" }
if ($consumerOutput -notmatch "IMAGEIO_JPEG_PNG_CONSUMER png=4x3") { throw "consumer did not report the PNG result: $consumerOutput" }
$dump = dumpbin /DEPENDENTS $consumerExe | Out-String
if ($dump -match "openvdb|nanovdb") { throw "consumer linked OpenVDB or NanoVDB" }
Write-Host $consumerOutput

# Legacy monolith config generation (configure only)
cmake -S $Root -B (Join-Path $env:RUNNER_TEMP "io-monolith-cfg") -G Ninja `
  -DCMAKE_CXX_STANDARD=17 `
  "-DCMAKE_PREFIX_PATH=$cmakePrefix" `
  -DCARBON_IMAGEIO_WITH_NANOVDB=OFF `
  -DINSTALL_TO_MONOLITH=ON `
  -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { throw "monolith configure failed ($LASTEXITCODE)" }
$gen = Join-Path $env:RUNNER_TEMP "io-monolith-cfg\carbon-imageioConfig.cmake"
if (-not (Test-Path $gen)) { throw "monolith config not generated" }
$mono = Get-Content $gen -Raw
if ($mono -match "NanoVDB") { throw "monolith OFF config must not require NanoVDB" }
Write-Host "WINDOWS_IMAGEIO_OFF_CI_OK"