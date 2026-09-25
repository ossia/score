# Writing shaders for score

Agent-facing reference for the three shader modes. Every example here is a real
file under `tests/gfx/corpus/guide-*`, baked for SPIR-V / GLSL / HLSL / MSL by
`test_gfx_shader_corpus_targets` and pinned by its rendered pixels in
`tests/gfx/GfxShaderGuideExamples.cpp`. **Read those files rather than copying
from this document if the two ever disagree** — they are the ones under test.

---

## 1. Mode detection (order matters)

The parser sniffs the source; you do not tell the engine the mode directly.
`libisf/src/isf.cpp`, `parser::parse()`:

| Order | Mode | Trigger |
|---|---|---|
| 1 | CSF compute | `"DISPATCH"` **and** `"LOCAL_SIZE"`, plus `"RESOURCES"` or an ISF header |
| 2 | Raw raster | the literal string `"RAW_RASTER_PIPELINE"` |
| 3 | Shadertoy / shadertoy-JSON | shadertoy shapes |
| 4 | VSA | sniffed from the **vertex** source, not the fragment |
| 5 | ISF | an ISF header |

- A shader with `LOCAL_SIZE` is compute **even if** it declares
  `RAW_RASTER_PIPELINE`. CSF is tested first.
- VSA is decided by the `.vs`. A `.fs` alone cannot tell you a pair is VSA.

## 2. Pick your data path before writing anything

| | Geometry path | Scene path |
|---|---|---|
| Upstream | primitive, CSF producer, particle system | anything through a **Scene Preprocessor** |
| Placement | `MODEL_MATRIX` | `per_draws.data[draw_id].model` |
| `MODEL_MATRIX` is | the transform | **identity, always** |
| Camera | identity stand-in unless wired | real camera, or a synthesised default |
| Lights / materials / IBL / shadows | unavailable | named auxiliaries |

A scene `Transform3D` is baked into `per_draws[draw_id].model`. It never reaches
`MODEL_MATRIX`, which is written only by a `transform3d` message on the raster
node's own port. **A scene-path shader reading `MODEL_MATRIX` pins everything at
the origin, silently.**

Only `ScenePreprocessorNode` and `SceneFilterNode` consume a `Types::Scene` port.
Every rasteriser takes `Types::Geometry`. The Preprocessor is the only bridge —
it is not an optimisation.

## 3. ISF — fragment effects

Default mode. One fragment shader, no geometry, full frame.

```glsl
/*{
  "DESCRIPTION": "Guide example 1 -- a minimal ISF fragment shader. Paints a horizontal ramp in the red channel, scales it by a float INPUT, and keeps green at a constant so a reader can tell 'the control did nothing' from 'the shader did not run'. isf_FragNormCoord is bottom-left origin, which is the ISF convention.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "CATEGORIES": ["GUIDE"],
  "INPUTS": [
    { "NAME": "intensity", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    isf_FragColor = vec4(uv.x * intensity, 0.5, 0.0, 1.0);
}
```

Free built-ins: `isf_FragNormCoord` (bottom-left origin), `isf_FragColor`,
`RENDERSIZE`, `TIME`, `TIMEDELTA`, `PROGRESS`, `FRAMEINDEX`, `DATE`,
`PASSINDEX`, `MSAA_SAMPLES`.

Image access — always via accessors, never a raw `texture()`:
`IMG_THIS_PIXEL`, `IMG_THIS_NORM_PIXEL`, `IMG_PIXEL`, `IMG_NORM_PIXEL`,
`IMG_SIZE`, `TEX_DIMENSIONS`, `IMG_THIS_DEPTH`, `IMG_DEPTH_PIXEL`,
`IMG_DEPTH_NORM_PIXEL`, `IMG_CUBE`, `IMG_CUBE_DEPTH`, `IMG_LOAD`, `IMG_STORE`,
`IMG_STORE_CUBE`, `IMG_STORE_LAYER`, `IMG_SIZE_CUBE`.

`INPUTS` types: `float` `long` `bool` `color` `point2d` `point3d` `event`
`image` `cubemap` `audio` `audiofft` `audiofloathistogram` `texture` `storage`
`uniform` `geometry`.

Multi-pass goes in `PASSES`; `"PERSISTENT": true` keeps a pass target across
frames (feedback, accumulation).

## 4. Raw raster — drawing geometry

Vertex + fragment pair, `"MODE": "RAW_RASTER_PIPELINE"`. **The header lives in
the `.fs`.** The `.vs` is bare GLSL: redeclaring an input the header already
declares is a redefinition compile error — the parser emits them.

