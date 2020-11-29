# https://github.com/appveyor/ci/issues/2953#issuecomment-531580265
Write-Host "DirectX Software Development Kit..." -ForegroundColor Cyan

Write-Host "Downloading..."
$exePath = "$env:temp\DXSDK_Jun10.exe"
(New-Object Net.WebClient).DownloadFile('https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe', $exePath)

Write-Host "Installing..."
$installPath = "C:\Program Files (x86)\Microsoft DirectX SDK"
cmd /c start /wait $exePath /P $installPath /U
refreshenv

Remove-Item $exePath
Write-Host "Installed" -ForegroundColor Green