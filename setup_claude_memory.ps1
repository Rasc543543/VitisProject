# setup_claude_memory.ps1
# 將 .claude/memory/ 的記憶檔複製到本機 Claude Code 對應路徑
# 在新電腦 clone 專案後執行一次即可

$projectPath = "C--VitisProject"   # 對應 C:\VitisProject
$targetDir   = "$env:USERPROFILE\.claude\projects\$projectPath\memory"
$sourceDir   = "$PSScriptRoot\.claude\memory"

if (-not (Test-Path $sourceDir)) {
    Write-Host "[ERROR] 找不到 $sourceDir，請確認從專案根目錄執行此腳本" -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force $targetDir | Out-Null

$files = Get-ChildItem -Path $sourceDir -File
foreach ($f in $files) {
    $dest = Join-Path $targetDir $f.Name
    if (Test-Path $dest) {
        Write-Host "[SKIP]  $($f.Name)（已存在，不覆蓋）"
    } else {
        Copy-Item $f.FullName $dest
        Write-Host "[OK]    $($f.Name)"
    }
}

Write-Host "`n完成。記憶檔已複製至：$targetDir" -ForegroundColor Green
Write-Host "重新開啟 Claude Code 後即可使用。" -ForegroundColor Green