### Geometry path

```glsl
/*{
  "DESCRIPTION": "Guide example 2 -- raw raster on the GEOMETRY path. Draws upstream geometry placed by MODEL_MATRIX and viewed through VIEWPROJECTION_MATRIX. On the geometry path an unresolved camera binds an identity stand-in, so the camera term costs nothing until a Camera exists; on a scene chain it binds the real one. Shades by the interpolated vertex colour so a pixel oracle measures placement, not lighting.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["GUIDE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = v_color;
}
```

```glsl
void main()
{
    // Always first: the engine's vertex prologue.
    isf_vertShaderInit();

    gl_Position = clipSpaceCorrMatrix
                * VIEWPROJECTION_MATRIX
                * MODEL_MATRIX
                * vec4(position.xyz, 1.0);
    v_color = color;

    // Always last: owns the one clip-space Y negation on Metal and D3D.
    isf_vertShaderFinish();
}
```

### Scene path

```glsl
/*{
  "DESCRIPTION": "Guide example 3 -- raw raster on the SCENE path. Placement comes from per_draws.data[draw_id].model, which is where a scene Transform3D is baked; MODEL_MATRIX is identity here by construction, so reading it instead would pin every object at the origin. draw_id arrives through the instance_draw_id semantic. The camera comes from the built-in, which indexes the camera block through VIEW_INDEX and is therefore correct under MULTIVIEW too.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["GUIDE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] }
  ],
  "TYPES": [
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model",           "TYPE": "mat4" },
        { "NAME": "normal",          "TYPE": "mat4" },
        { "NAME": "material_index",  "TYPE": "uint" },
        { "NAME": "tag_hash",        "TYPE": "uint" },
        { "NAME": "transform_slot",  "TYPE": "uint" },
        { "NAME": "skeleton_offset", "TYPE": "uint" }
    ] }
  ]
}*/

void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
```

```glsl
void main()
{
    isf_vertShaderInit();

    mat4 model = per_draws.data[draw_id].model;
    gl_Position = clipSpaceCorrMatrix
                * VIEWPROJECTION_MATRIX
                * model
                * vec4(position, 1.0);

    isf_vertShaderFinish();
}
```

### Camera built-ins

`VIEW_MATRIX`, `PROJECTION_MATRIX`, `VIEWPROJECTION_MATRIX`, `CAMERA_POSITION`.

They index the camera block through `VIEW_INDEX`, so they are correct under
`MULTIVIEW` unchanged — face *i* reads camera *i*. A scalar accessor resolving to
camera 0 would paint all six cubemap faces identically.

Declaring your own `camera` input suppresses the synthesised one; do that only
when you need a layout the built-in does not provide.

### PIPELINE_STATE

`CULL_MODE`, `FRONT_FACE`, `DEPTH_TEST`, `DEPTH_WRITE`, `DEPTH_COMPARE`,
`DEPTH_BIAS`, `BLEND`, `BLEND_PER_ATTACHMENT`, `COLOR_WRITE`, `TOPOLOGY`,
`POLYGON_MODE`, `LINE_WIDTH`, `STENCIL_*`, `CLIP_DISTANCES`, `CULL_DISTANCES`,
`SHADING_RATE`. Without `BLEND` the output is composited according to the
shader's `ALPHA` key (section 11).

`VERTEX_COUNT` / `INSTANCE_COUNT` are procedural overrides: they issue a bare
`draw(N, M)` and ignore the incoming index and indirect buffers. That is how
fullscreen passes (`VERTEX_COUNT: 3`) and procedural geometry are written.

## 5. CSF — compute

`"MODE": "COMPUTE_SHADER"`, with `RESOURCES` (what the pass owns) and `PASSES`
(how it dispatches).

### Writing an image

```glsl
/*{
  "DESCRIPTION": "Guide example 4 -- a compute shader writing a storage image. Note the two conventions that differ from ISF: a storage-image texel index is TOP-DOWN (row 0 is the top of the delivered image, the opposite of isf_FragNormCoord), and every write goes through IMG_STORE rather than imageStore, so the same source bakes on every backend. The bounds guard is mandatory: the dispatch is rounded up to whole workgroups, so the last one runs past the image.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["GUIDE"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos  = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;

    float u = float(pos.x) / float(size.x - 1);
    IMG_STORE(outputImage, pos, vec4(u, 0.25, 1.0 - u, 1.0));
}
```

