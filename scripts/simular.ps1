param(
    [Parameter(Position = 0)] [double] $TemperaturaAtual,
    [Parameter(Position = 1)] [double] $TemperaturaAnterior,
    [Parameter(Position = 2)] [double] $Setpoint = 25.0
)

$projeto = Split-Path -Parent $PSScriptRoot
$compilador = Join-Path $env:USERPROFILE '.platformio\packages\toolchain-gccmingw32\bin\g++.exe'
$platformio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$fonte = Join-Path $projeto 'scripts\simulador_cli.cpp'
$saida = Join-Path $projeto '.pio\build\simulador_estufa.exe'

if (-not (Test-Path -LiteralPath $compilador)) {
    if (-not (Test-Path -LiteralPath $platformio)) {
        Write-Error 'PlatformIO nao foi encontrado no perfil do usuario.'
        exit 1
    }
    Push-Location $projeto
    try {
        & $platformio pkg install -e native
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Pop-Location
    }
    if (-not (Test-Path -LiteralPath $compilador)) {
        Write-Error 'O compilador nativo nao foi instalado pelo PlatformIO.'
        exit 1
    }
}

$env:Path = "$(Split-Path -Parent $compilador);$env:Path"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saida) | Out-Null
& $compilador -std=gnu++11 -Wall -Wextra $fonte -o $saida
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($PSBoundParameters.ContainsKey('TemperaturaAtual') -and
    $PSBoundParameters.ContainsKey('TemperaturaAnterior')) {
    & $saida $TemperaturaAtual $TemperaturaAnterior $Setpoint
}
else {
    & $saida
}
exit $LASTEXITCODE
