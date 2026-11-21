$ErrorActionPreference = "Stop"
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$Prefix = Join-Path $env:RUNNER_TEMP "io-prefix"
$corePrefix = $env:CARBON_CORE_PREFIX
if (-not $corePrefix) { throw "CARBON_CORE_PREFIX required" }

$cmakePrefix = $corePrefix
if ($env:CMAKE_PREFIX_PATH) { $cmakePrefix = $env:CMAKE_PREFIX_PATH }

cmake -S $Root -B (Join-Path $env:RUNNER_TEMP "io-build") -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_INSTALL_PREFIX=$Prefix" `
  "-DCMAKE_PREFIX_PATH=$cmakePrefix" `
  -DCMAKE_CXX_STANDARD=17 `
  -DCARBON_IMAGEIO_WITH_NANOVDB=OFF `
  -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { throw "imageio configure failed ($LASTEXITCODE)" }
cmake --build (Join-Path $env:RUNNER_TEMP "io-build") --target install
if ($LASTEXITCODE -ne 0) { throw "imageio install failed ($LASTEXITCODE)" }

$config = Join-Path $Prefix "share\carbon-imageio\carbon-imageioConfig.cmake"
$text = Get-Content $config -Raw
if ($text -match '(?s)if\(ON\).*find_dependency\(OpenVDB') { throw "OFF config must not enable OpenVDB" }
if ($text -notmatch '(?s)if\(OFF\).*find_dependency\(OpenVDB') { throw "OFF config must gate OpenVDB behind if(OFF)" }
if ($text -match "zlib-ng") { throw "OFF config must not require zlib-ng" }

$consumer = Join-Path $env:RUNNER_TEMP "consumer"
New-Item -ItemType Directory -Force -Path $consumer | Out-Null
@'
cmake_minimum_required(VERSION 3.19)
project(io_consumer LANGUAGES CXX)
find_package(carbon-imageio CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE CcpImageIO)
'@ | Set-Content (Join-Path $consumer "CMakeLists.txt")
Set-Content (Join-Path $consumer "main.cpp") "int main(){return 0;}"
cmake -S $consumer -B (Join-Path $consumer "build") -G Ninja "-DCMAKE_PREFIX_PATH=$cmakePrefix;$Prefix"
if ($LASTEXITCODE -ne 0) { throw "consumer configure failed ($LASTEXITCODE)" }
cmake --build (Join-Path $consumer "build")
if ($LASTEXITCODE -ne 0) { throw "consumer link failed ($LASTEXITCODE)" }

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