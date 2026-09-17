Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
public static class ExternalFrameClean {
 static Color C(int r,int g,int b){return Color.FromArgb(r,g,b);}
 static GraphicsPath Path(float[] xy,bool close=false){var p=new GraphicsPath();var a=new PointF[xy.Length/2];for(int i=0;i<a.Length;i++)a[i]=new PointF(xy[i*2],xy[i*2+1]);p.AddLines(a);if(close)p.CloseFigure();return p;}
 static void Line(Graphics g,Color c,float width,params float[] xy){using(var p=Path(xy))using(var pen=new Pen(c,width)){pen.LineJoin=LineJoin.Round;pen.StartCap=LineCap.Round;pen.EndCap=LineCap.Round;g.DrawPath(pen,p);}}
 static void Poly(Graphics g,Color c,params float[] xy){using(var p=Path(xy,true))using(var b=new SolidBrush(c))g.FillPath(b,p);}
 static void SideBracket(Graphics g,bool right){float x=right?1040:39;float s=right?-1:1;using(var p=new GraphicsPath()){p.AddBezier(x,166,x+s*8,167,x+s*14,173,x+s*14,182);p.AddLine(x+s*14,182,x+s*14,317);p.AddBezier(x+s*14,317,x+s*14,325,x+s*8,332,x,334);using(var pen=new Pen(C(116,110,143),1.5f))g.DrawPath(pen,p);}}
 static void Base(Graphics g){
  using(var p=Path(new float[]{65,33,352,33,377,50,702,50,727,33,992,33,1045,85,1045,430,991,487,739,487,724,488,356,488,340,487,85,487,26,428,26,74},true)){
   using(var brush=new LinearGradientBrush(new PointF(0,30),new PointF(1080,490),C(6,13,20),C(9,9,16)))g.FillPath(brush,p);
   using(var pen=new Pen(C(101,83,111),1.0f)){pen.LineJoin=LineJoin.Round;g.DrawPath(pen,p);}
  }
  Line(g,C(73,39,56),0.85f,37,146,37,84,75,46,181,46,194,39,350,39,378,69,702,69,728,39,884,39,899,46,994,46,1044,94,1044,145);
  Line(g,C(61,38,53),0.8f,53,156,53,104,87,70,353,70);
  Line(g,C(61,38,53),0.8f,726,70,993,70,1025,104,1025,156);
  Line(g,C(56,46,65),0.9f,53,347,53,415,88,449,333,449);
  Line(g,C(56,46,65),0.9f,1025,347,1025,416,992,448,744,448);
  Line(g,C(127,135,154),0.9f,87,487,177,487,191,483,333,483);
  Line(g,C(127,135,154),0.9f,745,483,890,483,904,487,990,487);
  Line(g,C(107,67,90),1,180,471,338,471,356,487,724,487,740,471,895,471);
  Line(g,C(121,133,158),0.9f,78,122,78,160);
  Line(g,C(80,96,116),0.7f,78,170,78,174);
  Line(g,C(80,96,116),0.7f,78,178,78,181);
  SideBracket(g,false);SideBracket(g,true);
  // Dim sockets occupy exactly the same coordinates as emission shapes.
  Emit(g,true);
 }
 static void Emit(Graphics g,bool off){
  Color red=off?C(33,19,26):C(255,110,137);
  Color white=off?C(26,34,42):C(219,245,255);
  Poly(g,red,25.5f,188,34,196,34,300,25.5f,308);
  Poly(g,red,1048.5f,188,1040,196,1040,300,1048.5f,308);
  Poly(g,red,93,42,153,42,149,47,90,47);
  Poly(g,red,925,42,985,42,989,47,930,47);
  Poly(g,red,90,473,137,473,143,479,94,479);
  Poly(g,red,942,473,990,473,985,479,936,479);
  Line(g,red,3.7f,397,60.5f,445,60.5f);
  Line(g,red,3.7f,635,60.5f,683,60.5f);
  Line(g,red,3.7f,460,487.5f,617,487.5f);
  Line(g,red,4.2f,35,85,48,71);
  Line(g,red,3.6f,1034,81,1043,91,1043,112);
  Line(g,red,3.9f,35,413,35,423,44,431);
  Line(g,red,3.9f,1023,444,1035,432);
  Line(g,white,1.4f,29,68,60,36,65,33,132,33);
  Line(g,white,2.3f,162,487,176,487);
  Line(g,white,2.3f,902,487,917,487);
  Line(g,off?C(37,27,35):C(184,99,132),1.2f,438,31,454,31);
  Line(g,off?C(37,27,35):C(184,99,132),1.2f,627,31,643,31);
 }
 public static void Run(string dir){
  for(int layer=0;layer<3;layer++)using(var high=new Bitmap(4320,2080,PixelFormat.Format32bppArgb)){
   using(var g=Graphics.FromImage(high)){
    g.Clear(layer==1?Color.Black:Color.Transparent);g.SmoothingMode=SmoothingMode.AntiAlias;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.ScaleTransform(4,4);
    if(layer!=1)Base(g);if(layer!=0)Emit(g,false);
   }
   using(var dst=new Bitmap(2160,1040,PixelFormat.Format32bppArgb)){
    using(var g=Graphics.FromImage(dst)){g.CompositingMode=CompositingMode.SourceCopy;g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(high,new Rectangle(0,0,2160,1040),0,0,4320,2080,GraphicsUnit.Pixel);}
    string name=layer==0?"T_ExternalFrame_Base_Clean.png":layer==1?"T_ExternalFrame_Emissive_Clean.png":"ExternalFrame_Preview.png";
    dst.Save(System.IO.Path.Combine(dir,name),ImageFormat.Png);Console.WriteLine(name+" 2160x1040");
   }
  }
 }
}
'@
[ExternalFrameClean]::Run($PSScriptRoot)
