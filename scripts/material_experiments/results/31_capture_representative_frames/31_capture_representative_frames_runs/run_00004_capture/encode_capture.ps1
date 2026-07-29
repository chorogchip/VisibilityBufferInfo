$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$frames = Join-Path $root 'frames/frame_%06d.png'
$video = Join-Path $root 'video.mp4'
ffmpeg -y -framerate 60 -i $frames -frames:v 5 -c:v libx264 -pix_fmt yuv420p -crf 18 $video
