/*{
  "DESCRIPTION": "ISF with two OUTPUTS (the MRT path) writing straight red at alpha 0.5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": [],
  "OUTPUTS": [ { "NAME": "out0" }, { "NAME": "out1" } ]
}*/
void main()
{
    out0 = vec4(1.0, 0.0, 0.0, 0.5);
    out1 = vec4(1.0, 0.0, 0.0, 0.5);
}
