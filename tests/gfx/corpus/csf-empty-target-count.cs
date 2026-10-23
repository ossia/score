/*{
  "DESCRIPTION": "1D_BUFFER pass with no TARGET over a storage output of 24 vec4 elements behind a 16-byte header (400 bytes). Each invocation adds 1 to `invocations`: dispatched per element it reads 24, per byte 400 (shader board C26). Used by GfxCsfEmptyTargetCount.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-STORAGE"],
  "RESOURCES": [
    {
      "NAME": "items",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [
        { "NAME": "invocations", "TYPE": "uint" },
        { "NAME": "pad", "TYPE": "uint[3]" },
        { "NAME": "data", "TYPE": "vec4[24]" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER" } }
  ]
}*/

void main()
{
    uint i = gl_GlobalInvocationID.x;
    atomicAdd(items.invocations, 1u);
    if(i < 24u)
        items.data[i] = vec4(float(i + 1u), 0.0, 0.0, 0.0);
}
