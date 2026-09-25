/*{
  "DESCRIPTION": "Six fragment storage images: they take bindings 3 to 8, and binding 8 is past the 8 image units NVIDIA's OpenGL exposes. Used by GfxStorageImageUnitsA5: OpenGL warns once, other backends stay silent.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "img_a", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "img_b", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "img_c", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "img_d", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "img_e", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "img_f", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" }
  ]
}*/
void main()
{
    vec4 s = imageLoad(img_a, ivec2(0)) + imageLoad(img_b, ivec2(0)) + imageLoad(img_c, ivec2(0))
           + imageLoad(img_d, ivec2(0)) + imageLoad(img_e, ivec2(0)) + imageLoad(img_f, ivec2(0));
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0) + 0.0 * s;
}
