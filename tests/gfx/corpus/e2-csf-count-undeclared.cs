/*{
  "DESCRIPTION": "csf-a5-aux-consumer.cs with the `copy` SIZE misspelt as $COUNT_itemz, a symbol nothing declares. Writes red = items.length() / 255 and green = copy.length() / 255 into a 4x4 image, so both sizes can be read back. Used by GfxCsfPendingAuxCountE2.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [],
      "AUXILIARY": [
        { "NAME": "items", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] },
        { "NAME": "copy", "ACCESS": "read_write", "SIZE": "$COUNT_itemz", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] }
      ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "4", "HEIGHT": "4" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [4, 4, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 4 || pos.y >= 4)
        return;
    float n_items = float(geo_items.data.length());
    float n_copy = float(geo_copy.data.length());
    if(pos == ivec2(0))
        geo_copy.data[0] = vec4(n_copy);
    imageStore(outputImage, pos, vec4(n_items / 255.0, n_copy / 255.0, 0.0, 1.0));
}
