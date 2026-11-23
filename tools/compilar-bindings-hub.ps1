param([switch]$OnlyGenerate)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $env:LOCALAPPDATA 'esphome-2026.9.1\venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw "Python ESPHome nao encontrado: $python" }
$version = & $python -m esphome version
if ($LASTEXITCODE -ne 0 -or "$version" -notmatch 'Version: 2026\.9\.1|2026\.9\.1') {
    throw "Ambiente esperado ESPHome 2026.9.1. Obtido: $version"
}
$buildRoot = Join-Path $env:LOCALAPPDATA 'esphome-hub-bindings-2026.9.1'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$log = Join-Path $buildRoot ('compile-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
$previousData = $env:ESPHOME_DATA_DIR
try {
    $env:ESPHOME_DATA_DIR = Join-Path $buildRoot 'data'
    $config = Join-Path $repoRoot 'examples\quartogian_nspanel_bindings_check.yaml'
    $compileArgs = @('-m', 'esphome', '-s', 'protocol_build_root', $buildRoot, 'compile', $config)
    if ($OnlyGenerate) { $compileArgs += '--only-generate' }
    & $python @compileArgs 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { throw "Validacao falhou. Log: $log" }
    Write-Host "Concluido. Perfil virtual; nenhum upload executado. Log: $log"
} finally {
    $env:ESPHOME_DATA_DIR = $previousData
}
