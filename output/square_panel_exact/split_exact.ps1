Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile('C:\Users\user1\Desktop\square-panel-exact.png')
$base = New-Object System.Drawing.Bitmap($src.Width,$src.Height)
$emit = New-Object System.Drawing.Bitmap($src.Width,$src.Height)
# Keep a dim, color-preserving rim in base; separate brighter rim pixels.
# The threshold is an artistic unlit estimate, not recovered source layers.
$unlitPeak = 35
$changed = 0
$interiorChanged = 0
for ($y=0; $y -lt $src.Height; $y++) {
    for ($x=0; $x -lt $src.Width; $x++) {
        $c=$src.GetPixel($x,$y)
        $rgb=@([int]$c.R,[int]$c.G,[int]$c.B)
        $peak=($rgb | Measure-Object -Maximum).Maximum
        $b=@($rgb[0],$rgb[1],$rgb[2])
        # Restrict extraction to the narrow outer frame region.
        $isFrame=($x -le 19 -or $x -ge 94 -or $y -le 12 -or $y -ge 97)
        if ($isFrame -and $c.A -gt 0 -and $peak -gt $unlitPeak) {
            for ($k=0; $k -lt 3; $k++) { $b[$k]=[int][Math]::Round($rgb[$k]*$unlitPeak/[double]$peak) }
            $changed++
        }
        $e=@(($rgb[0]-$b[0]),($rgb[1]-$b[1]),($rgb[2]-$b[2]))
        $base.SetPixel($x,$y,[System.Drawing.Color]::FromArgb($c.A,$b[0],$b[1],$b[2]))
        $emit.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$e[0],$e[1],$e[2]))
    }
}
$basePath=Join-Path $PSScriptRoot 'T_SquarePanel_Base_Exact.png'
$emitPath=Join-Path $PSScriptRoot 'T_SquarePanel_Emissive_Exact.png'
$base.Save($basePath,[System.Drawing.Imaging.ImageFormat]::Png)
$emit.Save($emitPath,[System.Drawing.Imaging.ImageFormat]::Png)
$base.Dispose(); $emit.Dispose()
$base=[System.Drawing.Bitmap]::FromFile($basePath)
$emit=[System.Drawing.Bitmap]::FromFile($emitPath)
$maxError=0; $alphaErrors=0
for ($y=0; $y -lt $src.Height; $y++) {
    for ($x=0; $x -lt $src.Width; $x++) {
        $c=$src.GetPixel($x,$y); $b=$base.GetPixel($x,$y); $e=$emit.GetPixel($x,$y)
        foreach ($channel in @('R','G','B')) {
            $maxError=[Math]::Max($maxError,[Math]::Abs([int]$c.$channel-[int]$b.$channel-[int]$e.$channel))
        }
        if ($b.A -ne $c.A) { $alphaErrors++ }
        if ($x -ge 20 -and $x -le 93 -and $y -ge 13 -and $y -le 96 -and $c.ToArgb() -ne $b.ToArgb()) { $interiorChanged++ }
    }
}
$report="Size: $($src.Width)x$($src.Height); separated pixels: $changed; max stored RGB reconstruction error: $maxError; base alpha errors: $alphaErrors; interior changed pixels: $interiorChanged"
$report
$report | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'verification.txt') -Encoding UTF8
$src.Dispose(); $base.Dispose(); $emit.Dispose()
if ($maxError -ne 0 -or $alphaErrors -ne 0 -or $interiorChanged -ne 0) { throw 'Verification failed' }
