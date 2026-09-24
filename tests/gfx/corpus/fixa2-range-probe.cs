/*{
  "DESCRIPTION": "Reads three GPU buffers published as sub-ranges of larger allocations and checks both length() and the first element of each. R: geometry attribute position (3 elements). G: geometry AUXILIARY extra (5 floats, 42..46). B: storage input vals (7 vec4, x = 100..106). A channel is 1 when the shader sees exactly the published range.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" },
    { "NAME": "vals", "TYPE": "storage", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "values", "TYPE": "vec4[]" } ] },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" } ],
      "AUXILIARY": [ { "NAME": "extra", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "data", "TYPE": "float[]" } ] } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
    bool attr = ISF_READ(geoIn, position).length() == 3
             && ISF_READ(geoIn, position)[0].x == 1.0
             && ISF_READ(geoIn, position)[2].x == 3.0;
    bool aux = extra.data.length() == 5 && extra.data[0] == 42.0 && extra.data[4] == 46.0;
    bool sto = vals.values.length() == 7 && vals.values[0].x == 100.0 && vals.values[6].x == 106.0;
    IMG_STORE(outputImage, p, vec4(attr ? 1.0 : 0.0, aux ? 1.0 : 0.0, sto ? 1.0 : 0.0, 1.0));
}
