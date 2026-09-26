/*{
  "DESCRIPTION": "Publishes, on its output geometry, a 64x64 write-only storage image e6img (green on every texel) and an auxiliary storage buffer e6buf whose first element is blue. Consumed by e6-procgeo-consumer.fs, a procedural raw raster. Used by GfxE6ProceduralGeometryAdopt.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "e6img", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8" }
  ],
  "RESOURCES": [
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" }
      ],
      "AUXILIARY": [
        { "NAME": "e6buf", "ACCESS": "read_write", "SIZE": "4", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "e6img" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= imageSize(e6img).x || pos.y >= imageSize(e6img).y)
        return;
    imageStore(e6img, pos, vec4(0.0, 1.0, 0.0, 1.0));
    if(pos.y == 0 && pos.x < 3)
    {
        vec2 p = vec2(-1.0, -1.0);
        if(pos.x == 1) p = vec2(3.0, -1.0);
        if(pos.x == 2) p = vec2(-1.0, 3.0);
        geoOut_position_out[pos.x] = vec4(p, 0.0, 1.0);
    }
    if(pos.y == 0 && pos.x < 4)
        geoOut_e6buf.data[pos.x] = vec4(0.0, 0.0, 1.0, 1.0);
}
