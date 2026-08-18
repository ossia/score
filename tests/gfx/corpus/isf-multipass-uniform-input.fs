/*{
  "DESCRIPTION": "Tests multi-pass ISF with uniform_input (camera UBO). Pass 0 uses camera.cameraPosition.x to drive red and writes to an intermediate target; pass 1 reads pass 0 and composites with green from camera.cameraPosition.y. Routes through RenderedISFNode (multi-pass), so the camera UBO must be bound on BOTH the main and alt SRB of every inner pass. Wire: CameraUBOBuilder → this.camera → Window. Expected: red gradient shifts with Eye X slider, green with Eye Y. If broken: one channel stays at 0 (upstream UBO not bound in one of the passes) or GPU validation error on pass 1's descriptor set.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MULTIPASS", "TEST-BINDING"],
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
  ],
  "PASSES": [
    { "TARGET": "firstPass" },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        float r = fract(camera.cameraPosition.x * 0.1 + 0.5);
        isf_FragColor = vec4(r, uv.y * 0.3, 0.0, 1.0);
    }
    else
    {
        vec4 p0 = IMG_NORM_PIXEL(firstPass, uv);
        float g = fract(camera.cameraPosition.y * 0.1 + 0.5);
        isf_FragColor = vec4(p0.r, g, uv.x * 0.3, 1.0);
    }
}
