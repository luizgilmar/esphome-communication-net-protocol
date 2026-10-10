param(
  [ValidateSet('publisher','tx')][string]$Role = 'publisher',
  [string]$PythonPath = "$env:LOCALAPPDATA\esphome-2026.9.1\venv\Scripts\python.exe"
)
$ErrorActionPreference = 'Stop'
$repoSnapshot = Split-Path $PSScriptRoot -Parent
$versionSnapshot = & $PythonPath -c 'import importlib.metadata; print(importlib.metadata.version("esphome"))'
if ($LASTEXITCODE -ne 0 -or "$versionSnapshot".Trim() -ne '2026.9.1') { throw 'Requer ESPHome 2026.9.1.' }
$configSnapshot = if ($Role -eq 'publisher') {
  Join-Path $repoSnapshot 'examples\snapshot_capacity_compile.yaml'
} else {
  Join-Path (Split-Path $repoSnapshot -Parent) 'esphome-sonoff-txultimate\examples\quartogian_tx_snapshot_compile.yaml'
}
$rootSnapshot = Join-Path $env:LOCALAPPDATA $(if ($Role -eq 'publisher') { 'esphome-snapshot-capacity-2026.9.1' } else { 'esphome-tx-snapshot-capacity-2026.9.1' })
$namesSnapshot = @('ESPHOME_BUILD_PATH','ESPHOME_DATA_DIR','ESPHOME_ESP_IDF_PREFIX')
$savedSnapshot = @{}
foreach ($nameSnapshot in $namesSnapshot) { $savedSnapshot[$nameSnapshot] = [Environment]::GetEnvironmentVariable($nameSnapshot,'Process') }
try {
  $env:ESPHOME_BUILD_PATH = Join-Path $rootSnapshot 'build'
  $env:ESPHOME_DATA_DIR = Join-Path $rootSnapshot 'data'
  $env:ESPHOME_ESP_IDF_PREFIX = Join-Path $env:LOCALAPPDATA 'esphome\Cache\idf'
  & $PythonPath -m esphome compile $configSnapshot
  if ($LASTEXITCODE -ne 0) { throw "Compilacao $Role falhou; preserve o log." }
  Write-Host "Compilacao $Role concluida. Nenhum upload executado. Fixture nao preparada para instalacao."
} finally {
  foreach ($nameSnapshot in $namesSnapshot) { [Environment]::SetEnvironmentVariable($nameSnapshot,$savedSnapshot[$nameSnapshot],'Process') }
}
