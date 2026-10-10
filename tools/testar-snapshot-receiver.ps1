param(
  [ValidateSet('compile','upload','logs')][string]$Action = 'compile',
  [string]$Port = 'COM4',
  [string]$PythonPath = "$env:LOCALAPPDATA\esphome-2026.9.1\venv\Scripts\python.exe"
)
$ErrorActionPreference = 'Stop'
$repoReceiver = Split-Path $PSScriptRoot -Parent
$configReceiver = Join-Path $repoReceiver 'examples\snapshot_receiver_runtime.yaml'
$versionReceiver = & $PythonPath -c 'import importlib.metadata; print(importlib.metadata.version("esphome"))'
if ($LASTEXITCODE -ne 0 -or "$versionReceiver".Trim() -ne '2026.9.1') { throw 'Requer ESPHome 2026.9.1.' }
$rootReceiver = Join-Path $env:LOCALAPPDATA 'esphome-snapshot-receiver-2026.9.1'
$namesReceiver = @('ESPHOME_BUILD_PATH','ESPHOME_DATA_DIR','ESPHOME_ESP_IDF_PREFIX')
$savedReceiver = @{}
foreach ($nameReceiver in $namesReceiver) { $savedReceiver[$nameReceiver] = [Environment]::GetEnvironmentVariable($nameReceiver,'Process') }
try {
  $env:ESPHOME_BUILD_PATH = Join-Path $rootReceiver 'build'
  $env:ESPHOME_DATA_DIR = Join-Path $rootReceiver 'data'
  $env:ESPHOME_ESP_IDF_PREFIX = Join-Path $env:LOCALAPPDATA 'esphome\Cache\idf'
  if ($Action -eq 'compile') {
    & $PythonPath -m esphome compile $configReceiver
  } elseif ($Action -eq 'upload') {
    $imageReceiver = Join-Path $rootReceiver 'build\snapshot-receiver-test\build\firmware.factory.bin'
    if (-not (Test-Path -LiteralPath $imageReceiver)) { throw 'Compile o autoteste primeiro.' }
    & $PythonPath -m esptool --port $Port write-flash 0x0 $imageReceiver
  } else {
    & $PythonPath -m esphome logs $configReceiver --device $Port
  }
  if ($LASTEXITCODE -ne 0) { throw "Etapa $Action falhou; preserve o log." }
} finally {
  foreach ($nameReceiver in $namesReceiver) { [Environment]::SetEnvironmentVariable($nameReceiver,$savedReceiver[$nameReceiver],'Process') }
}
