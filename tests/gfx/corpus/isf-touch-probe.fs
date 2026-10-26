/*{
  "DESCRIPTION": "Reads a touch history the ISF way, with the raw reads IMG_NORM_PIXEL_PREMULTIPLIED / IMG_PIXEL_PREMULTIPLIED (IMG_NORM_PIXEL returns straight colour, dividing by alpha, and the history keeps time in alpha), in four vertical bands showing a sample as (x * 0.5 + 0.5, y * 0.5 + 0.5, pressure): 0 v = 0 (the newest row), 1 the centre of row 5, 2 the pixel read of row 5, 3 column 1 at v = 0. The same reads as vsa-touch-probe.vs bands 0, 2, 5, 6: a history reads the same in both formats. Used by GfxVsaTouch.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": [ { "LABEL": "Touch", "NAME": "touch", "TYPE": "image" } ]
}*/

vec4 show(vec4 t) { return vec4(t.xy * 0.5 + 0.5, t.z, 1.0); }

void main()
{
  float band = floor(isf_FragNormCoord.x * 4.0);
  vec4 c;
  if(band < 0.5)
    c = IMG_NORM_PIXEL_PREMULTIPLIED(touch, vec2(0.5 / 32., 0.));
  else if(band < 1.5)
    c = IMG_NORM_PIXEL_PREMULTIPLIED(touch, vec2(0.5 / 32., 5.5 / 240.));
  else if(band < 2.5)
    c = IMG_PIXEL_PREMULTIPLIED(touch, vec2(0.5, 5.5));
  else
    c = IMG_NORM_PIXEL_PREMULTIPLIED(touch, vec2(1.5 / 32., 0.));
  gl_FragColor = show(c);
}
