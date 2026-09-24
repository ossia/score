/*{
  "DESCRIPTION": "A geometry input declares a flexible-array storage AUXILIARY (meta, Meta[] of 80-byte elements) that nothing provides. Green when the fallback holds at least one element (meta.entries.length() >= 1), red otherwise.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "TYPES": [
    { "NAME": "Meta", "LAYOUT": [ { "NAME": "model", "TYPE": "mat4" }, { "NAME": "tint", "TYPE": "vec4" } ] }
  ],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "foo", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false } ],
      "AUXILIARY": [ { "NAME": "meta", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "entries", "TYPE": "Meta[]" } ] } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
    bool ok = meta.entries.length() >= 1;
    IMG_STORE(outputImage, p, ok ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1));
}
