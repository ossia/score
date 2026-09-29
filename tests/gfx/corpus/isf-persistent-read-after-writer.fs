/*{
  "DESCRIPTION": "Passes after a PERSISTENT pass see what it rendered in the same frame. Pass 0 adds 16/255 to its own previous red (it reads itself, so it sees the previous frame), pass 1 copies pass 0, the last pass shows pass 0 in red and pass 1 in green. After k rendered frames both are k*16.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "PASSES": [
    { "TARGET": "acc", "PERSISTENT": true },
    { "TARGET": "copy" },
    {}
  ]
}*/

void main()
{
    if(PASSINDEX == 0)
    {
        float prev = IMG_NORM_PIXEL(acc, isf_FragNormCoord).r;
        gl_FragColor = vec4(min(prev + 16.0 / 255.0, 1.0), 0.0, 0.0, 1.0);
    }
    else if(PASSINDEX == 1)
    {
        gl_FragColor = vec4(IMG_NORM_PIXEL(acc, isf_FragNormCoord).r, 0.0, 0.0, 1.0);
    }
    else
    {
        float a = IMG_NORM_PIXEL(acc, isf_FragNormCoord).r;
        float c = IMG_NORM_PIXEL(copy, isf_FragNormCoord).r;
        gl_FragColor = vec4(a, c, 0.0, 1.0);
    }
}
