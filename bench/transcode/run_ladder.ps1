<#
.SYNOPSIS
  B2: encode thang rendition bang NVENC, do toc do va VMAF (thiet ke muc 5.3, 5.4.6).
.NOTES
  Ban khoi dau, can chinh tham so theo ban FFmpeg dang dung.
#>
param(
    [Parameter(Mandatory)] [string] $Source,
    [string] $Machine = $env:COMPUTERNAME,
    [string] $FFmpeg = "ffmpeg",
    [string] $FFprobe = "ffprobe",
    [int]    $GopSeconds = 2,
    [int]    $VmafThreads = 8
)

$ErrorActionPreference = "Stop"
$root    = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$results = Join-Path $root "bench\results"
$work    = Join-Path $results "B2_work"
New-Item -ItemType Directory -Force -Path $work | Out-Null
$Source  = (Resolve-Path $Source).Path

# Thang thu nghiem: moi bac vai muc bitrate (Mbps). Chinh tuy y.
$Rungs = @(
    @{ Name = "1080p-hevc"; Height = 1080; Codec = "hevc_nvenc"; Mbps = @(4, 6, 8) },
    @{ Name = "720p-h264";  Height = 720;  Codec = "h264_nvenc"; Mbps = @(2.5, 4, 5) },
    @{ Name = "480p-h264";  Height = 480;  Codec = "h264_nvenc"; Mbps = @(1, 1.5, 2) }
)

# Thong tin nguon
$probe = & $FFprobe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate:format=duration -of json $Source | ConvertFrom-Json
$srcW = [int]$probe.streams[0].width
$srcH = [int]$probe.streams[0].height
$fpsParts = $probe.streams[0].r_frame_rate.Split("/")
$fps = [double]$fpsParts[0] / [double]$fpsParts[1]
$duration = [double]$probe.format.duration
$gop = [int][math]::Round($fps * $GopSeconds)

$gpu = (& nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>$null) -join "; "
$ffver = (& $FFmpeg -hide_banner -version | Select-Object -First 1)
Write-Host "Nguon: ${srcW}x${srcH} @ $([math]::Round($fps,3)) fps, $([math]::Round($duration,1)) s, GOP $gop"
Write-Host "GPU: $gpu"
Write-Host "FFmpeg: $ffver"

$stamp = Get-Date -Format "yyyyMMdd"
$csv = Join-Path $results "B2_${Machine}_${stamp}.csv"
"# source=$([IO.Path]::GetFileName($Source)); gpu=$gpu; ffmpeg=$ffver" | Out-File -Encoding utf8 $csv
"rung,codec,target_mbps,realtime_x,vmaf_mean,vmaf_p5,mb_per_hour" | Out-File -Encoding utf8 -Append $csv

Push-Location $work
try {
    foreach ($r in $Rungs) {
        if ($r.Height -gt $srcH) { Write-Host "Bo qua $($r.Name): lon hon nguon"; continue }
        foreach ($mbps in $r.Mbps) {
            $out = "$($r.Name)_$($mbps)M.mp4"
            $b = "$($mbps)M"; $max = "$([math]::Round($mbps * 1.5, 2))M"; $buf = "$([math]::Round($mbps * 2, 2))M"
            Write-Host "== $($r.Name) @ $b"

            $sw = [Diagnostics.Stopwatch]::StartNew()
            & $FFmpeg -hide_banner -loglevel error -y `
                -hwaccel cuda -hwaccel_output_format cuda -i $Source `
                -map 0:v:0 -an -sn `
                -vf "scale_cuda=-2:$($r.Height)" `
                -c:v $r.Codec -preset p5 -rc vbr -b:v $b -maxrate $max -bufsize $buf `
                -g $gop -bf 2 -forced-idr 1 -force_key_frames "expr:gte(t,n_forced*$GopSeconds)" `
                $out
            $sw.Stop()
            if ($LASTEXITCODE -ne 0) { Write-Warning "Encode loi: $out"; continue }
            $rt = $duration / $sw.Elapsed.TotalSeconds

            # VMAF: phong bac len kich thuoc nguon roi so sanh. Duong dan log tuong doi de tranh dau ':' tren Windows.
            $log = "$($r.Name)_$($mbps)M_vmaf.json"
            & $FFmpeg -hide_banner -loglevel error -y -i $out -i $Source `
                -lavfi "[0:v]scale=${srcW}:${srcH}:flags=bicubic,setpts=PTS-STARTPTS[d];[1:v]setpts=PTS-STARTPTS[ref];[d][ref]libvmaf=n_threads=${VmafThreads}:log_fmt=json:log_path=$log" `
                -f null - | Out-Null
            $vmafMean = ""; $vmafP5 = ""
            if (Test-Path $log) {
                $j = Get-Content $log -Raw | ConvertFrom-Json
                $vmafMean = [math]::Round($j.pooled_metrics.vmaf.mean, 2)
                $scores = $j.frames | ForEach-Object { $_.metrics.vmaf } | Sort-Object
                if ($scores.Count -gt 0) { $vmafP5 = [math]::Round($scores[[int]($scores.Count * 0.05)], 2) }
            }
            $mbh = [math]::Round((Get-Item $out).Length / 1MB / $duration * 3600, 0)
            "$($r.Name),$($r.Codec),$mbps,$([math]::Round($rt,2)),$vmafMean,$vmafP5,$mbh" | Out-File -Encoding utf8 -Append $csv
            Write-Host ("   {0:N2}x realtime, VMAF {1} (p5 {2}), {3} MB/h" -f $rt, $vmafMean, $vmafP5, $mbh)
        }
    }
}
finally { Pop-Location }

Write-Host "Ket qua: $csv"
