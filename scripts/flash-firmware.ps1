param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [int]$Baud = 460800,
    [switch]$Erase
)

$ErrorActionPreference = "Stop"
$required = @("camper-level-factory.bin")
foreach ($file in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $file))) {
        throw "Falta $file. Ejecuta este script dentro del ZIP oficial de firmware."
    }
}

if (Get-Command py -ErrorAction SilentlyContinue) {
    $python = "py"
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $python = "python"
} else {
    throw "No se encontró Python. Instálalo y ejecuta: py -m pip install esptool"
}

& $python -m esptool version *> $null
if ($LASTEXITCODE -ne 0) {
    throw "No se encontró esptool. Ejecuta: $python -m pip install esptool"
}

if ($Erase) {
    Write-Host "Borrando toda la flash; también se perderán Wi-Fi, medidas y nivel 0."
    & $python -m esptool --chip esp32c3 --port $Port erase-flash
    if ($LASTEXITCODE -ne 0) { throw "No se pudo borrar el ESP32-C3." }
}

Write-Host "Grabando Camper Level en $Port..."
& $python -m esptool --chip esp32c3 --port $Port --baud $Baud `
    --before default-reset --after hard-reset write-flash `
    0x0000 (Join-Path $PSScriptRoot "camper-level-factory.bin")
if ($LASTEXITCODE -ne 0) { throw "La grabación falló." }

Write-Host "Grabación terminada. Configura el equipo con:"
Write-Host "  py .\configure-esp.py --port $Port"
Write-Host "También puedes abrir el puerto a 115200 baudios y escribir HELP."
