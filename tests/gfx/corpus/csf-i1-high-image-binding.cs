/*{
  "DESCRIPTION": "A Params block and six sampled textures ahead of a write-only output image and a persistent read_write image. Numbered in RESOURCES order, the images land on bindings 9, 10 and 11 (_prev), past the 8 image units NVIDIA's OpenGL exposes, and the shader does not compile there. The output image is red while state accumulates frame to frame. Used by GfxIsfCsfImageBindingI1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "t0", "TYPE": "texture" },
    { "NAME": "t1", "TYPE": "texture" },
    { "NAME": "t2", "TYPE": "texture" },
    { "NAME": "t3", "TYPE": "texture" },
    { "NAME": "t4", "TYPE": "texture" },
    { "NAME": "t5", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "64", "HEIGHT": "64" },
    { "NAME": "state", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "64", "HEIGHT": "64", "PERSISTENT": true }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 64 || pos.y >= 64)
        return;
    vec2 uv = (vec2(pos) + 0.5) / 64.0;
    vec4 s = texture(t0, uv) + texture(t1, uv) + texture(t2, uv)
           + texture(t3, uv) + texture(t4, uv) + texture(t5, uv);
    vec4 prev = imageLoad(state_prev, pos);
    imageStore(state, pos, prev + vec4(1.0 / 255.0));
    imageStore(outputImage, pos, vec4(gain, 0.0, 0.0, 1.0) + 0.0 * s + 0.0 * prev);
}
