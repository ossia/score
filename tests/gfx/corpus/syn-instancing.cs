/*{
  "DESCRIPTION": "Instancing: geometry with VERTEX_COUNT 3 and INSTANCE_COUNT 4, one attribute at RATE vertex and one at RATE instance. The PER_INSTANCE pass marks one slot per instance, so the count reads back as exactly 4 -- a dispatch sized from the vertex count instead would read 3, and one that never ran reads 0.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-INSTANCING"],
  "RESOURCES": [
    {
      "NAME": "tally",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [ { "NAME": "instanceHits", "TYPE": "int[32]" } ]
    },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "INSTANCE_COUNT": "4",
      "ATTRIBUTES": [
        { "NAME": "position",    "SEMANTIC": "position",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "translation", "SEMANTIC": "translation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance" }
      ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [3, 1, 1],  "EXECUTION_MODEL": { "TYPE": "PER_VERTEX",   "TARGET": "geo" } },
    { "LOCAL_SIZE": [4, 1, 1],  "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE", "TARGET": "geo" } },
    { "LOCAL_SIZE": [8, 8, 1],  "EXECUTION_MODEL": { "TYPE": "2D_IMAGE",     "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 3u)
      return;
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(3.0, -1.0);
    if(idx == 2u) p = vec2(-1.0, 3.0);
    geo_position_out[idx] = vec4(p, 0.0, 1.0);
    return;
  }

  if(PASSINDEX == 1)
  {
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 32u)
      return;
    geo_translation_out[idx] = vec4(float(idx), 0.0, 0.0, 0.0);
    tally.instanceHits[idx] = 1;
    return;
  }

  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(outputImage);
  if(pos.x >= size.x || pos.y >= size.y)
    return;

  int total = 0;
  for(int i = 0; i < 32; i++)
    total += tally.instanceHits[i];

  IMG_STORE(outputImage, pos, vec4(float(total) / 255.0, 0.0, 0.0, 1.0));
}
