/*{
  "DESCRIPTION": "Tests ISF three-pass pipeline. Pass 0: generates a red gradient into buf0. Pass 1: reads buf0, applies green tint, writes to buf1. Pass 2: reads both buf0 and buf1, composites them. Tests that PASSINDEX increments correctly and intermediate targets are readable. If any pass fails, the output will be missing a color channel.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MULTIPASS"],
  "INPUTS": [],
  "PASSES": [
    { "TARGET": "buf0" },
    { "TARGET": "buf1" },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        // Red horizontal gradient
        gl_FragColor = vec4(uv.x, 0.0, 0.0, 1.0);
    }
    else if(PASSINDEX == 1)
    {
        // Read pass 0, add green vertical gradient
        vec4 p0 = IMG_NORM_PIXEL(buf0, uv);
        gl_FragColor = vec4(p0.r * 0.5, uv.y, 0.0, 1.0);
    }
    else
    {
        // Composite: red from buf0, green from buf1, blue from UV diagonal
        vec4 p0 = IMG_NORM_PIXEL(buf0, uv);
        vec4 p1 = IMG_NORM_PIXEL(buf1, uv);
        gl_FragColor = vec4(p0.r, p1.g, (uv.x + uv.y) * 0.5, 1.0);
    }
}
