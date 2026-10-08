$projeto = Split-Path -Parent $PSScriptRoot
$platformio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$mingw = Join-Path $env:USERPROFILE '.platformio\packages\toolchain-gccmingw32\bin'

if (-not (Test-Path -LiteralPath $platformio)) {
    Write-Error 'PlatformIO nao foi encontrado no perfil do usuario.'
    exit 1
}

# O pacote e declarado em platformio.ini. O caminho pode ainda nao existir na
# primeira execucao; o PlatformIO o cria antes da etapa de compilacao.
$env:Path = "$mingw;$env:Path"

Push-Location $projeto
try {
    & $platformio test -e native
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