### Producing geometry for a rasteriser

An attribute declared `read_write` becomes **two** symbols, `<name>_in` and
`<name>_out`.

```glsl
/*{
  "DESCRIPTION": "Guide example 5 -- a compute shader that PRODUCES geometry for a rasteriser downstream. A geometry RESOURCE declares its attributes by SEMANTIC; the parser then exposes each one as <name>_out for writing and <name>_in for reading, so a read_write attribute is two symbols, not one. EXECUTION_MODEL PER_VERTEX dispatches one invocation per vertex of TARGET. Pair with guide-rawraster-geo, whose VERTEX_INPUTS match these semantics.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["GUIDE"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= uint(geo_position_out.length()))
        return;

    // A triangle in the z=0 plane, wound counter-clockwise.
    vec2 corners[3] = vec2[3](
        vec2(-0.6, -0.5),
        vec2( 0.6, -0.5),
        vec2( 0.0,  0.6)
    );
    vec4 colors[3] = vec4[3](
        vec4(1.0, 0.0, 0.0, 1.0),
        vec4(0.0, 1.0, 0.0, 1.0),
        vec4(0.0, 0.0, 1.0, 1.0)
    );

    geo_position_out[idx] = vec4(corners[idx], 0.5, 1.0);
    geo_color_out[idx]    = colors[idx];
}
```

### EXECUTION_MODEL

| TYPE | Dispatches |
|---|---|
| `2D_IMAGE` | one invocation per texel of the target image |
| `1D_BUFFER` | one per element of the target buffer |
| `PER_VERTEX` | one per vertex of the target geometry |
| `PER_INSTANCE` | one per instance |
| `MANUAL` | the size given in `WORKGROUPS` |
| `INDIRECT` | from a GPU buffer, with a mandatory worst-case ceiling |
| `USER` | an expression, may reference `$USER` |

`STRIDE_X/Y/Z` make one invocation handle several elements. `PER_LAYER`,
`PER_CUBE_FACE`, `PER_MIP` fan a pass over a layered target.

## 6. Hard rules

1. **Every vertex `main` opens with `isf_vertShaderInit()` and closes with
   `isf_vertShaderFinish()`.** The epilogue owns the single clip-space Y negation
   applied on Metal and D3D. Miss it → mirrored against every other backend, with
   inverted face culling. Never hand-roll the negation; running it twice cancels it.
2. **Depth is reverse-Z: `GREATER` compare, clear 0.0, float D32F.** A triangle at
   `z = 0.0` is rejected. Fullscreen probes go at `z = 0.5`.
3. **Never declare a bare `PI`, `EPSILON`, `TAU` or `E`** in GLSL that gets
   concatenated with user text. A score's `#define PI 3.1415926535` once rewrote
   `const float PI` into `const float 3.1415926535 = ...` and killed the stage.
4. **`IMG_STORE`, not `imageStore`. `isf_FragColor`, not `gl_FragColor`.**
   `gl_FragCoord` is textually rewritten to `isf_FragCoord` — do not define that
   name yourself.
5. **Guard bounds in compute.** Dispatches round up to whole workgroups; the last
   one runs past the end. Compare against `imageSize()` / `.length()`.

## 7. Traps

- **The default scene camera is not identity.** With no Camera node the flattener
  synthesises one at eye `(0,1,3)` looking at the origin. Adding a view term to a
  scene-path shader that lacked one therefore *re-frames* it. On the geometry path
  the same edit is a no-op.
- **Auxiliaries resolve by name against `meshes[0]` only.** Multi-object work is
  the scene path's job (`per_draws` + `indirect_draw_cmds`).
- **Several producers on one raster input do not accumulate.** The renderer builds
  a merged scene, but the rasteriser reads a single `geometry` field — first or
  last, never both. Only the Preprocessor and Scene Filter read the merged scene.
- **An unbound auxiliary is a zero-filled placeholder** and shaders read zeros as
  sentinels. `camera` is the one exception: identity-seeded, because an all-zero
  viewProjection collapses every vertex to the origin.
- **The same geometry winds CCW on OpenGL, CW on Vulkan and Metal.** The engine
  compensates so `CULL_MODE` means the same thing everywhere. Do not compensate
  again.
- **Mean and standard deviation are orientation-invariant.** They cannot detect a
  flip or a winding fault. Use a reference comparison.

## 8. Multi-stage pipelines

