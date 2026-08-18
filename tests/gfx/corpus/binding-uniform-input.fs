/*{
  "DESCRIPTION": "Tests Step 1 'uniform_input' ISF type — a UBO sourced from an upstream Buffer port, bound via QRhiShaderResourceBinding::uniformBuffer (std140). Wire: CameraUBOBuilder (emits a 240-byte camera UBO) → this.camera port → Window. Expected: fullscreen color derived from camera.cameraPosition.x component of the UBO — sliding the Eye X control changes the screen red channel. If broken: binding fails, GPU validation error, or color stays at 0 (upstream buffer not bound).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BINDING"],
  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "view",           "TYPE": "mat4" },
        { "NAME": "projection",     "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" },
        { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize",     "TYPE": "vec4" },
        { "NAME": "params",         "TYPE": "vec4" }
      ]
    }
  ]
}*/

void main()
{
    float r = fract(camera.cameraPosition.x * 0.1 + 0.5);
    isf_FragColor = vec4(r, 0.2, 0.6, 1.0);
}
