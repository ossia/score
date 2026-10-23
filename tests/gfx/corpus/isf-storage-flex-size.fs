/*{
  "DESCRIPTION": "Classic ISF with a read_write storage buffer whose LAYOUT ends in a flexible array (vec4[]), sized by the synthesized `data size` control (default 1024). Paints the runtime length the shader sees: red when it is 1024, blue when it is 10, green when it is 1 (the buffer allocated as one element). Used by GfxIsfStorageFlexSize (shader board U44).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-STORAGE"],
  "INPUTS": [
    { "NAME": "data", "TYPE": "storage", "ACCESS": "read_write", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "v", "TYPE": "vec4[]" } ] }
  ]
}*/
void main()
{
    int n = data.v.length();
    if(gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0)
        data.v[0] = vec4(float(n));
    gl_FragColor = vec4(n == 1024 ? 1.0 : 0.0, n == 1 ? 1.0 : 0.0, n == 10 ? 1.0 : 0.0, 1.0);
}