This is the part that makes non-trivial renderers possible, and it is not
obvious from any single shader: **stages do not wire buffers to each other. They
attach named auxiliaries to the geometry stream, and downstream shaders pick
them up by name.** A compute pass declares an `AUXILIARY` with `ACCESS:
"write_only"`; a later pass declares one with the same `NAME` and
`ACCESS: "read_only"`. The cable between them carries a geometry edge; the
buffers ride along it.

Intermediate stages pass everything they do not touch through unmodified, so a
rasteriser at the end of a four-stage chain still sees the Preprocessor's
original 22 auxiliaries plus everything each stage added.

### Worked example: clustered forward+

Shipping in `packages/csf-examples/presets/`:

```
ScenePreprocessor
  -> lighting/build_cluster_aabbs.csf        writes cluster_aabbs, cluster_config
  -> lighting/cull_lights_into_clusters.csf  writes cluster_light_counts, cluster_light_lists
  -> rasterizers/clustered_lambert.frag      reads all of the above, by name
  -> Window
```

Read those three files before writing your own pipeline; they are the reference.
Points worth lifting from them:

- **One source of truth for shared configuration.** `build_cluster_aabbs` emits a
  `cluster_config` SSBO carrying the grid dimensions rather than every stage
  hardcoding them. The header is explicit that a mismatch across the triplet
  *silently* truncates lights or reads out of bounds.
- **Size auxiliaries with `$USER` expressions.** `"SIZE": "$total_clusters"`
  keeps the allocation tied to a control instead of a magic number.
- **Degrade gracefully.** `clustered_lambert` checks `cluster_config.cluster_x == 0`
  and falls back to the non-clustered loop, so the shader still works standalone
  and can be A/B'd against the simple path. This matters more than it looks: an
  unbound auxiliary is a **zero-filled** placeholder, so `== 0` is exactly the
  signal that the producer was not wired.
- **Cost-aware staging.** The AABB pass depends only on the camera, so it can be
  gated on a dirty flag rather than rerun per frame.

### Multiple render targets

Declare several `FRAGMENT_OUTPUTS` and the node grows one image output port per
attachment — that is how a G-buffer is written. See
`tests/gfx/corpus/raw-raster-mrt.fs`. All colour outputs of one raw-raster
shader share a render pass, so they must agree on size.

## 9. Feedback, and particle systems

A geometry attribute declared `read_write` **persists across frames**. That one
fact is the whole basis of particle simulation here: a pass reads what it wrote
last frame and integrates.

```glsl
/*{
  "DESCRIPTION": "Guide example 6 -- frame-to-frame feedback, the basis of every particle system. A geometry attribute declared read_write persists across frames, so a pass can read what it wrote last frame and integrate. ISF_READ(geo, attr) and ISF_WRITE(geo, attr) are the readable spelling of geo_attr_in / geo_attr_out; a read_write attribute is always two symbols. Alpha doubles as an initialised flag, because the buffer's first-frame contents are not guaranteed. Pair with guide-rawraster-geo to see it: the triangle brightens the longer it runs.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["GUIDE"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= uint(ISF_WRITE(geo, position).length()))
        return;

    vec2 corners[3] = vec2[3](
        vec2(-0.6, -0.5),
        vec2( 0.6, -0.5),
        vec2( 0.0,  0.6)
    );

    vec4 prev = ISF_READ(geo, color)[idx];

    // First frame: the buffer's contents are undefined, so seed rather than
    // integrate. Alpha is the flag -- a seeded vertex always has alpha 1.
    if(prev.a < 0.5)
    {
        ISF_WRITE(geo, position)[idx] = vec4(corners[idx], 0.5, 1.0);
        ISF_WRITE(geo, color)[idx]    = vec4(0.02, 0.0, 0.0, 1.0);
        return;
    }

    // Integrate: accumulate red, saturating well below 1.0 so a longer run is
    // always distinguishable from a shorter one.
    ISF_WRITE(geo, position)[idx] = vec4(corners[idx], 0.5, 1.0);
    ISF_WRITE(geo, color)[idx]    = vec4(min(prev.r + 0.03, 0.95), 0.0, 0.0, 1.0);
}
```

Two conventions this example exists to show:

- `ISF_READ(geo, attr)` and `ISF_WRITE(geo, attr)` expand to `geo_attr_in` and
  `geo_attr_out`. A `read_write` attribute is **two symbols**, never one; some
  contexts accept no shorthand at all.
- **Seed explicitly.** First-frame buffer contents are not guaranteed, so carry an
  initialised flag (alpha, here) rather than assuming zeros.

