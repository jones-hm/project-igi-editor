for ($i=1; $i -le 13; $i++) {
    Write-Host "Testing Level $i..."
    $proc = Start-Process -FilePath ".\bin\Release\terrain.exe" -ArgumentList "-level $i" -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 3
    if ($proc.HasExited) {
        Write-Host "Level $i CRASHED or EXITED prematurely with exit code $($proc.ExitCode)"
    } else {
        Write-Host "Level $i seems OK, stopping..."
        Stop-Process -Id $proc.Id -Force
    }
}
