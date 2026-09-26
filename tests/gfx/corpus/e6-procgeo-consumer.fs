/*{
  "DESCRIPTION": "Procedural raw raster (no VERTEX_INPUTS, one fullscreen triangle) reading the read-only storage image e6img and the auxiliary buffer e6buf that the upstream CSF e6-procgeo-producer.cs publishes on the geometry it cables in. Left half: the image texel; right half: e6buf.data[0]. Used by GfxE6ProceduralGeometryAdopt.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "e6img", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" }
  ],
  "AUXILIARY": [
    { "NAME": "e6buf", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec4 im = imageLoad(e6img, p);
    vec4 b = e6buf.data[0];
    isf_FragColor = p.x < 32 ? vec4(im.rgb, 1.0) : vec4(b.rgb, 1.0);
}