For the ISF equivalent, a pass target marked `"PERSISTENT": true` survives
between frames — `shaderlib/simulations/GameOfLife.fs` and `ReactionDiffusion.fs`
are the reference. For a persistent **SSBO** in compute, put `"PERSISTENT": true`
on the resource: the runtime then owns a ping-pong pair, where `<name>` is this
frame's writable buffer and `<name>_prev` is last frame's read-only snapshot.

A real particle chain composes these as separate nodes:

```
emitter (CSF, writes position/velocity)
  -> forces/CurlNoiseForce.cs      accumulates into velocity
  -> forces/Integrate.cs           position += velocity * dt
  -> renderers/SphereSplat.vs/.fs  draws them
```

Each force stage takes `velocity` as `read_write` and `position` as `read_only`,
so they compose in any order; `Integrate` goes last. That decomposition is why
`shaderlib/forces/` has seven small files instead of one monolith.

## 10. Point clouds and splats

Point clouds are not a parallel universe — they deliberately reuse the mesh
contract. Two component types exist:

- **`point_cloud_component`** — the simple case: parallel `positions` / `colors`
  / `normals` / `intensities` buffers plus `point_count`.
- **`primitive_cloud_component`** — the general design, built for 3D Gaussian
  splats. It keeps the payload **opaque**: a `raw_data` buffer of verbatim
  per-row bytes (a `.ply` with its header stripped, or post-decode bytes), a
  `row_stride`, and a `format_id` such as `"3dgs.classic"`. `extra_buffers`
  carries what one array cannot (a quantised SH codebook plus per-primitive
  indices). `struct_type_name` — e.g. `"Splat3DGS"` — exposes those same bytes
  as a typed per-vertex attribute instead of a raw block.

### What the Scene Preprocessor emits

Clouds are **bucketed by `format_id`**, so every cloud of one format batches
into a single draw. Each bucket puts four things on the geometry stream:

| Auxiliary | Contents |
|---|---|
| `raw_splats` | the bucket's `raw_data`, concatenated |
| `cloud_meta` | `CloudMetaGPU[]`, one per cloud |
| `cloud_id_lookup` | one `uint` per primitive → its `cloud_meta` index |
| indirect cmd | `{total_primitives, 1, 0, 0, 0}` |

`CloudMetaGPU` is 128 bytes and **mirrors `PerDrawGPU` on purpose**:

```
float    model[16]              // 64 — per-cloud TRS
float    bounds_min[4]          // 80 — world AABB, xyz + pad
float    bounds_max[4]          // 96
uint32_t primitive_offset       // 100
uint32_t primitive_count        // 104
uint32_t transform_slot         // 108 — 0xFFFFFFFF = none
uint32_t format_param_index     // 112
uint32_t _pad[4]                // 128
```

So a splat CSF reads per-cloud TRS exactly the way a mesh shader reads
`per_draws[gl_DrawID]`. `bounds_min/max` are the world AABB obtained by walking
the eight corners of the local bounds through the world transform, for per-cloud
frustum culling.

Two things to know if you touch this path: the buffers are `growBuf`-managed so
downstream SRBs see **pointer-stable handles**, and each bucket carries a
`content_fingerprint` over raw-data identity, primitive count, world transform
and slot — a match skips the CPU concat and the upload entirely.

### The consuming pipeline

From `2026/splats-room.score`, transcribed in `tests/gfx/GfxSplatRender.cpp`:

```
AssetLoader (room.ply) -> scene_group -> Scene Preprocessor
  -> Flattened Scene Filter     Mode 12, Format ID "3dgs.classic"
  -> CSF "01_Decode"            reads $VERTEX_COUNT_geoIn = N,
                                emits the instanced 6xN quad topology
  -> Render Pipeline "02_DrawSplat"
```

`FlattenedSceneFilterNode` mode `12` is *format_id equals match_str*, mode `13`
is *differs* — that is how only the matching clouds are routed into a
format-specific chain. `$VERTEX_COUNT_<name>` is a real CSF expression
(`RenderedCSFNode.cpp`), so a decode stage sizes its dispatch from the incoming
geometry rather than a constant.

`2026/test-3dgs-full.score` is the same head with an eight-stage chain: Decode,
TileEmit, RadixHistogram, RadixScan, RadixScatter, TileRanges, TileRender,
Composite.

### Do not use Threedim/Splat/

