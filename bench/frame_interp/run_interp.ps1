<#
.SYNOPSIS
  B6: do cac bac noi suy khung hinh phia client (thiet ke muc 11.3.3).
.DESCRIPTION
  Lay mot doan nguon 60 fps lam chuan, bo moi khung le de co ban 30 fps, cho tung bac noi suy
  dung lai 60 fps, roi do toc do va VMAF so voi doan chuan.
.NOTES
  Ban khoi dau. Bac 3 (RIFE) can -Rife tro toi rife-ncnn-vulkan.exe va chay tren anh PNG nen
  nen do o 1080p tro xuong; toc do bac 3 chi tinh thoi gian goi RIFE, khong tinh xuat/nap PNG.
#>
param(
    [Parameter(Mandatory)] [string] $Source,
    [string] $Machine = $env:COMPUTERNAME,
    [int]    $Height = 1080,
    [int]    $Start = 10,
    [int]    $Seconds = 5,
    [string] $Rife = "",
    [string] $RifeModel = "rife-v4.6",
    [string[]] $Tiers = @("t0-repeat", "t1-blend", "t1-placebo", "t2-mci", "t3-rife"),
    [string] $FFmpeg = "ffmpeg",
    [string] $FFprobe = "ffprobe",
    [int]    $VmafThreads = 8
)

$root    = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$results = Join-Path $root "bench\results"
$work    = Join-Path $results "B6_work"
New-Item -ItemType Directory -Force -Path $work | Out-Null
$Source  = (Resolve-Path $Source).Path

$probe = & $FFprobe -v error -select_streams v:0 -show_entries stream=r_frame_rate -of csv=p=0 $Source
$fpsNum, $fpsDen = ($probe.Trim() -split "/") | ForEach-Object { [int]$_ }
if (-not $fpsDen) { $fpsDen = 1 }
$fps    = "$fpsNum/$fpsDen"
$half   = "$fpsNum/$($fpsDen * 2)"
$gpu    = (& nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>$null) -join "; "
$ffver  = (& $FFmpeg -hide_banner -version | Select-Object -First 1)
Write-Host "Nguon: $fps fps, doan $Seconds s tu giay $Start, cao $Height; GPU: $gpu"
if ($fpsNum / $fpsDen -lt 50) { Write-Warning "Nguon duoi 50 fps: khong co chuan de so sanh." }

$stamp = Get-Date -Format "yyyyMMdd"
$csv = Join-Path $results "B6_${Machine}_${stamp}.csv"
"# source=$([IO.Path]::GetFileName($Source)); height=$Height; seconds=$Seconds; gpu=$gpu; ffmpeg=$ffver" | Out-File -Encoding utf8 $csv
"tier,realtime_x,vmaf_mean,vmaf_p5,frames_out,note" | Out-File -Encoding utf8 -Append $csv

function Invoke-Tool([string[]] $ToolArgs) {
    # FFmpeg ghi canh bao ra stderr; mot bac loi khong duoc dung ca script.
    $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    & $FFmpeg @ToolArgs 2>&1 | Out-Null
    $ErrorActionPreference = $old
    return $LASTEXITCODE
}

