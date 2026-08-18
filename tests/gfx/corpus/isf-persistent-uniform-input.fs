/*{
  "DESCRIPTION": "Tests a SINGLE-pass PERSISTENT ISF that consumes a camera UBO. Because the pass is persistent, ISFNode::createRenderer routes this to RenderedISFNode (not SimpleRenderedISFNode) — making this the minimal reproduction of the original bug where uniform_input cables never reached Rendered's pass SRBs. A feedback trail fades each frame (tests texture ping-pong in Rendered), while the newly-added color comes from camera.cameraPosition.x (tests uniform_input binding on both the main and alt SRB of the persistent pass). Wire: CameraUBOBuilder → this.camera → Window. Expected: visible feedback trail whose colour shifts as Eye X changes. If broken: trail is grey (camera never read) or GPU validation fails on the alt SRB (bindUpstreamBuffers missed the alt chain).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BINDING", "TEST-PERSISTENT"],
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
    { "TARGET": "feedback", "PERSISTENT": true, "FLOAT": true }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    // Fade previous frame's content from the persistent target.
    vec4 prev = IMG_NORM_PIXEL(feedback, uv) * 0.96;

    // Moving dot whose colour is driven by the camera UBO — this is the
    // signal that confirms the upstream uniform buffer is actually bound
    // to the persistent pass's SRB (and its alt SRB after the texture
    // ping-pong swap).
    float r = fract(camera.cameraPosition.x * 0.15 + 0.5);
    float g = fract(camera.cameraPosition.y * 0.15 + 0.5);
    vec2 dotPos = vec2(0.5) + 0.35 * vec2(cos(TIME * 1.5), sin(TIME * 1.5));
    float d = length(uv - dotPos);
    float mask = smoothstep(0.06, 0.0, d);

    isf_FragColor = prev + vec4(r * mask, g * mask, 0.2 * mask, mask);
}
