/*{
  "DESCRIPTION": "Persistent passes each copying the one drawn just before it, and a persistent last pass showing them. b1 adds 16/255 to its own previous red, b2 copies b1, b3 copies b2, all in the same frame. The last pass shows b1, b2, b3 in red, green, blue: after k rendered frames all three are k*16.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "PASSES": [
    { "TARGET": "b1", "PERSISTENT": true },
    { "TARGET": "b2", "PERSISTENT": true },
    { "TARGET": "b3", "PERSISTENT": true },
    { "TARGET": "shown", "PERSISTENT": true }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    if(PASSINDEX == 0)
        gl_FragColor = vec4(min(IMG_NORM_PIXEL(b1, uv).r + 16.0 / 255.0, 1.0), 0.0, 0.0, 1.0);
    else if(PASSINDEX == 1)
        gl_FragColor = vec4(IMG_NORM_PIXEL(b1, uv).r, 0.0, 0.0, 1.0);
    else if(PASSINDEX == 2)
        gl_FragColor = vec4(IMG_NORM_PIXEL(b2, uv).r, 0.0, 0.0, 1.0);
    else
        gl_FragColor = vec4(
            IMG_NORM_PIXEL(b1, uv).r, IMG_NORM_PIXEL(b2, uv).r, IMG_NORM_PIXEL(b3, uv).r, 1.0);
}
