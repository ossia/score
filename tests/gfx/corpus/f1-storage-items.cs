/*{
  "DESCRIPTION": "Publishes a storage RESOURCE `items` of 24 vec4 elements (384 bytes) on its Types::Buffer output. Wire: this -> f1-rr-input-count.fs (INPUTS storage_input `items`).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-STORAGE"],
  "RESOURCES": [
    {
      "NAME": "items",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[24]" } ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [24, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER", "TARGET": "items" } }
  ]
}*/

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i < 24u)
        items.data[i] = vec4(float(i + 1u), 0.0, 0.0, 0.0);
}
