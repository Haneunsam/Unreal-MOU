Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
public static class ProgressFill {
 static GraphicsPath Segment(float offset){
  var p=new GraphicsPath();p.AddPolygon(new[]{new PointF(353+offset,289),new PointF(527+offset,289),new PointF(527+offset,298),new PointF(474+offset,373),new PointF(465+offset,380),new PointF(294+offset,380),new PointF(294+offset,366)});return p;
 }
 public static void Run(string dir,string source){
  const int w=2172,h=724;float s=w/2048f;float[] offsets={0,245.5f,491,737,982.5f,1228};
  using(var mask=new Bitmap(w,h,PixelFormat.Format32bppArgb)){
   using(var g=Graphics.FromImage(mask)){g.Clear(Color.Transparent);g.SmoothingMode=SmoothingMode.AntiAlias;g.ScaleTransform(s,s);foreach(float off in offsets)using(var p=Segment(off))g.FillPath(Brushes.White,p);}
   using(var fill=new Bitmap(w,h,PixelFormat.Format32bppArgb))using(var ramp=new Bitmap(w,h,PixelFormat.Format32bppArgb)){
    int count=0;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
     int a=mask.GetPixel(x,y).A;
     fill.SetPixel(x,y,Color.FromArgb(a,255,255,255));
     int v=254;
     if(a>0){
      count++;float px=x/s,py=y/s;int index=0;
      for(int i=0;i<6;i++){float local=px-offsets[i];if(local>=292&&local<=529){index=i;break;}}
      float left=353-(py-289)*59/91;
      float localProgress=Math.Max(0,Math.Min(1,(px-offsets[index]-left)/174));
      int lo=(int)Math.Floor(255.0*index/6)+1; int hi=Math.Min(254,(int)Math.Floor(255.0*(index+1)/6)); v=(int)Math.Round(lo+(hi-lo)*localProgress);
     }
     
     ramp.SetPixel(x,y,Color.FromArgb(255,v,v,v));
    }
    fill.Save(System.IO.Path.Combine(dir,"T_Progress_Fill.png"),ImageFormat.Png);
    ramp.Save(System.IO.Path.Combine(dir,"T_Progress_Ramp.png"),ImageFormat.Png);
    foreach(int percent in new[]{0,50,100})using(var preview=new Bitmap(source)){
     for(int y=0;y<h;y++)for(int x=0;x<w;x++){
      var m=fill.GetPixel(x,y);if(m.A==0||ramp.GetPixel(x,y).R/255f>percent/100f)continue;
      var c=preview.GetPixel(x,y);float a=m.A/255f*0.87f;
      preview.SetPixel(x,y,Color.FromArgb(255,(int)(c.R*(1-a)+255*a),(int)(c.G*(1-a)+43*a),(int)(c.B*(1-a)+91*a)));
     }
     preview.Save(System.IO.Path.Combine(dir,"Preview_"+percent+".png"),ImageFormat.Png);
    }
    Console.WriteLine("Fill and ramp: 2172x724; filled mask pixels="+count+"; previews: 0,50,100 percent");
   }
  }
 }
}
'@
[ProgressFill]::Run($PSScriptRoot,'C:\Users\user1\Desktop\b469d09f-f98d-4c4b-97b6-726ffb237d80.png')

