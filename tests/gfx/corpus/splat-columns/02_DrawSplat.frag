/*{
  "DESCRIPTION": "3DGS classic draw stage — instanced quad per primitive, EWA-projected to screen as the reference rasteriser does (Kerbl 2023): kernelSize adds that many px^2 to the 2D covariance (0.3 = the vanilla 3DGS dilation); 'antialiased' additionally scales opacity by the Mip-Splatting 2D filter's determinant ratio, which is right only for scenes trained with it. LAYER: the sorted splats accumulate with premultiplied 'over' into two private targets, depth-tested read-only against the consumer's depth: splatColor (rgba16f) and zsum (r32f, the alpha-weighted window depth). The shader's resolve pass composites splatColor over the consumer and writes the expected depth zsum / alpha into its depth where the accumulated alpha reaches coverageThreshold (0.05, as gsplat's expected depth, which covers every pixel a splat reaches), so depth effects downstream see the splats. Reads pos/color/scale/rotation as per-instance vertex inputs from the upstream Decode CSF (PointsToSprites pattern: VERTEX_COUNT=6, INSTANCE_COUNT=N, attrs RATE=instance), emits 6 vertices per instance using a hardcoded quad-corner LUT. The `camera` UBO is auto-resolved from the upstream geometry's `camera` auxiliary — 01_Decode forwards it via COPY_FROM from its own geoIn (which ScenePreprocessor populated). No separate camera cable.",
  "CREDIT": "ossia score — port of GaussianSplatNode render shaders",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["3D", "Splat"],

  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "pos",      "SEMANTIC": "position" },
    { "TYPE": "vec4", "NAME": "color",    "SEMANTIC": "color"    },
    { "TYPE": "vec3", "NAME": "scale",    "SEMANTIC": "scale"    },
    { "TYPE": "vec4", "NAME": "rotation", "SEMANTIC": "rotation" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec2", "NAME": "v_center" },
    { "TYPE": "vec4", "NAME": "v_color"  },
    { "TYPE": "vec3", "NAME": "v_conic"  }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec2", "NAME": "v_center" },
    { "TYPE": "vec4", "NAME": "v_color"  },
    { "TYPE": "vec3", "NAME": "v_conic"  }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "splatColor" },
    { "TYPE": "vec4", "NAME": "zsum" }
  ],

  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "vertex+fragment",
      "LAYOUT": [
        { "NAME": "view",           "TYPE": "mat4" },
        { "NAME": "projection",     "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" },
        { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize",     "TYPE": "vec4" },
        { "NAME": "params",         "TYPE": "vec4" }
      ]
    },
    { "NAME": "kernelSize", "TYPE": "float", "DEFAULT": 0.3, "MIN": 0.0, "MAX": 2.0, "LABEL": "2D dilation (px², 0.3 = 3DGS)" },
    { "NAME": "antialiased", "TYPE": "bool", "DEFAULT": false, "LABEL": "Mip-Splatting antialiasing (scenes trained with it)" },
    { "NAME": "coverageThreshold", "TYPE": "float", "DEFAULT": 0.05, "MIN": 0.001, "MAX": 1.0, "LABEL": "Depth written above this coverage" }
  ],

  "LAYER": {
    "TARGETS": [
      { "NAME": "splatColor", "FORMAT": "rgba16f", "COMPOSITE": "over" },
      { "NAME": "zsum",       "FORMAT": "r32f",    "COMPOSITE": "over" }
    ],
    "DEPTH_TEST": true,
    "RESOLVE": { "COMPOSITE": "over", "DEPTH_WRITE": true }
  },

  "PIPELINE_STATE": {
    "CULL_MODE":   "none",
    "TOPOLOGY":    "triangles"
  }
}*/

#if defined(ISF_RESOLVE_PASS)
// Once per pixel: the accumulated colour over the consumer, and in the depth
// draw the expected depth sum(z * w) / sum(w) where the coverage is enough;
// ISF_DEPTH_FAR elsewhere loses the consumer's depth test and leaves it as is.
void main()
{
  vec4 c = LAYER_TEXEL(splatColor);
  float z = LAYER_TEXEL(zsum).r;
  isf_FragColor = c;
  gl_FragDepth = c.a >= coverageThreshold ? z / c.a : ISF_DEPTH_FAR;
}
#else
void main()
{
  // Pixel convention. The raw-raster parser textually rewrites gl_FragCoord
  // to isf_FragCoord, which is BOTTOM-LEFT origin on every backend
  // (libisf: vec4(x, RENDERSIZE.y - y, ...) on SPIR-V). v_center is
  // (ndc * 0.5 + 0.5) * viewport from the GL-convention camera, also
  // bottom-left origin, and the conic was built in the same y-up frame, so
  // d is consistent. Pixel centres sit at i + 0.5 in both, which is Kerbl's
  // ndc2Pix ((ndc + 1) * S - 1) / 2 sampled at integer pixels.
  vec2 d = gl_FragCoord.xy - v_center;
  float power = -0.5 * (
      v_conic.x * d.x * d.x +
      2.0 * v_conic.y * d.x * d.y +
      v_conic.z * d.y * d.y);
  if (power > 0.0) discard;

  float gaussian = exp(power);
  float alpha = min(0.99, gaussian * v_color.a);
  if (alpha < 1.0/255.0) discard;

  // Premultiplied (raw raster ALPHA default), composited back to front into
  // both LAYER targets: zsum.r accumulates z * alpha with the same weights as
  // the colour.
  splatColor = vec4(v_color.rgb * alpha, alpha);
  zsum = vec4(gl_FragCoord.z * alpha, 0.0, 0.0, alpha);
}
#endif
