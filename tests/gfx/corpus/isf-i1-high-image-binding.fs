/*{
  "DESCRIPTION": "Six sampled images ahead of two fragment storage images. Numbered after the samplers, the storage images land on bindings 9 and 10, past the 8 image units NVIDIA's OpenGL exposes, and the shader does not compile there. Used by GfxIsfCsfImageBindingI1: the frame is red on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "t0", "TYPE": "image" },
    { "NAME": "t1", "TYPE": "image" },
    { "NAME": "t2", "TYPE": "image" },
    { "NAME": "t3", "TYPE": "image" },
    { "NAME": "t4", "TYPE": "image" },
    { "NAME": "t5", "TYPE": "image" },
    { "NAME": "img_a", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "img_b", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" }
  ]
}*/
void main()
{
    vec4 s = texture(t0, vec2(0.5)) + texture(t1, vec2(0.5)) + texture(t2, vec2(0.5))
           + texture(t3, vec2(0.5)) + texture(t4, vec2(0.5)) + texture(t5, vec2(0.5))
           + imageLoad(img_a, ivec2(0)) + imageLoad(img_b, ivec2(0));
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0) + 0.0 * s;
}
