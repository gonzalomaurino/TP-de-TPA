# Prepara la simulacion en Wokwi SIN tocar los sketches originales.
# Copia cada sketch, le cambia solo lo necesario para el simulador y lo compila:
#   - WiFi -> "Wokwi-GUEST" (la red virtual de Wokwi, sin clave)
#   - Pantalla SH1106 -> SSD1306 (Wokwi no tiene la SH1106; se usan igual)
# Uso: click derecho -> "Ejecutar con PowerShell", o desde una terminal:
#   powershell -ExecutionPolicy Bypass -File .\preparar_simulacion.ps1

$ErrorActionPreference = "Stop"
$aqui = $PSScriptRoot
$proyecto = Split-Path $aqui -Parent
$cli = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\ArduinoSA.IDE.stable_Microsoft.Winget.Source_8wekyb3d8bbwe\resources\app\lib\backend\resources\arduino-cli.exe"
$cfg = "$env:USERPROFILE\.arduinoIDE\arduino-cli.yaml"
$placa = "esp32:esp32:esp32doit-devkit-v1"

$sketches = @(
  @{ nombre = "sketch_envio_grupo_24";   destino = "envio" },
  @{ nombre = "sketch_lectura_grupo_24"; destino = "lectura" }
)

foreach ($s in $sketches) {
  $nombre = $s.nombre
  Write-Host "`n=== $nombre ===" -ForegroundColor Cyan

  # 1) Copia del sketch con los cambios para el simulador
  $carpetaCopia = Join-Path $aqui "_codigo\$nombre"
  New-Item -ItemType Directory -Force $carpetaCopia | Out-Null
  $codigo = Get-Content (Join-Path $proyecto "$nombre\$nombre.ino") -Raw
  $codigo = $codigo -replace 'const char \*WIFI_SSID = "[^"]*";', 'const char *WIFI_SSID = "Wokwi-GUEST";'
  $codigo = $codigo -replace 'const char \*WIFI_PASS = "[^"]*";', 'const char *WIFI_PASS = "";'
  $codigo = $codigo -replace '#include <Adafruit_SH110X.h>', '#include <Adafruit_SSD1306.h>'
  $codigo = $codigo -replace 'Adafruit_SH1106G display = Adafruit_SH1106G\(128, 64, &Wire, -1\);', 'Adafruit_SSD1306 display(128, 64, &Wire, -1);'
  $codigo = $codigo -replace 'display\.begin\(0x3C, true\);', 'display.begin(SSD1306_SWITCHCAPVCC, 0x3C);'
  $codigo = $codigo -replace 'SH110X_WHITE', 'SSD1306_WHITE'
  Set-Content -Path (Join-Path $carpetaCopia "$nombre.ino") -Value $codigo -NoNewline

  # 2) Compilacion: deja el firmware en <destino>\build, donde lo busca wokwi.toml
  $build = Join-Path $aqui "$($s.destino)\build"
  & $cli --config-file $cfg compile --fqbn $placa --output-dir $build $carpetaCopia
  if ($LASTEXITCODE -ne 0) { throw "Fallo la compilacion de $nombre" }
}

Write-Host "`nListo. En VS Code: F1 -> 'Wokwi: Select Config File' -> elegir envio\wokwi.toml o lectura\wokwi.toml" -ForegroundColor Green
