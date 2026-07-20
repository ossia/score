/*{
  "DESCRIPTION": "Tests NEAREST filter on ISF pass. Pass 0 writes a small 32x32 checkerboard. Pass 1 reads it with NEAREST filter, which should produce hard pixel edges. If NEAREST works, you see sharp blocky pixels. If it falls back to LINEAR, edges are blurry.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-FILTER"],
  "INPUTS": [],
  "PASSES": [
    { "TARGET": "pixelBuf", "WIDTH": 32, "HEIGHT": 32, "FILTER": "NEAREST" },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        // 4x4 checkerboard in 32x32 buffer
        float cx = step(0.5, fract(uv.x * 4.0));
        float cy = step(0.5, fract(uv.y * 4.0));
        float check = abs(cx - cy);
        gl_FragColor = vec4(check, 0.2, 1.0 - check, 1.0);
    }
    else
    {
        gl_FragColor = IMG_NORM_PIXEL(pixelBuf, uv);
    }
}
