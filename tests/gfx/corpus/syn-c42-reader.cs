/*{
  "DESCRIPTION": "C42 consumer: reads the producer's 'packet' attribute as Packet, the producer's own TYPES struct, and paints a 16x16 image with its value: green.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY"],
  "TYPES": [
    { "NAME": "Packet", "LAYOUT": [ { "NAME": "value", "TYPE": "vec4" }, { "NAME": "tag", "TYPE": "float" } ] }
  ],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "16", "HEIGHT": "16" },
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "packet", "SEMANTIC": "packet", "TYPE": "Packet", "ACCESS": "read_only", "REQUIRED": false }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/

void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  if(p.x >= 16 || p.y >= 16)
    return;
  Packet k = ISF_READ(geoIn, packet)[0];
  IMG_STORE(outputImage, p, vec4(k.value.rgb, 1.0));
}