The legacy `Threedim/Splat/` process (`GaussianSplatNode`, with its own depth
sort, radix passes and `splatCount`) is used by **zero** of the 263 corpus
documents; `GfxSplatRender.cpp` documents the greps that establish this. The
live path is AssetLoader → primitive cloud → Preprocessor → format filter → CSF
chain.

## 11. Alpha: premultiplied targets and the `ALPHA` key

**Every texture that travels between nodes holds premultiplied colour**:
`rgb` is already multiplied by `a`. A cable into an image input is drawn into
that input's render target, cleared to `(0, 0, 0, 0)` (the final output clears to
opaque black), and composited **over** what the target already holds. The
engine picks the blend from what the shader says it writes:

| `ALPHA` | the shader writes | colour factors | alpha factors |
|---|---|---|---|
| `"straight"` | `vec4(rgb, a)` | `SrcAlpha`, `OneMinusSrcAlpha` | `One`, `OneMinusSrcAlpha` |
| `"premultiplied"` | `vec4(rgb * a, a)` | `One`, `OneMinusSrcAlpha` | `One`, `OneMinusSrcAlpha` |

Both store premultiplied colour, so writing `(1, 0, 0, 0.5)` straight and
`(0.5, 0, 0, 0.5)` premultiplied leave the same texel `(0.5, 0, 0, 0.5)`.

- Defaults: **ISF straight**; **CSF, raw raster and VSA premultiplied**.
- `"ALPHA"` is a top-level header key; an `OUTPUTS` entry may carry its own
  `"ALPHA"` to override it for that attachment. Any other value is a parse error.
- **An explicit `PIPELINE_STATE.BLEND` / `BLEND_PER_ATTACHMENT` wins** over the
  `ALPHA` default (so does the raw raster's "Enable blend" control). Use
  `"BLEND": false` to replace instead of composite, e.g. for data textures whose
  alpha is not coverage.
- A CSF storage image is copied into the consumer with the same rule, using the
  CSF's `ALPHA`. MRT outputs (ISF `OUTPUTS`, raw raster `FRAGMENT_OUTPUTS`) are
  stored premultiplied in the node's own textures and copied with
  premultiplied over.
- **Several cables into one input** are composited in order of their source
  node (the order the nodes were created, then the output index), each over the
  ones before it.
- **Sampling returns what is stored**: `IMG_PIXEL` / `IMG_NORM_PIXEL` of an image
  input give premultiplied colour. A shader that forwards texels unchanged must
  declare `"ALPHA": "premultiplied"`; declared straight, it would multiply the
  colour by alpha a second time. A straight shader that needs straight colour
  divides by alpha itself (`c.a > 0.0 ? c.rgb / c.a : vec3(0.0)`).
- Inside a multi-pass ISF, pass targets follow the ISF reference renderer: they
  clear to `(0, 0, 0, 0)` and each pass **replaces** its target (no blend), so a
  pass reads exactly what the previous one wrote. Only the pass that reaches the
  node's output (or the copy of a persistent last pass) is composited with the
  `ALPHA` rule.

## 12. Verifying a shader you wrote

```sh
# 1. bake on all four dialects — catches D3D/Metal-only defects CI misses
cp myshader.fs myshader.vs tests/gfx/corpus/
ninja test_gfx_shader_corpus_targets
DISPLAY=:0 SCORE_TEST_API=vulkan ./tests/gfx/test_gfx_shader_corpus_targets

# 2. render it and read pixels
ninja test_gfx_shader_guide_examples
DISPLAY=:0 SCORE_TEST_API=vulkan ./tests/gfx/test_gfx_shader_guide_examples
```

Helpers live in `tests/fixtures/score_test/Gfx.hpp`: `render_isf_chain`,
`render_raster`, `GfxPipeline`, `readback()`.

Three things that will waste your time otherwise:

- **Call render helpers inside `run_in_gui_app`.** From a bare `TEST_CASE` body
  they segfault in `QVulkanInstance::supportedApiVersion()` — the RHI probe runs
  before any `QGuiApplication` exists.
- **Rebuild the test executable, not just the plugin.** Tests link a static
  library; `ninja <test>` can report success having relinked nothing, leaving you
  measuring a days-old binary. Check its mtime before believing a result. This has
  produced phantom crashes, phantom passes and a wrong root cause in this repo.
- **Give every oracle a witness** — a channel whose value proves the shader ran —
  so a green assertion cannot be vacuous. If your probe can return all-zero both
  when the shader is wrong and when nothing drew, it is not a test.
