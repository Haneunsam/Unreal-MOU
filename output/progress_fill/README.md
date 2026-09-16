# Progress Fill

Source and both textures: 2172 x 724. Use the same full-canvas UVs as the source frame; do not crop only one texture.

- T_Progress_Fill.png: white RGB, antialiased alpha for the six inset shapes. Transparent outside. Tint in the material.
- T_Progress_Ramp.png: grayscale progression coordinate, normalized across the six cells. Set sRGB OFF. R/G/B are identical; use R. Outside values must always be masked with Fill alpha.

Conceptual shader:
```
Opacity = FillAlpha * step(RampR, saturate(Progress));
Color = FillColor;
```
Progress 0 hides all six cells, 0.5 fills the first three, 1 fills all six. Within each cell the advancing edge follows its slant. In a material If node use A=Progress, B=RampR, A>B=1, A==B=1, A<B=0; multiply result by Fill alpha. Ramp has 8-bit quantization; a smoothed comparison may be used for softer transitions.

For UI materials use the computed opacity with the UI material's opacity input, and the chosen tint as Final Color. Source frame remains a separate underlying image. Tested with offline image previews at 0, 0.5, and 1; not imported or tested in Unreal.

Mask sRGB reference: https://dev.epicgames.com/documentation/en-us/unreal-engine/using-texture-masks?application_version=4.27
