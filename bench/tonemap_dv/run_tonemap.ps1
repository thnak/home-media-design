<#
.SYNOPSIS
  B3: so sanh tone-map HDR/Dolby Vision sang SDR (thiet ke muc 5.5.1).
.NOTES
  Ban khoi dau, can chinh tham so theo ban FFmpeg dang dung.
#>
param(
    [Parameter(Mandatory)] [string] $Source,
    [int[]]  $Times = @(60, 300, 900),
    [string] $FFmpeg = "ffmpeg",
    [int]    $SpeedSeconds = 60
)

$ErrorActionPreference = "Stop"
$root   = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Source = (Resolve-Path $Source).Path
$name   = [IO.Path]::GetFileNameWithoutExtension($Source)
$outDir = Join-Path $root "bench\results\B3_$name"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$Methods = [ordered]@{
    # libplacebo: tu ap RPU Dolby Vision (apply_dolbyvision mac dinh bat), tone-map bt.2390
    "libplacebo" = @{
        Pre = @("-init_hw_device", "vulkan")
        Vf  = "libplacebo=tonemapping=bt.2390:colorspace=bt709:color_primaries=bt709:color_trc=bt709:range=tv:format=yuv420p:w=1920:h=-2"
    }
    # CPU: zscale + tonemap hable, khong hieu RPU Dolby Vision
    "zscale-hable" = @{
        Pre = @()
        Vf  = "zscale=w=1920:h=-2,zscale=t=linear:npl=100,format=gbrpf32le,zscale=p=bt709,tonemap=tonemap=hable:desat=0,zscale=t=bt709:m=bt709:r=tv,format=yuv420p"
    }
    # Khong xu ly: thay loi mau khi phat DV5 nhu HDR thuong
    "untouched" = @{
        Pre = @()
        Vf  = "scale=1920:-2,format=yuv420p"
    }
}

# FFmpeg ghi loi ra stderr; mot phuong an loi (vd zscale voi DV5) khong duoc dung ca script.
$ErrorActionPreference = "Continue"
foreach ($m in $Methods.Keys) {
    foreach ($t in $Times) {
        $png = Join-Path $outDir ("{0}_t{1:D5}.png" -f $m, $t)
        & $FFmpeg -hide_banner -loglevel error -y @($Methods[$m].Pre) -ss $t -i $Source `
            -map 0:v:0 -vf $Methods[$m].Vf -frames:v 1 $png
        if ($LASTEXITCODE -ne 0) { Write-Warning "Loi: $m t=$t" }
    }
}

$dur = [double](& ffprobe -v error -show_entries format=duration -of csv=p=0 $Source)
Write-Host "Toc do tone-map $SpeedSeconds giay phim (ra null):"
foreach ($m in @("libplacebo", "zscale-hable")) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $FFmpeg -hide_banner -loglevel error -y @($Methods[$m].Pre) -i $Source -t $SpeedSeconds `
        -map 0:v:0 -vf $Methods[$m].Vf -f null -
    $sw.Stop()
    if ($LASTEXITCODE -ne 0) { Write-Host ("  {0,-14} loi" -f $m); continue }
    $played = [math]::Min($SpeedSeconds, $dur)
    Write-Host ("  {0,-14} {1:N2}x realtime" -f $m, ($played / $sw.Elapsed.TotalSeconds))
}

Write-Host "Anh so sanh: $outDir"
