/*{
  "DESCRIPTION": "Tests ISF persistent buffer feedback. Pass 0 writes into a PERSISTENT buffer: accumulates a moving dot trail with decay. Pass 1 outputs the persistent buffer. If persistence works, you see a fading trail following the dot. If broken, you see just a single dot with no trail. Also tests FLOAT storage (16-bit buffer) and NEAREST filter.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "INPUTS": [
    { "NAME": "decay", "TYPE": "float", "DEFAULT": 0.95, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "dotSize", "TYPE": "float", "DEFAULT": 0.05, "MIN": 0.01, "MAX": 0.2 }
  ],
  "PASSES": [
    { "TARGET": "trail", "PERSISTENT": true, "FLOAT": true },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        // Read previous persistent buffer and decay
        vec4 prev = IMG_NORM_PIXEL(trail, uv) * decay;

        // Moving dot: circular path
        vec2 dotPos = vec2(0.5) + 0.3 * vec2(cos(TIME * 2.0), sin(TIME * 2.0));
        float d = length(uv - dotPos);
        float mask = smoothstep(dotSize, 0.0, d);

        gl_FragColor = prev + vec4(mask, mask * 0.5, 0.0, mask);
    }
    else
    {
        // Pass 1: output the persistent buffer
        gl_FragColor = IMG_NORM_PIXEL(trail, uv);
    }
}
