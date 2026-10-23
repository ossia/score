/*{
  "DESCRIPTION": "Raw raster that reads its mesh (VERTEX_INPUTS position) and declares PIPELINE_STATE INSTANCE_COUNT 4 without VERTEX_COUNT. Each instance draws the incoming triangle shrunk into one quadrant (rr-instance-count-mesh.vs), so 4 lit quadrants mean the count multiplied the mesh draw; 1 means it was ignored (shader board R21). Feed it syn-geo-producer.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "INSTANCE_COUNT": 4, "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" }
}*/
void main()
{
    isf_FragColor = vec4(1.0);
}
