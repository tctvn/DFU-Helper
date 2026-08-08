# DFU Helper - One-click runner
# This script downloads the latest release from GitHub and executes it automatically.

$Repo = "tctvn/DFU-Helper"
$ExeName = "dfu_helper.exe"
$DestPath = Join-Path $env:TEMP $ExeName

if (Test-Path $DestPath) {
    Write-Host "[*] Found existing version. Preparing to update..." -ForegroundColor Cyan
    Write-Host "[*] Terminating any running instances..." -ForegroundColor Cyan
    Stop-Process -Name "dfu_helper" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

Write-Host "[*] Fetching latest release info from GitHub..." -ForegroundColor Cyan
try {
    # Get the latest release from GitHub API
    $ReleaseUrl = "https://api.github.com/repos/$Repo/releases/tags/latest"
    $Release = Invoke-RestMethod -Uri $ReleaseUrl -UseBasicParsing
    
    $DownloadUrl = $null
    foreach ($asset in $Release.assets) {
        if ($asset.name -eq $ExeName) {
            $DownloadUrl = $asset.browser_download_url
            break
        }
    }

    if (-not $DownloadUrl) {
        Write-Host "[-] Could not find $ExeName in the latest release." -ForegroundColor Red
        exit 1
    }

    Write-Host "[*] Downloading $ExeName..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $DestPath -UseBasicParsing

    Write-Host "[+] Download complete! Launching..." -ForegroundColor Green
    Start-Process -FilePath $DestPath -Wait
}
catch {
    Write-Host "[-] An error occurred: $_" -ForegroundColor Red
}
