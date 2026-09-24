/*{
  "DESCRIPTION": "Procedural fullscreen raw raster painting the colour it reads from an INPUTS storage_input, which is a Types::Buffer port rather than a geometry auxiliary. Wire: syn-storage-colour.cs -> this -> Window. Green when the upstream buffer is bound; black when the placeholder is.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-STORAGE", "TEST-BINDING"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": [
    { "NAME": "src", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "colour", "TYPE": "vec4" }
      ]
    }
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
    isf_FragColor = vec4(src.colour.rgb, 1.0);
}
