Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class ExactPanelUpscale {
 public static void Run(string input, string output, int scale) {
  using (var src = new Bitmap(input)) {
   int w=src.Width, h=src.Height, ow=w*scale, oh=h*scale;
   int[,,] p=new int[h,w,4];
   for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
    Color c=src.GetPixel(x,y);
    p[y,x,0]=c.B;p[y,x,1]=c.G;p[y,x,2]=c.R;p[y,x,3]=c.A;
   }
   using(var dst=new Bitmap(ow,oh,PixelFormat.Format32bppArgb)) {
    var d=dst.LockBits(new Rectangle(0,0,ow,oh),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);
    byte[] data=new byte[d.Stride*oh];
    for(int y=0;y<oh;y++) {
     double sy=Math.Max(0,Math.Min(h-1,(y+0.5)/scale-0.5));
     int y0=(int)Math.Floor(sy), y1=Math.Min(h-1,y0+1);double fy=sy-y0;
     for(int x=0;x<ow;x++) {
      double sx=Math.Max(0,Math.Min(w-1,(x+0.5)/scale-0.5));
      int x0=(int)Math.Floor(sx),x1=Math.Min(w-1,x0+1);double fx=sx-x0;
      for(int k=0;k<4;k++) {
       double v=(p[y0,x0,k]*(1-fx)+p[y0,x1,k]*fx)*(1-fy)+(p[y1,x0,k]*(1-fx)+p[y1,x1,k]*fx)*fy;
       data[y*d.Stride+x*4+k]=(byte)Math.Round(v,MidpointRounding.AwayFromZero);
      }
     }
    }
    Marshal.Copy(data,0,d.Scan0,data.Length);dst.UnlockBits(d);dst.Save(output,ImageFormat.Png);
   }
  }
 }
}
'@
$pairs=@(
 @{Folder='square_panel_exact';Prefix='T_SquarePanel'},
 @{Folder='info_panel_split';Prefix='T_InfoPanel'}
)
foreach($pair in $pairs) {
 $folder=Join-Path $PSScriptRoot $pair.Folder
 foreach($layer in @('Base','Emissive')) {
  $inputFile=Join-Path $folder ($pair.Prefix+'_'+$layer+'_Exact.png')
  $outputFile=Join-Path $folder ($pair.Prefix+'_'+$layer+'_8x.png')
  [ExactPanelUpscale]::Run($inputFile,$outputFile,8)
  $check=[System.Drawing.Bitmap]::FromFile($outputFile)
  "$outputFile : $($check.Width)x$($check.Height), $($check.PixelFormat)"
  $check.Dispose()
 }
}
