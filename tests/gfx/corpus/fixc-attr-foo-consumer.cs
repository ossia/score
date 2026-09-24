/*{
  "DESCRIPTION": "Reads attribute foo (SEMANTIC custom, so matched by NAME) and paints green when foo[0] == 1, red otherwise.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "custom", "TYPE": "float", "ACCESS": "read_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
    bool ok = ISF_READ(geoIn, foo)[0] == 1.0;
    IMG_STORE(outputImage, p, ok ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1));
}
