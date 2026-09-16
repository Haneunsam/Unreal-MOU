Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
public static class DangerFrameClean {
 static Color C(int r,int g,int b){return Color.FromArgb(r,g,b);}
 static GraphicsPath P(params float[] xy){var p=new GraphicsPath();PointF[] a=new PointF[xy.Length/2];for(int i=0;i<a.Length;i++)a[i]=new PointF(xy[2*i],xy[2*i+1]);p.AddLines(a);p.CloseFigure();return p;}
 static void Line(Graphics g,Color c,float width,params float[] xy){PointF[] a=new PointF[xy.Length/2];for(int i=0;i<a.Length;i++)a[i]=new PointF(xy[2*i],xy[2*i+1]);using(var pen=new Pen(c,width)){pen.LineJoin=LineJoin.Round;g.DrawLines(pen,a);}}
 static void Fill(Graphics g,Color c,params float[] xy){using(var p=P(xy))using(var b=new SolidBrush(c))g.FillPath(b,p);}
 static void Lights(Graphics g,bool dim){
  Color bright=dim?C(50,28,33):C(255,58,79);
  Color faint=dim?C(34,27,32):C(130,38,53);
  Line(g,bright,1.6f,33.5f,38.5f,53,59.4f);
  Line(g,bright,1.6f,296.5f,38.5f,276,59.4f);
  Line(g,faint,0.65f,15.5f,16.3f,21,21.5f,21,25.5f);
  Line(g,faint,0.65f,315,16.3f,309,21.5f,309,25.5f);
  Line(g,dim?C(35,28,32):C(100,36,47),0.5f,53,59.5f,276,59.5f);
 }
 static void Base(Graphics g){
  using(var p=P(4,3,318,3,322,7,322,72,4,72))using(var b=new LinearGradientBrush(new PointF(0,3),new PointF(0,72),C(20,28,34),C(37,44,51)))g.FillPath(b,p);
  Fill(g,C(17,23,29),14,4,310,4,310,12,304,18,304,26,297,33,297,37,276,59.5f,53,59.5f,33,38,33,33,25,26,25,19,14,10);
  Fill(g,C(13,19,25),53,60,99,60,104,65,229,65,234,60,276,60,276,61,234,61,230,66,103,66,98,61,53,61);
  Line(g,C(25,34,41),0.7f,36,72,36,56,32,51);
  Line(g,C(25,34,41),0.7f,290,72,290,55,298,48);
  Lights(g,true);
 }
 public static void Run(string dir){
  for(int mode=0;mode<3;mode++)using(var hi=new Bitmap(2576,576,PixelFormat.Format32bppArgb)){
   using(var g=Graphics.FromImage(hi)){
    g.Clear(mode==1?Color.Black:Color.Transparent);g.SmoothingMode=SmoothingMode.AntiAlias;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.ScaleTransform(8,8);
    if(mode!=1)Base(g);if(mode!=0)Lights(g,false);
   }
   using(var dst=new Bitmap(1288,288,PixelFormat.Format32bppArgb)){
    using(var g=Graphics.FromImage(dst)){g.CompositingMode=CompositingMode.SourceCopy;g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(hi,new Rectangle(0,0,1288,288),0,0,2576,576,GraphicsUnit.Pixel);}
    string name=mode==0?"T_DangerFrame_Base_Clean.png":mode==1?"T_DangerFrame_Emissive_Clean.png":"DangerFrame_Preview.png";
    dst.Save(System.IO.Path.Combine(dir,name),ImageFormat.Png);Console.WriteLine(name+" 1288x288");
   }
  }
 }
}
'@
[DangerFrameClean]::Run($PSScriptRoot)
