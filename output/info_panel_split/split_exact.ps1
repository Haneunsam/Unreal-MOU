Add-Type -AssemblyName System.Drawing
$sourcePath = 'C:\Users\user1\Downloads\info-panel-exact.png'
$outputDir = $PSScriptRoot
$src = [System.Drawing.Bitmap]::FromFile($sourcePath)
$base = New-Object System.Drawing.Bitmap($src.Width, $src.Height)
$emit = New-Object System.Drawing.Bitmap($src.Width, $src.Height)
# Estimate the unlit background across each small light from adjacent pixels.
# Subtract only positive RGB differences. Base + Emission then equals source exactly.
$regions = @(
    @{ X0=7; X1=14; Y0=16; Y1=41; Axis='x' },
    @{ X0=72; X1=122; Y0=46; Y1=53; Axis='y' },
    @{ X0=19; X1=63; Y0=240; Y1=263; Axis='y' }
)
$changed = 0
$maxError = 0
for ($y=0; $y -lt $src.Height; $y++) {
    for ($x=0; $x -lt $src.Width; $x++) {
        $c = $src.GetPixel($x,$y)
        $rgb = @([int]$c.R,[int]$c.G,[int]$c.B)
        $e = @(0,0,0)
        foreach ($region in $regions) {
            if ($x -ge $region.X0 -and $x -le $region.X1 -and $y -ge $region.Y0 -and $y -le $region.Y1) {
                if ($region.Axis -eq 'x') {
                    $a=$src.GetPixel(($region.X0-1),$y)
                    $b=$src.GetPixel(($region.X1+1),$y)
                    $t=($x-$region.X0+1)/[double]($region.X1-$region.X0+2)
                } else {
                    $a=$src.GetPixel($x,($region.Y0-1))
                    $b=$src.GetPixel($x,($region.Y1+1))
                    $t=($y-$region.Y0+1)/[double]($region.Y1-$region.Y0+2)
                }
                $ar=@([int]$a.R,[int]$a.G,[int]$a.B)
                $br=@([int]$b.R,[int]$b.G,[int]$b.B)
                for ($k=0; $k -lt 3; $k++) {
                    $background=[int][Math]::Round($ar[$k]*(1-$t)+$br[$k]*$t)
                    $e[$k]=[Math]::Max(0,$rgb[$k]-$background)
                }
                break
            }
        }
        if (($e[0]+$e[1]+$e[2]) -gt 0) { $changed++ }
        $base.SetPixel($x,$y,[System.Drawing.Color]::FromArgb($c.A,($rgb[0]-$e[0]),($rgb[1]-$e[1]),($rgb[2]-$e[2])))
        $emit.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$e[0],$e[1],$e[2]))
    }
}
$basePath=Join-Path $outputDir 'T_InfoPanel_Base_Exact.png'
$emitPath=Join-Path $outputDir 'T_InfoPanel_Emissive_Exact.png'
$base.Save($basePath,[System.Drawing.Imaging.ImageFormat]::Png)
$emit.Save($emitPath,[System.Drawing.Imaging.ImageFormat]::Png)
$base.Dispose()
$emit.Dispose()
# Verify the saved PNG files, including source alpha on base.
$base=[System.Drawing.Bitmap]::FromFile($basePath)
$emit=[System.Drawing.Bitmap]::FromFile($emitPath)
$alphaErrors=0
for ($y=0; $y -lt $src.Height; $y++) {
    for ($x=0; $x -lt $src.Width; $x++) {
        $c=$src.GetPixel($x,$y); $b=$base.GetPixel($x,$y); $e=$emit.GetPixel($x,$y)
        foreach ($channel in @('R','G','B')) {
            $maxError=[Math]::Max($maxError,[Math]::Abs([int]$c.$channel-[int]$b.$channel-[int]$e.$channel))
        }
        if ($c.A -ne $b.A) { $alphaErrors++ }
    }
}
"Size: $($src.Width)x$($src.Height); changed pixels: $changed; max RGB reconstruction error: $maxError; alpha errors: $alphaErrors"
$src.Dispose(); $base.Dispose(); $emit.Dispose()
if ($maxError -ne 0 -or $alphaErrors -ne 0) { throw 'Reconstruction verification failed.' }