Push-Location $work
try {
    $scale = "scale=-2:${Height}:flags=lanczos,format=yuv420p"
    # Chuan 60 fps, khong mat du lieu
    Invoke-Tool @("-hide_banner","-loglevel","error","-y","-ss","$Start","-t","$Seconds","-i",$Source,
        "-map","0:v:0","-an","-vf",$scale,"-c:v","ffv1","-r",$fps,"ref.mkv") | Out-Null
    # Ban 30 fps: bo khung le
    Invoke-Tool @("-hide_banner","-loglevel","error","-y","-i","ref.mkv",
        "-vf","select='not(mod(n\,2))',setpts=N*$($fpsDen * 2)/$fpsNum/TB",
        "-c:v","ffv1","-r",$half,"dec.mkv") | Out-Null
    $refFrames = [int](& $FFprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of csv=p=0 ref.mkv)

    $filters = @{
        "t0-repeat"  = @{ Pre = @(); Vf = "fps=$fps" }
        "t1-blend"   = @{ Pre = @(); Vf = "framerate=fps=${fps}:interp_start=0:interp_end=255" }
        "t1-placebo" = @{ Pre = @("-init_hw_device","vulkan"); Vf = "libplacebo=fps=${fps}:frame_mixer=linear:format=yuv420p" }
        "t2-mci"     = @{ Pre = @(); Vf = "minterpolate=fps=${fps}:mi_mode=mci:mc_mode=aobmc:me_mode=bidir:vsbmc=1" }
    }

    foreach ($t in $Tiers) {
        $out = "out_$t.mkv"
        Remove-Item $out -ErrorAction SilentlyContinue
        $note = ""
        $rt = ""
        if ($t -eq "t3-rife") {
            if (-not $Rife) { Write-Host "Bo qua t3-rife: chua co -Rife"; continue }
            Remove-Item rife_in, rife_out -Recurse -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Force rife_in, rife_out | Out-Null
            Invoke-Tool @("-hide_banner","-loglevel","error","-y","-i","dec.mkv","rife_in\%08d.png") | Out-Null
            $nIn = (Get-ChildItem rife_in).Count
            $sw = [Diagnostics.Stopwatch]::StartNew()
            $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"
            & $Rife -i rife_in -o rife_out -m $RifeModel -n ($nIn * 2) 2>&1 | Out-Null
            $ErrorActionPreference = $old
            $sw.Stop()
            if ($LASTEXITCODE -ne 0) { Write-Warning "RIFE loi"; continue }
            $rt = [math]::Round($Seconds / $sw.Elapsed.TotalSeconds, 2)
            $note = "chi tinh thoi gian RIFE ($RifeModel), khong tinh PNG"
            Invoke-Tool @("-hide_banner","-loglevel","error","-y","-framerate",$fps,"-i","rife_out\%08d.png",
                "-vf","format=yuv420p","-c:v","ffv1",$out) | Out-Null
        } else {
            $f = $filters[$t]
            if (-not $f) { Write-Warning "Khong biet bac $t"; continue }
            $sw = [Diagnostics.Stopwatch]::StartNew()
            $code = Invoke-Tool (@("-hide_banner","-loglevel","error","-y") + $f.Pre + @("-i","dec.mkv","-vf",$f.Vf,"-c:v","ffv1","-r",$fps,$out))
            $sw.Stop()
            if ($code -ne 0 -or -not (Test-Path $out)) { Write-Warning "Bac $t loi"; "$t,,,,,loi" | Out-File -Encoding utf8 -Append $csv; continue }
            $rt = [math]::Round($Seconds / $sw.Elapsed.TotalSeconds, 2)
            $note = "gom decode va ffv1"
        }
        $log = "${t}_vmaf.json"
        Remove-Item $log -ErrorAction SilentlyContinue
        Invoke-Tool @("-hide_banner","-loglevel","error","-y","-i",$out,"-i","ref.mkv","-lavfi",
            "[0:v]setpts=PTS-STARTPTS[d];[1:v]setpts=PTS-STARTPTS[r];[d][r]libvmaf=n_threads=${VmafThreads}:log_fmt=json:log_path=$log",
            "-f","null","-") | Out-Null
        $mean = ""; $p5 = ""; $n = ""
        if (Test-Path $log) {
            $j = Get-Content $log -Raw | ConvertFrom-Json
            $mean = [math]::Round($j.pooled_metrics.vmaf.mean, 2)
            $scores = $j.frames | ForEach-Object { $_.metrics.vmaf } | Sort-Object
            $n = $scores.Count
            if ($n -gt 0) { $p5 = [math]::Round($scores[[int]($n * 0.05)], 2) }
        }
        "$t,$rt,$mean,$p5,$n,$note" | Out-File -Encoding utf8 -Append $csv
        Write-Host ("{0,-11} {1,6}x realtime, VMAF {2} (p5 {3}), {4}/{5} khung" -f $t, $rt, $mean, $p5, $n, $refFrames)
    }
}
finally { Pop-Location }

Write-Host "Ket qua: $csv  (xoa $work sau khi xem)"
