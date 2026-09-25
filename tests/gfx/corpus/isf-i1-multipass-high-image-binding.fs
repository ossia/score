/*{
  "DESCRIPTION": "Two passes: five sampled images and the bufferA pass target ahead of a fragment storage image. Numbered after the samplers, the storage image lands on binding 9, past the 8 image units NVIDIA's OpenGL exposes. Pass 0 fills bufferA with red, pass 1 outputs bufferA. Used by GfxIsfCsfImageBindingI1: the frame is red on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "t0", "TYPE": "image" },
    { "NAME": "t1", "TYPE": "image" },
    { "NAME": "t2", "TYPE": "image" },
    { "NAME": "t3", "TYPE": "image" },
    { "NAME": "t4", "TYPE": "image" },
    { "NAME": "img_a", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" }
  ],
  "PASSES": [
    { "TARGET": "bufferA" },
    {}
  ]
}*/
void main()
{
    vec4 s = texture(t0, vec2(0.5)) + texture(t1, vec2(0.5)) + texture(t2, vec2(0.5))
           + texture(t3, vec2(0.5)) + texture(t4, vec2(0.5)) + imageLoad(img_a, ivec2(0));
    if(PASSINDEX == 0)
        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0) + 0.0 * s;
    else
        gl_FragColor = texture(bufferA, isf_FragNormCoord) + 0.0 * s;
}
