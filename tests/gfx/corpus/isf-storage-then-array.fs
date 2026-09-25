/*{
  "DESCRIPTION": "An ISF whose first INPUTS entry is a write-only storage buffer, which creates an output port and no input port, followed by an IS_ARRAY image input declaring WRAP repeat. Samples layer 0 at u = 1.25: the declared repeat sampler reads u = 0.25, a clamping one the right edge.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-SAMPLER"],
  "INPUTS": [
    { "NAME": "scratch", "TYPE": "storage", "ACCESS": "write_only",
      "LAYOUT": [ { "NAME": "hits", "TYPE": "uint" } ] },
    { "NAME": "src", "TYPE": "image", "IS_ARRAY": true, "WRAP": "repeat" }
  ]
}*/
void main()
{
    if(gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0)
        scratch.hits = 1u;
    gl_FragColor = texture(src, vec3(1.25, 0.5, 0.0));
}
