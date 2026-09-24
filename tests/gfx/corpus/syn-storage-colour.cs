/*{
  "DESCRIPTION": "Writes one known colour into a storage RESOURCE, whose Types::Buffer output feeds rr-storage-input.fs. Wire: this -> rr-storage-input.fs -> Window.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-STORAGE"],
  "RESOURCES": [
    {
      "NAME": "buf",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [
        { "NAME": "colour", "TYPE": "vec4" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER", "TARGET": "buf" } }
  ]
}*/

void main()
{
    if(gl_GlobalInvocationID.x == 0u)
        buf.colour = vec4(0.0, 1.0, 0.0, 1.0);
}
