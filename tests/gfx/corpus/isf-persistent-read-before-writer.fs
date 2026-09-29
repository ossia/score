/*{
  "DESCRIPTION": "Passes before a PERSISTENT pass see what it rendered in the previous frame. Pass 0 copies pass 2, pass 1 writes a constant 128/255 blue, pass 2 adds 16/255 to its own previous red, the last pass shows pass 2 in red, pass 0 in green and pass 1 in blue. After k rendered frames: (k*16, (k-1)*16, 128).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "PASSES": [
    { "TARGET": "early" },
    { "TARGET": "spacer" },
    { "TARGET": "acc", "PERSISTENT": true },
    {}
  ]
}*/

void main()
{
    if(PASSINDEX == 0)
    {
        gl_FragColor = vec4(IMG_NORM_PIXEL(acc, isf_FragNormCoord).r, 0.0, 0.0, 1.0);
    }
    else if(PASSINDEX == 1)
    {
        gl_FragColor = vec4(0.0, 0.0, 128.0 / 255.0, 1.0);
    }
    else if(PASSINDEX == 2)
    {
        float prev = IMG_NORM_PIXEL(acc, isf_FragNormCoord).r;
        gl_FragColor = vec4(min(prev + 16.0 / 255.0, 1.0), 0.0, 0.0, 1.0);
    }
    else
    {
        gl_FragColor = vec4(
            IMG_NORM_PIXEL(acc, isf_FragNormCoord).r,
            IMG_NORM_PIXEL(early, isf_FragNormCoord).r,
            IMG_NORM_PIXEL(spacer, isf_FragNormCoord).b,
            1.0);
    }
}
