/*{
  "DESCRIPTION": "A geometry input whose AUXILIARY declares a sampled texture with DEPTH:true, then a sampled texture and a write-only output image. A sampled auxiliary is one texture binding; the output image must still be red. Used by GfxCsfAuxDepthBinding.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "foo", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false } ],
      "AUXILIARY": [ { "NAME": "shadow", "TYPE": "texture", "IS_DEPTH": true } ] },
    { "NAME": "after", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "64", "HEIGHT": "64" }
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
    vec4 s = texture(shadow, uv) + texture(after, uv);
    imageStore(outputImage, pos, vec4(1.0, 0.0, 0.0, 1.0) + 0.0 * s);
}
