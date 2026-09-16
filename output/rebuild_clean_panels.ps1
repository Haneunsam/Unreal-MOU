Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
public static class CleanPanels {
 static Color C(int r,int g,int b){return Color.FromArgb(255,r,g,b);}
 static GraphicsPath Rounded(float x,float y,float w,float h,float r){
  var p=new GraphicsPath();float d=r*2;
  p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);
  p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();return p;
 }
 static void Stroke(Graphics g,GraphicsPath path,Color color,float width){using(var pen=new Pen(color,width)){pen.LineJoin=LineJoin.Round;g.DrawPath(pen,path);}}
 static void Capsule(Graphics g,float x,float y,float w,float h,Color color){using(var p=Rounded(x,y,w,h,Math.Min(w,h)/2))using(var b=new SolidBrush(color))g.FillPath(b,p);}
 public static void Run(string folder,bool info){
  int w=info?144:114,h=info?280:114,scale=8,ss=2;
  string prefix=info?"T_InfoPanel":"T_SquarePanel";
  for(int layer=0;layer<2;layer++){
   using(var hi=new Bitmap(w*scale*ss,h*scale*ss,PixelFormat.Format32bppArgb)){
    using(var g=Graphics.FromImage(hi)){
     g.Clear(layer==0?Color.Transparent:Color.Black);g.SmoothingMode=SmoothingMode.AntiAlias;
     g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.ScaleTransform(scale*ss,scale*ss);
     float x=info?9.5f:7.8f,y=info?9.5f:7.7f,pw=info?125:98,ph=info?261:94.5f,r=info?12:10.5f;
     using(var outline=Rounded(x,y,pw,ph,r)){
      if(layer==0){
       using(var fill=new LinearGradientBrush(new PointF(x,y),new PointF(x+pw,y+ph),C(7,16,25),C(5,13,20)))g.FillPath(fill,outline);
       Stroke(g,outline,C(30,48,61),0.8f);
       if(info){
        Stroke(g,outline,C(67,86,106),0.65f);
        using(var pen=new Pen(C(29,43,55),0.65f))g.DrawLine(pen,27,55,121,55);
        Capsule(g,27,249.1f,88,4.1f,C(31,39,55));
        Capsule(g,10,18,1.3f,20,C(24,35,44));
        using(var brush=new SolidBrush(C(28,42,53)))g.FillPolygon(brush,new[]{new PointF(75,50.3f),new PointF(77.5f,48.6f),new PointF(118,48.6f),new PointF(121,50.3f)});
       }
      }else if(!info){
       Stroke(g,outline,C(71,108,130),0.65f);
       // White-cyan corner highlights share precisely the same rounded-rectangle arcs.
       using(var pen=new Pen(C(178,222,244),0.85f)){
        float d=r*2;
        g.DrawArc(pen,x,y,d,d,180,90);g.DrawArc(pen,x+pw-d,y,d,d,270,90);
        g.DrawArc(pen,x,y+ph-d,d,d,90,90);g.DrawArc(pen,x+pw-d,y+ph-d,d,d,0,90);
       }
      }else{
       Capsule(g,10,18,1.3f,20,C(225,239,250));
       using(var brush=new SolidBrush(C(170,204,230)))g.FillPolygon(brush,new[]{new PointF(75,50.3f),new PointF(77.5f,48.6f),new PointF(118,48.6f),new PointF(121,50.3f)});
       using(var pen=new Pen(C(228,242,255),0.8f))g.DrawLine(pen,77.5f,49.1f,105,49.1f);
       Capsule(g,27,249.1f,27,4.1f,C(248,117,193));
      }
     }
    }
    using(var output=new Bitmap(w*scale,h*scale,PixelFormat.Format32bppArgb)){
     using(var g=Graphics.FromImage(output)){
      g.CompositingMode=CompositingMode.SourceCopy;g.InterpolationMode=InterpolationMode.HighQualityBicubic;
      g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(hi,new Rectangle(0,0,output.Width,output.Height),0,0,hi.Width,hi.Height,GraphicsUnit.Pixel);
     }
     string file=System.IO.Path.Combine(folder,prefix+(layer==0?"_Base_Clean.png":"_Emissive_Clean.png"));
     output.Save(file,ImageFormat.Png);Console.WriteLine(file+" "+output.Width+"x"+output.Height);
    }
   }
  }
 }
}
'@
[CleanPanels]::Run((Join-Path $PSScriptRoot 'square_panel_exact'),$false)
[CleanPanels]::Run((Join-Path $PSScriptRoot 'info_panel_split'),$true)
