/*{
  "DESCRIPTION": "Valid ISF header, invalid GLSL body. Parses as a node and then throws when its pipeline is built, which is the shape that used to leak the QRhi frame and wedge the renderer for the rest of the session.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ERROR"],
  "INPUTS": []
}*/

void main()
{
    this is not glsl at all;
}
