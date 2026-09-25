/*{
  "DESCRIPTION": "One-pixel black and white checkerboard (GfxMipToggleL4.cpp): every 2x2 block averages to 0.5, so any mip level above 0 is a uniform 0.5 grey.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": []
}*/

void main()
{
    float c = mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0);
    gl_FragColor = vec4(c, c, c, 1.0);
}
