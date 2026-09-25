/*{
  "DESCRIPTION": "A single PERSISTENT pass writing straight red at alpha 0.5; the node copies the persistent target to its output.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "PASSES": [ { "TARGET": "fb", "PERSISTENT": true } ]
}*/
void main()
{
    gl_FragColor = vec4(1.0, 0.0, 0.0, 0.5);
}
