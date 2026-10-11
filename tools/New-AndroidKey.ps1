<#
.SYNOPSIS
    Creates the Android release signing key, once. Run it yourself; it is never committed.

.DESCRIPTION
    - Creates %USERPROFILE%\.julybangla\android-release.jks with a random password.
    - Writes android-release.properties next to it and points android\local.properties at it,
      so `gradlew assembleRelease` signs on this computer.
    - Writes github-secrets.txt with the four values to paste into GitHub
      (Settings -> Secrets and variables -> Actions), so releases attach a signed APK.

    BACK UP the .julybangla folder (for example on a USB drive). If the key is lost, the app
    can never be updated for existing users. Never share it or put it in the repository.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\New-AndroidKey.ps1
#>
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $env:USERPROFILE '.julybangla'
$jks = Join-Path $dir 'android-release.jks'
if (Test-Path $jks) { throw "A key already exists: $jks. Keep using it; do not create a second one." }

$keytool = Get-ChildItem "$env:ProgramFiles\Microsoft\jdk-17*\bin\keytool.exe", "$env:JAVA_HOME\bin\keytool.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $keytool) { throw 'keytool not found. Install JDK 17 (winget install Microsoft.OpenJDK.17).' }

New-Item -ItemType Directory -Force $dir | Out-Null
$bytes = New-Object byte[] 24
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
$password = ([Convert]::ToBase64String($bytes) -replace '[^A-Za-z0-9]', '')

& $keytool.FullName -genkeypair -keystore $jks -storetype PKCS12 -alias julybangla -keyalg RSA -keysize 4096 `
    -validity 10000 -storepass $password -keypass $password `
    -dname 'CN=July Bangla Keyboard, O=July Bangla Keyboard, C=BD'
if ($LASTEXITCODE -ne 0) { throw 'keytool failed.' }

$props = Join-Path $dir 'android-release.properties'
@(
    "storeFile=$($jks -replace '\\', '/')"
    "storePassword=$password"
    'keyAlias=julybangla'
    "keyPassword=$password"
) | Set-Content -Path $props -Encoding ASCII

$local = Join-Path $repo 'android\local.properties'
$line = "july.signing=$($props -replace '\\', '/')"
if (-not (Test-Path $local) -or -not (Select-String -Path $local -Pattern '^july\.signing=' -Quiet)) {
    Add-Content -Path $local -Value $line -Encoding ASCII
}

$secrets = Join-Path $dir 'github-secrets.txt'
@(
    'Paste each value into GitHub: Settings -> Secrets and variables -> Actions -> New repository secret'
    ''
    'Name:  ANDROID_KEYSTORE_BASE64'
    "Value: $([Convert]::ToBase64String([IO.File]::ReadAllBytes($jks)))"
    ''
    'Name:  ANDROID_KEYSTORE_PASSWORD'
    "Value: $password"
    ''
    'Name:  ANDROID_KEY_ALIAS'
    'Value: julybangla'
    ''
    'Name:  ANDROID_KEY_PASSWORD'
    "Value: $password"
) | Set-Content -Path $secrets -Encoding ASCII

Write-Host ''
Write-Host "Key created in $dir" -ForegroundColor Green
Write-Host "1. Open $secrets and add the four GitHub secrets."
Write-Host "2. BACK UP the folder $dir. Losing the key means the app can never be updated."
