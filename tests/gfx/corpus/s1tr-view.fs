/*{
  "DESCRIPTION": "S1 TRANSPARENCY fixture: left half the stored premultiplied rgb (opaque), right half the input's depth as grey.",
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image", "DEPTH": true } ]
}*/
void main()
{
  vec2 uv = isf_FragNormCoord;
  if(uv.x < 0.5)
  {
    gl_FragColor = vec4(IMG_NORM_PIXEL_PREMULTIPLIED(inputImage, vec2(uv.x * 2.0, uv.y)).rgb, 1.0);
  }
  else
  {
    float d = IMG_DEPTH_NORM_PIXEL(inputImage, vec2((uv.x - 0.5) * 2.0, uv.y));
    gl_FragColor = vec4(d, d, d, 1.0);
  }
}
