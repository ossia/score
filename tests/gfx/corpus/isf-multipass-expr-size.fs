/*{
  "DESCRIPTION": "Tests ISF pass with expression-based WIDTH/HEIGHT. Pass 0 renders into a buffer sized at half the render output (RENDERSIZE.x/2 x RENDERSIZE.y/2). Pass 1 reads it back. If expressions work: output is visibly half-res pixelated. If expressions fail: either full-res or broken.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MULTIPASS"],
  "INPUTS": [],
  "PASSES": [
    { "TARGET": "halfBuf", "WIDTH": "$WIDTH / 2", "HEIGHT": "$HEIGHT / 2" },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        // Concentric rings pattern - easy to see resolution loss
        float d = length(uv - 0.5) * 20.0;
        float rings = 0.5 + 0.5 * sin(d);
        gl_FragColor = vec4(rings, uv.x, uv.y, 1.0);
    }
    else
    {
        gl_FragColor = IMG_NORM_PIXEL(halfBuf, uv);
    }
}
