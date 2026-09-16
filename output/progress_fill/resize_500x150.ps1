Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
public static class ProgressResize {
 public static void Run(string input,string output,bool ramp) {
  using(var src=new Bitmap(input))using(var dst=new Bitmap(500,150,PixelFormat.Format32bppArgb)) {
   using(var g=Graphics.FromImage(dst)){
    g.Clear(Color.Transparent);g.CompositingMode=CompositingMode.SourceCopy;
    g.InterpolationMode=ramp?InterpolationMode.NearestNeighbor:InterpolationMode.HighQualityBicubic;
    g.PixelOffsetMode=PixelOffsetMode.HighQuality;
    using(var attr=new ImageAttributes()){
     attr.SetWrapMode(WrapMode.TileFlipXY);
     g.DrawImage(src,new Rectangle(0,0,500,150),0,0,src.Width,src.Height,GraphicsUnit.Pixel,attr);
    }
   }
   dst.Save(output,ImageFormat.Png);
  }
 }
}
'@
$dest=Join-Path $PSScriptRoot '500x150'
New-Item -ItemType Directory -Path $dest -Force | Out-Null
[ProgressResize]::Run('C:\Users\user1\Desktop\b469d09f-f98d-4c4b-97b6-726ffb237d80.png',(Join-Path $dest 'T_Progress_Background_500x150.png'),$false)
[ProgressResize]::Run((Join-Path $PSScriptRoot 'T_Progress_Fill.png'),(Join-Path $dest 'T_Progress_Fill_500x150.png'),$false)
[ProgressResize]::Run((Join-Path $PSScriptRoot 'T_Progress_Ramp.png'),(Join-Path $dest 'T_Progress_Ramp_500x150.png'),$true)
[ProgressResize]::Run((Join-Path $PSScriptRoot 'Preview_50.png'),(Join-Path $dest 'Preview_50_500x150.png'),$false)
Get-ChildItem -LiteralPath $dest -Filter '*.png' | ForEach-Object {
 $b=[System.Drawing.Bitmap]::FromFile($_.FullName)
 "$($_.Name): $($b.Width)x$($b.Height), $($b.PixelFormat)"
 $b.Dispose()
}
