#pragma once
#include <ossia/detail/variant.hpp>

#include <score_plugin_gfx_export.h>

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace isf
{
class invalid_file : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

struct event_input
{
};

struct bool_input
{
  using value_type = bool;
  using has_default = std::true_type;
  bool def{};
};

struct long_input
{
  using value_type = int64_t;
  using has_minmax = std::true_type;
  std::vector<ossia::variant<int64_t, double, std::string>> values;
  std::vector<std::string> labels;

  // Enum mode (values/labels non-empty): `def` is the INDEX into `values`.
  // Numeric mode (values empty, min/max set): `def` is the default VALUE.
  //
  // The shader always receives the selected numeric VALUE from `values[i]`
  // (for int/double entries) or the INDEX (for string-only VALUES, since
  // GLSL can't consume strings). The renderer's UBO-init path resolves this
  // index→value step so the initial shader state matches what arrives after
  // any user interaction — see ISFNode.cpp / GeometryFilterNode.cpp long_input
  // port visitors.
  std::size_t def{};

  // Numeric mode: when values/labels are empty and min/max are set,
  // create an IntSpinBox instead of a ComboBox. In that mode `def` is the
  // default value directly (not an index).
  std::optional<int64_t> min;
  std::optional<int64_t> max;
};

struct float_input
{
  using value_type = double;
  using has_minmax = std::true_type;
  double min{0.};
  double max{0.};
  double def{0.};
};

struct point2d_input
{
  using value_type = std::array<double, 2>;
  using has_minmax = std::true_type;
  std::optional<value_type> def{};
  std::optional<value_type> min{};
  std::optional<value_type> max{};
};

struct point3d_input
{
  using value_type = std::array<double, 3>;
  using has_minmax = std::true_type;
  std::optional<value_type> def{};
  std::optional<value_type> min{};
  std::optional<value_type> max{};

  // AS_COLOR: hint to the UI that this vec3 should be shown as a color
  // swatch (RGB picker) rather than three spin boxes. Useful for e.g.
  // direction-as-RGB visualisations where editing components individually
  // is awkward. Does not affect the GLSL type (still vec3).
  bool as_color{false};
};

struct color_input
{
  using value_type = std::array<double, 4>;
  using has_minmax = std::true_type;
  std::optional<value_type> def{};
  std::optional<value_type> min{};
  std::optional<value_type> max{};
};

// Sampler configuration fields shared by image/texture/cubemap inputs.
// All fields are optional: empty/unset string keeps the current default.
// Address modes accept: "repeat", "clamp_to_edge"/"clamp", "mirror"/"mirrored_repeat",
//                       "mirror_once"/"mirror_clamp_to_edge".
// Filter modes accept:  "nearest", "linear" (and "none" for mipmap_mode).
// Border color accepts: "transparent_black"/"transparent", "opaque_black", "opaque_white".
// Compare op accepts:   "never", "less", "less_equal"/"lequal", "equal",
//                       "greater", "greater_equal"/"gequal", "not_equal"/"neq", "always".
//                       When set (and not "never") a comparison sampler is created and
//                       the GLSL type becomes sampler*Shadow. Supported on 2D,
//                       2D-array, cubemap (image/texture/cubemap inputs) and
//                       cubemap-array (AUXILIARY only). Silently dropped with a
//                       stderr warning on 3D inputs (sampler3DShadow is not a core
//                       GLSL type) — use a 2D / 2D-array / cube shadow instead.
//                       With the engine's reverse-Z convention, the typical
//                       compare op for a standard "shadowed if closer" test is
//                       "greater_equal" (not "less_equal").
struct sampler_config
{
  std::string wrap;       // Applied to all 3 axes if individual WRAP_S/T/R unset
  std::string wrap_s;
  std::string wrap_t;
  std::string wrap_r;
  std::string filter;     // Applied to both min and mag if individual MIN/MAG_FILTER unset
  std::string min_filter;
  std::string mag_filter;
  std::string mipmap_mode;
  std::optional<float> anisotropy;
  std::string border_color;
  std::optional<float> lod_bias;
  std::optional<float> min_lod;
  std::optional<float> max_lod;
  std::string compare; // empty / "never" = no comparison sampler
};

struct image_input
{
  int dimensions{2};    // 2 or 3
  bool depth{false};    // true = shader wants sampleable depth on this input
  bool is_array{false}; // true = sampler2DArray rather than sampler2D
  // STATIC: producer publishes a long-lived QRhiTexture that downstream binds
  // directly; engine skips the consumer-side render-target allocation. Use for
  // precomputed LUTs, IBL bakes, asset caches — anything where the upstream
  // is a CPU producer (avnd gpu_texture_output, etc.) rather than an ISF /
  // raster pass that draws into the consumer's RT each frame. Orthogonal to
  // dimensions / is_array (cube + 3D + array inputs already grab from source
  // implicitly because they can't be 2D color attachments anyway).
  bool is_static{false};
  sampler_config sampler;
};

struct cubemap_input
{
  // DEPTH: true = request a sampleable depth cube alongside the color cube.
  // Mirrors image_input::depth: pairs the main `samplerCube` (or
  // `samplerCubeShadow` under COMPARE) with a `samplerCube <name>_depth`
  // companion for raw depth reads. Useful for omni-directional scene probes
  // where the upstream provides both a colour cube and its depth cube.
  // For plain shadow-cube sampling (HW PCF only) set COMPARE instead and
  // leave DEPTH false — the texture already has to be depth-format for the
  // compare sampler to return meaningful values.
  //
  // Note: cube-arrays (samplerCubeArray) are intentionally NOT exposed. No
  // QRhi backend (Vulkan/D3D12/Metal/GL) constructs a cube-array view
  // correctly from the CubeMap | TextureArray flag combination, so the
  // shader-side type would always disagree with the bound resource. Bind N
  // individual cubemap inputs instead, or decompose to a sampler2DArray
  // with face math in the shader.
  bool depth{false};
  sampler_config sampler;
};

// Sampler state accepted by all audio input flavours. Reuses the same
// string vocabulary as sampler_config (see above) — any unrecognised or
// empty string keeps the built-in default (linear / clamp_to_edge). Full
// sampler_config is overkill here: audio textures are 1-mip 2D samplers
// with no COMPARE / BORDER_COLOR / LOD semantics, so only FILTER and WRAP
// are honoured. Nearest filtering is the common ask for band-exact FFT
// reads where linear interpolation would smear adjacent bins.
struct audio_sampler_config
{
  std::string filter; // "nearest" or "linear" (default)
  std::string wrap;   // "repeat", "clamp_to_edge"/"clamp", "mirror"/"mirrored_repeat"
};

struct audio_input
{
  int max{};
  audio_sampler_config sampler;
};

struct audioFFT_input
{
  int max{};
  audio_sampler_config sampler;
};

struct audioHist_input
{
  int max{};
  audio_sampler_config sampler;
};

// UBO-style input declared in INPUTS as `"TYPE": "uniform"`.
//
// Emitted as `layout(std140, binding=N) uniform <name>_t { ... } <name>;`
// and bound via QRhiShaderResourceBinding::uniformBuffer (not bufferLoad).
//
// Use for small (≤ MaxUniformBufferRange, typically 16KB), read-only data
// like cameras, light/material counts, indexing constants. For larger or
// writable data, use `storage_input` (SSBO) instead.
struct uniform_input
{
  // Reuse storage_input's layout_field shape via full struct definition here
  // to keep the type self-contained.
  struct layout_field
  {
    std::string name;
    std::string type;
  };

  std::vector<layout_field> layout;

  // VISIBILITY: which shader stage(s) see this binding in a graphics pipeline.
  // Accepted values: "vertex+fragment"/"both" (default), "fragment", "vertex",
  // "compute" (implicit for CSF).
  std::string visibility{"vertex+fragment"};
};

// CSF-specific input types
struct storage_input
{
  std::string access; // "read_only", "write_only", "read_write"

  struct layout_field
  {
    std::string name;
    std::string type;
  };

  std::vector<layout_field> layout;

  std::string buffer_usage; // "", "indirect_draw", "indirect_draw_indexed"

  // PERSISTENT: creates a ping-pong pair of SSBOs swapped each frame.
  // In GLSL, `name` is the current (read-write) buffer, `name_prev` is the
  // previous frame's read-only buffer.
  bool persistent{false};

  // VISIBILITY: which shader stage(s) see this binding in a graphics pipeline.
  // Accepted values: "fragment" (default), "vertex", "vertex+fragment"/"both",
  // "compute" (implicit for CSF), "none" (no shader binding).
  std::string visibility{"fragment"};
};

struct texture_input
{
  int dimensions{2}; // 2 or 3
  sampler_config sampler;
};

struct csf_image_input
{
  std::string access; // "read_only", "write_only", "read_write"
  std::string format; // "RGBA8", "R32F", etc.

  std::string width_expression;
  std::string height_expression;
  std::string depth_expression; // non-empty means 3D texture

  int dimensions{2}; // 2 or 3 (alternative to depth_expression for declaring 3D)

  // Set internally when the RESOURCES entry uses TYPE: "image_cube".
  // Writable cubemap (imageCube in GLSL, QRhiTexture::CubeMap |
  // UsedWithLoadStore). Width must equal height (face edge length). Use for
  // in-compute reflection-probe baking, environment IBL, etc. Read-only
  // sampling of the same data is done via TYPE: "cubemap".
  bool cubemap{false};

  // IS_ARRAY: writable 2D texture array (image2DArray in GLSL, allocated
  // via QRhi::newTextureArray + UsedWithLoadStore). Layer count comes from
  // layers_expression (LAYERS: "$USER" / literal). Useful for shadow
  // cascades, layered G-buffers, compute-written texture atlases.
  //
  // Cube-arrays (imageCubeArray) are intentionally NOT supported: no QRhi
  // backend plumbs CubeMap | TextureArray views correctly, and the shader-
  // side type would disagree with the bound resource. The parser rejects
  // is_array + cubemap combinations with a stderr warning.
  bool is_array{false};
  std::string layers_expression; // LAYERS: expression for arraySize, may contain $USER

  // VISIBILITY: which shader stage(s) see this binding.
  // Accepted: "compute" (default), "fragment", "vertex", "vertex+fragment"/"both".
  std::string visibility{"compute"};

  // PERSISTENT: creates a ping-pong pair of images swapped each frame.
  // In GLSL, `<name>` is the current (write or read_write) image and
  // `<name>_prev` is the previous frame's read-only image — mirrors the
  // storage_input convention. Works for both 2D and 3D images.
  bool persistent{false};

  // GENERATE_MIPS: when true, the runtime runs QRhi's generateMips() on
  // this image after every frame's compute dispatches complete, so
  // downstream samplers with MIPMAP_MODE: linear / nearest see a valid
  // mip chain instead of zero-filled upper levels. Ignored for 3D images,
  // cubemaps, and 2D arrays where generateMips semantics differ across
  // QRhi backends (per-face / per-layer / per-slice).
  bool generate_mips{false};

  bool is3D() const noexcept { return dimensions == 3 || !depth_expression.empty(); }
  bool isCube() const noexcept { return cubemap; }
};

// CSF geometry port input: SoA layout, one SSBO per attribute.
// Declares which geometry attributes the compute shader wants to access.
struct geometry_input
{
  // Explicit cross-geometry forwarding directive.
  // Used when an output geometry needs data from a different input geometry.
  struct copy_from
  {
    std::string geometry;   // Source geometry resource name (e.g. "geoIn")
    std::string attribute;  // Source attribute name (for attribute forwarding)
    std::string auxiliary;  // Source auxiliary name (for auxiliary forwarding; defaults to this name)
  };

  struct attribute_request
  {
    std::string name;     // Attribute name used in GLSL (e.g. "position", "velocity")
    std::string semantic; // Maps to ossia::attribute_semantic name (e.g. "position", "custom")
    std::string type;     // GLSL type (e.g. "vec3", "vec4", "float")
    std::string access;   // "read_only", "write_only", "read_write"
    std::string rate;     // "vertex" (default) or "instance"
    bool required{true};  // false = optional, zero fallback if missing

    // If set, this attribute is forwarded from another geometry's buffer
    // rather than being allocated/computed by this shader.
    std::optional<copy_from> forward;
  };

  // Structured buffers that travel with the geometry (matched by name
  // against ossia::geometry::auxiliary_buffer entries). Default kind is
  // SSBO (`layout(std430) buffer`); set `is_uniform = true` to declare a
  // std140 UBO instead (`layout(std140) uniform`).
  struct auxiliary_request
  {
    std::string name;
    std::string access; // "read_only", "write_only", "read_write"
                        //  (meaningful for SSBO kind only; UBO is always read-only from GLSL)
    std::vector<storage_input::layout_field> layout;
    std::string size; // expression for flexible array count, may contain $USER
                      //  (SSBO only; UBOs require fixed-size layouts per std140)

    // If set, this auxiliary is forwarded from another geometry's upstream.
    std::optional<copy_from> forward;

    // Raw-raster only: when true the node owns a ping-pong pair of buffers
    // (allocated from the LAYOUT + SIZE) that are swapped each frame, and
    // the auxiliary is NOT resolved from upstream geometry. In GLSL,
    // `<name>` is the current (writable) buffer, `<name>_prev` is the
    // previous frame's read-only buffer. Useful for temporal accumulation
    // / history buffers that live only in the rendering node.
    // (SSBO only; persistent ping-pong makes no sense for read-only UBOs.)
    bool persistent{false};

    // When true, declare/bind this auxiliary as a std140 uniform block
    // (`layout(std140, binding=N) uniform name_t { … } name;`) and bind
    // with QRhiShaderResourceBinding::uniformBuffer. When false (default),
    // it's an std430 SSBO. The upstream geometry's
    // ossia::geometry::auxiliary_buffer is kind-agnostic — the shader's
    // declaration alone determines how the buffer is bound.
    bool is_uniform{false};
  };

  // Texture variant of auxiliary: resolved from ossia::geometry::auxiliary_textures
  // by name, no score input port. Declared in the top-level AUXILIARY array
  // with TYPE: "image" / "texture" / "cubemap". Unlike regular INPUTS
  // textures, does not create an input port — the texture handle travels
  // bundled with the geometry (e.g. ScenePreprocessor ships `base_color_array`
  // / `skybox` / `shadow_atlas`).
  struct auxiliary_texture_request
  {
    std::string name;
    int dimensions{2};     // 2 or 3
    bool is_array{false};  // sampler2DArray when true
    bool is_cubemap{false};// samplerCube when true
    bool is_depth{false};  // sampleable depth (promotes comparison when cfg set)
    // Storage-image kind: emit `image2D/3D/Cube/Array` with imageLoad/
    // imageStore semantics instead of `sampler2D/…` with texture(). Set
    // by TYPE: "storage_image" in the AUXILIARY JSON. Paired with:
    //   - `format`: GLSL layout qualifier (e.g. "rgba8", "r32f", "rgba16f").
    //   - `access`: "read_only" / "write_only" / "read_write", controlling
    //     imageLoad / imageStore / imageLoadStore binding type + the
    //     GLSL `readonly`/`writeonly` decoration.
    bool is_storage{false};
    std::string format{"rgba8"}; // only meaningful when is_storage
    std::string access{"read_write"}; // only meaningful when is_storage

    // Sizing expressions for write_only / read_write storage images. Same
    // convention as csf_image_input (top-level INPUTS images): an integer
    // literal or a `$variable` reference resolved against the shader's
    // long/float input ports + the standard $WIDTH/$HEIGHT/$DEPTH/$LAYERS
    // family. Empty → engine falls back to renderer state (renderSize for
    // 2D, voxel-resolution heuristics for 3D). When the engine
    // auto-allocates a writable nested-aux storage image, these strings
    // drive its dimensions; for sampled (read-only) entries they're
    // ignored — the texture comes from the upstream producer at whatever
    // size that producer baked.
    std::string width_expression;
    std::string height_expression;
    std::string depth_expression;   // 3rd dimension for 3D textures
    std::string layers_expression;  // array slice count for 2D arrays

    sampler_config sampler;
  };

  std::vector<attribute_request> attributes;
  std::vector<auxiliary_request> auxiliary;
  std::vector<auxiliary_texture_request> auxiliary_textures;

  std::string vertex_count;   // expression string, may contain $USER
  std::string instance_count; // expression string, may contain $USER

  // Optional format identity stamped onto the consumer geometry's
  // filter_tag (rapidhash truncated to 32 bits). Only meaningful on
  // RESOURCES of TYPE: geometry used as outputs (geoOut). Empty leaves
  // filter_tag at 0 (the "untagged" sentinel) — no routing change for
  // CSFs that don't author an output format.
  std::string format_id;

  struct indirect_request
  {
    std::string count; // expression string (same resolver as vertex_count)
  };
  std::optional<indirect_request> indirect;
};

struct input
{
  using input_impl = ossia::variant<
      float_input, long_input, event_input, bool_input, color_input, point2d_input,
      point3d_input, image_input, cubemap_input, audio_input, audioFFT_input,
      audioHist_input, storage_input, texture_input, csf_image_input,
      geometry_input, uniform_input>;

  std::string name;
  std::string label;

  input_impl data;
};

// Matches QShaderDescription::VariableType
enum class attribute_type
{
  Unknown = 0,

  Float,
  Vec2,
  Vec3,
  Vec4,
  Mat2,
  Mat2x3,
  Mat2x4,
  Mat3,
  Mat3x2,
  Mat3x4,
  Mat4,
  Mat4x2,
  Mat4x3,

  Int,
  Int2,
  Int3,
  Int4,

  Uint,
  Uint2,
  Uint3,
  Uint4,

  Bool,
  Bool2,
  Bool3,
  Bool4,

  Double,
  Double2,
  Double3,
  Double4,
  DMat2,
  DMat2x3,
  DMat2x4,
  DMat3,
  DMat3x2,
  DMat3x4,
  DMat4,
  DMat4x2,
  DMat4x3,

  Sampler1D,
  Sampler2D,
  Sampler2DMS,
  Sampler3D,
  SamplerCube,
  Sampler1DArray,
  Sampler2DArray,
  Sampler2DMSArray,
  Sampler3DArray,
  SamplerCubeArray,
  SamplerRect,
  SamplerBuffer,
  SamplerExternalOES,
  Sampler,

  Image1D,
  Image2D,
  Image2DMS,
  Image3D,
  ImageCube,
  Image1DArray,
  Image2DArray,
  Image2DMSArray,
  Image3DArray,
  ImageCubeArray,
  ImageRect,
  ImageBuffer,

  Struct,

  Half,
  Half2,
  Half3,
  Half4
};

struct vertex_attribute
{
  int location{};
  attribute_type type{};
  std::string name;

  // Optional explicit ossia attribute_semantic name ("position", "velocity",
  // "texcoord0", ..., "custom"). Only meaningful on `vertex_input` (raw
  // raster), where it controls how the runtime matches the declared input
  // to an upstream geometry attribute — same lookup algorithm as CSF
  // attribute_request. When empty, the parser implicitly uses `name` as the
  // semantic key. Set to "custom" to force exact-name matching against
  // custom attributes.
  std::string semantic;

  // Interpolation qualifier (only applicable to vertex_output / fragment_input).
  // Allowed: "smooth" (default), "flat", "noperspective", "centroid", "sample".
  // "sample" forces per-sample fragment shading on this varying — the fragment
  // shader runs once per MSAA sample for that coverage. Required when MSAA
  // outputs need per-sample correct interpolation (specular highlights,
  // normal-mapped surfaces). Empty string = default smooth.
  std::string interpolation;
};

struct vertex_input : vertex_attribute
{
  // When false, the raw-raster renderer tolerates an upstream geometry that
  // does not carry a matching attribute: instead of failing the pipeline
  // build, it synthesises a tiny PerInstance step_rate=1 buffer filled with
  // a neutral "identity" value (zero for translation, white for color, 1
  // for roughness, etc.) and binds that in place of the missing upstream
  // attribute. Lets a single shader cover both instanced and non-instanced
  // upstreams without per-shape variants.
  //
  // When false AND `default_val` is set, those explicit numbers are used
  // verbatim (after component-truncation / zero-padding against the
  // declared TYPE). When false AND `default_val` is empty, the runtime
  // looks the semantic up in a built-in whitelist (see
  // score::gfx::vertexFallbackDefault) — non-whitelisted semantics without
  // an explicit DEFAULT are rejected at pipeline-build time with a clear
  // error to avoid silently-wrong rendering.
  //
  // When true (default), the upstream geometry MUST provide the attribute
  // or the pipeline build fails — existing strict behaviour.
  bool required{true};

  // Explicit DEFAULT numbers from the JSON header. Stored as doubles for
  // JSON fidelity; converted to the runtime format (float / int) at
  // buffer-build time. Empty = use the whitelist neutral (see `required`).
  // Length is not pre-validated against TYPE here — the runtime truncates
  // or zero-pads to match the declared GLSL type width.
  std::vector<double> default_val;
};
struct vertex_output : vertex_attribute
{
};
struct fragment_input : vertex_attribute
{
};
struct fragment_output : vertex_attribute
{
};

// --- Pipeline state control (PIPELINE_STATE descriptor key) ---------------
//
// All fields are optional (std::optional): missing = keep current/legacy
// default. Two instances live in `descriptor`: a global `default_state`
// (from PIPELINE_STATE), and a per-pass `override_state` that merges on top.

struct blend_attachment
{
  bool enable{false};
  std::string src_color{"src_alpha"};
  std::string dst_color{"one_minus_src_alpha"};
  std::string op_color{"add"};
  std::string src_alpha{"one"};
  std::string dst_alpha{"one_minus_src_alpha"};
  std::string op_alpha{"add"};
  std::string color_write{"rgba"}; // "rgba", "rgb", "r", ...
};

struct stencil_op_state
{
  std::string fail_op{"keep"};
  std::string depth_fail_op{"keep"};
  std::string pass_op{"keep"};
  std::string compare_op{"always"};
};

struct pipeline_state
{
  std::optional<bool> depth_test;
  std::optional<bool> depth_write;
  std::optional<std::string> depth_compare; // "less", "less_equal", "greater", ...
  std::optional<float> depth_bias;
  std::optional<float> slope_scaled_depth_bias;

  std::optional<std::string> cull_mode;   // "none", "front", "back"
  std::optional<std::string> front_face;  // "ccw", "cw"
  std::optional<std::string> polygon_mode;// "fill", "line"
  std::optional<float> line_width;

  // Procedural-draw override (Vertex Shader Art style). When
  // `vertex_count` is set, the renderer issues a single
  // cb.draw(vertex_count, instance_count, 0, 0) and ignores the
  // incoming geometry's index / indirect buffers entirely. The vertex
  // shader drives positions purely from gl_VertexIndex +
  // gl_InstanceIndex. Use cases:
  //   - Fullscreen passes: VERTEX_COUNT=3, TOPOLOGY=triangles (skybox).
  //   - VSA-style plasma / curves: VERTEX_COUNT=10000,
  //     TOPOLOGY=line_strip.
  //   - Procedural particle grids: VERTEX_COUNT=65536, TOPOLOGY=points.
  //
  // Safety: if VERTEX_INPUTS is non-empty (the shader declares vertex
  // attribute reads), the renderer clamps vertex_count to the incoming
  // geometry's vertex_count to avoid reading past buffer ends. Shaders
  // that rely purely on gl_VertexIndex should declare an empty
  // `VERTEX_INPUTS: []` so the pipeline is built with no vertex
  // bindings and the draw count is used verbatim.
  std::optional<uint32_t> vertex_count;
  std::optional<uint32_t> instance_count;
  // Topology override. When unset, the incoming geometry's topology is
  // used. Values: "triangles", "triangle_strip", "triangle_fan",
  // "lines", "line_strip", "points".
  std::optional<std::string> topology;

  // Blending: either a single state applied to all color attachments, or a
  // per-attachment vector. If both are present the per-attachment wins.
  std::optional<blend_attachment> blend_all;
  std::vector<blend_attachment> blend_per_attachment;

  // Stencil (optional)
  std::optional<bool> stencil_test;
  std::optional<uint32_t> stencil_read_mask;
  std::optional<uint32_t> stencil_write_mask;
  std::optional<stencil_op_state> stencil_front;
  std::optional<stencil_op_state> stencil_back;

  // Variable-rate shading (VRS).
  //   "SHADING_RATE": [w, h]   — per-draw shading rate where w,h ∈ {1, 2, 4}.
  //                              [1,1] = 1×1 (full rate, default).
  //                              [2,2] = 1 invocation per 2×2 pixel block.
  //                              [4,4] = 1 per 4×4 block.
  // Combined with a shading-rate map (set on the render target) the actual
  // rate is the per-draw rate combined with the per-tile rate via the chosen
  // combiner op. Requires QRhi::Feature::VariableRateShading (Vulkan, D3D12).
  std::optional<std::array<int, 2>> shading_rate;
};

struct pass
{
  std::string target;
  bool persistent{};
  bool float_storage{};
  bool nearest_filter{};
  std::string width_expression{};
  std::string height_expression{};

  // Render to a specific layer of a texture-array output (-1 = layer 0).
  int layer{-1};

  // Render to a specific Z-slice of a 3D output. Expression string so the
  // slice can be computed from inputs (e.g. "$USER_slice"). Empty = slice 0
  // when the target is 3D, or irrelevant when 2D.
  std::string z_expression{};

  // Optional format override for the intermediate render target of this
  // pass (e.g. "rgba16f" for precision-sensitive blur stages). Empty = use
  // FLOAT: true mapping (rgba32f / rgba8) as before.
  std::string format{};

  // Per-pass pipeline state overrides (merged with descriptor.default_state).
  pipeline_state override_state;
};

struct output_declaration
{
  std::string name;     // User-chosen name (e.g. "color", "sceneDepth")
  std::string type;     // "color" (default) or "depth"

  // LAYERS: >1 allocates a texture array with this many layers.
  int layers{1};

  // DEPTH: >1 allocates a 3D texture of this depth. Mutually exclusive with
  // LAYERS (a ThreeDimensional texture is not a TextureArray). A fragment
  // PASSES entry with Z renders into a single Z-slice via a color attachment
  // with setLayer(z).
  int depth{1};

  // FORMAT: optional explicit texture format ("rgba8", "rgba16f", "r32f", "d32f", ...).
  // Empty = use the default (RGBA8 for color, D32F for depth).
  std::string format;

  // SAMPLES: MSAA sample count (1, 2, 4, 8, 16, 32, 64). 1 = no MSAA (default).
  // The renderer allocates an MSAA texture and inserts an automatic resolve
  // pass when downstream consumers expect a non-MSAA input. Each declared
  // OUTPUT can have its own sample count; the depth attachment for a colour
  // OUTPUT inherits the same sample count.
  int samples{1};

  // CUBEMAP: when true the output is allocated with the QRhi cubemap flag
  // so downstream consumers can bind it as a samplerCube. Implies
  // `layers == 6` on allocation even when the shader didn't set LAYERS
  // explicitly. Used by the IBL precompute path (irradiance_convolve,
  // prefilter_ggx) together with MULTIVIEW:6.
  bool is_cubemap{false};

  // GENERATE_MIPS: when true the runtime calls generateMips() on this
  // output's texture after the render pass completes, auto-averaging
  // the base level into a full mip chain. Implies the QRhi
  // `MipMapped` + `UsedWithGenerateMips` flags on allocation. Use this
  // for "source-data" targets whose base level is authored by the
  // fragment shader and whose sub-mips should be GPU-filtered (skybox
  // converter, base color textures, SSAO LUTs…). NOT for the
  // prefilter-style case where each mip has distinct shader-authored
  // content — use EXECUTION_MODEL: PER_MIP instead.
  bool generate_mips{false};

  // WIDTH / HEIGHT: explicit target size for offscreen outputs. Set
  // by the shader author when the intrinsic size of the algorithm
  // isn't tied to the window / swap-chain (IBL precompute, shadow
  // atlases, post-process LUTs, …). Zero → fall back to the
  // renderer's render-size (classic behaviour). Integer literal or
  // string expression; the expression is evaluated once at init
  // against the same variable surface as CSF dispatch expressions
  // ($WIDTH_<input> / $HEIGHT_<input> / scalar input values).
  //
  // All colour OUTPUTs of a single RAW_RASTER_PIPELINE shader share
  // a render pass and must therefore resolve to the same final size;
  // the runtime uses the first colour OUTPUT's resolved size as the
  // RT size and allocates every attachment at that size. Cubemaps
  // are additionally clamped to square via min(w, h) (QRhi contract).
  int width{0};
  int height{0};
  std::string width_expression;
  std::string height_expression;
};

struct descriptor
{
  enum Mode
  {
    ISF,
    VSA,
    CSF,
    RawRaster
  } mode{ISF};
  std::string description;
  std::string credits;
  std::vector<std::string> categories;
  std::vector<input> inputs;
  std::vector<output_declaration> outputs; // Parsed from OUTPUTS array; empty = single color output
  std::vector<pass> passes;
  std::vector<std::string> pass_targets;
  bool default_vertex_shader{};

  // For VSA
  int point_count{};
  std::string primitive_mode;
  std::string line_size;
  std::array<double, 4> background_color;

  // For CSF
  struct type_definition
  {
    std::string name;
    std::vector<storage_input::layout_field> layout;
  };
  std::vector<type_definition> types;

  struct dispatch_info
  {
    std::array<int, 3> local_size{16, 16, 1};
    std::string execution_type{"2D_IMAGE"}; // "2D_IMAGE", "1D_BUFFER", "PER_VERTEX", "PER_INSTANCE", "MANUAL", "USER"
    std::string target_resource;
    std::array<int, 3> workgroups{1, 1, 1}; // For MANUAL mode
    std::array<std::string, 3> stride{"1", "1", "1"}; // Per-axis stride (supports formulas)
    std::array<int, 3> user_dispatch_ports{-1, -1, -1}; // Port indices for USER mode (X, Y, Z)
  };
  std::vector<dispatch_info> csf_passes;

  // For raw shaders

  std::vector<vertex_input> vertex_inputs;
  std::vector<vertex_output> vertex_outputs;
  std::vector<fragment_input> fragment_inputs;
  std::vector<fragment_output> fragment_outputs;

  // Auxiliary SSBOs expected from upstream geometry (matched by name).
  // Populated from top-level AUXILIARY key in RAW_RASTER_PIPELINE mode.
  std::vector<geometry_input::auxiliary_request> auxiliary;

  // Auxiliary textures travelling with the geometry (matched by name
  // against ossia::geometry::auxiliary_textures). Populated from the same
  // top-level AUXILIARY array when entries have TYPE: "image" / "texture"
  // / "cubemap". Unlike INPUTS-declared textures they don't consume a
  // score input port — the renderer looks them up on the geometry every
  // frame.
  std::vector<geometry_input::auxiliary_texture_request> auxiliary_textures;

  // PIPELINE_STATE: global pipeline state (depth, blend, cull, stencil, ...).
  // Applies to every output pass; may be overridden per-pass via pass::override_state.
  pipeline_state default_state;

  // MULTIVIEW: render to N layers of a texture array in a single draw.
  // 0 or 1 = disabled. N>=2 = enabled (requires QRhi::MultiView capability).
  int multiview_count{0};

  // EXECUTION_MODEL (RAW_RASTER_PIPELINE only — silently ignored in other
  // modes). Drives the invocation count of the single raster pass:
  //
  //   "SINGLE"        (default) — one invocation per frame, RT bound at
  //                   mip 0.
  //   "PER_MIP"       — N invocations, RT bound at mip `i` on iteration
  //                     `i`. N is derived from the `target` texture's
  //                     mip chain (floor(log2(min(w, h))) + 1).
  //                     ProcessUBO.passIndex carries the mip index.
  //   "PER_CUBE_FACE" — 6 invocations, RT bound at cube layer `i`
  //                     (face order +X, -X, +Y, -Y, +Z, -Z). Target
  //                     OUTPUT must be CUBEMAP: true. Mutually
  //                     exclusive with MULTIVIEW (which already
  //                     amplifies one draw to 6 faces).
  //   "PER_LAYER"     — N invocations, RT bound at array layer `i`. N
  //                     comes from the target OUTPUT's `layers`
  //                     declaration. Works on either colour TextureArray
  //                     targets (setLayer attachment) or depth
  //                     TextureArray targets (rendered to a scratch
  //                     and copied into the array layer post-pass —
  //                     QRhi 6.11 has no per-layer depth attachment
  //                     API). ProcessUBO.passIndex carries the layer
  //                     index. Drives shadow_cascades.frag.
  //   "MANUAL"        — N invocations, same RT each time, where N is
  //                     evaluated from the `count` expression string
  //                     via the math_expression parser every frame
  //                     (same variable bindings as CSF's stride /
  //                     image-size expressions: $WIDTH, $HEIGHT,
  //                     $<inputName>, ...).
  struct raster_execution_model
  {
    std::string type;            // "SINGLE" / "PER_MIP" / "PER_CUBE_FACE" / "PER_LAYER" / "MANUAL"
    std::string target;          // PER_MIP / PER_CUBE_FACE / PER_LAYER: OUTPUT name to iterate
    std::string count_expression; // MANUAL: integer-valued expression
  };
  raster_execution_model execution_model;

  // User-declared GLSL extension names, emitted as `#extension NAME : require`
  // immediately after `#version` in every generated stage. Examples:
  // "GL_KHR_shader_subgroup_arithmetic", "GL_EXT_shader_atomic_float".
  std::vector<std::string> extensions{
      "GL_GOOGLE_include_directive", "GL_GOOGLE_cpp_style_line_directive"};

  // CLIP_DISTANCES: number of gl_ClipDistance[N] outputs the vertex shader
  // writes (1..8 typical). When > 0 the parser injects
  // `out float gl_ClipDistance[N];` in the vertex stage so user code can
  // assign without writing the declaration. Each declared distance enables
  // one user-defined clipping plane: fragments where gl_ClipDistance[i] < 0
  // are discarded.
  int clip_distances{0};

  // CULL_DISTANCES: like clip distances but per-primitive: a primitive whose
  // every vertex has all gl_CullDistance[i] < 0 is fully culled before
  // rasterisation. Useful for cheap frustum-/occlusion-style culling.
  int cull_distances{0};

  // DEPTH_LAYOUT: conservative-depth qualifier on gl_FragDepth. Allowed:
  //   "any"        — driver default (no guarantee, disables early-Z when
  //                  gl_FragDepth is written).
  //   "greater"    — promise the value written is >= the value rasterisation
  //                  would have produced. Lets the HW keep early-Z reject
  //                  for fragments already deeper than the depth buffer.
  //   "less"       — symmetric promise in the other direction.
  //   "unchanged"  — promise the written value equals the rasterised value
  //                  (mostly for documentation; same fast path as "greater"
  //                   on hardware where reverse-Z applies).
  // Empty = no qualifier emitted.
  std::string depth_layout;
};

class SCORE_PLUGIN_GFX_EXPORT parser
{
  std::string m_sourceVertex;
  std::string m_sourceFragment;
  std::string m_source_geometry_filter;
  int m_version{450};

  std::string m_vertex;
  std::string m_fragment;
  std::string m_geometry_filter;

  descriptor m_desc;

public:
  enum class ShaderType
  {
    Autodetect,
    ISF,
    ShaderToy,
    GLSLSandBox,
    GeometryFilter,
    VertexShaderArt,
    CSF,
    RawRasterPipeline,
    RawRaytracePipeline,
    RawMeshPipeline
  };
  parser(std::string vert, std::string frag, int glslVersion, ShaderType);
  explicit parser(std::string isf_geom_filter, ShaderType t);

  descriptor data() const;
  descriptor::Mode mode() const;
  std::string vertex() const;
  std::string fragment() const;
  std::string geometry_filter() const;
  std::string compute_shader() const;
  static std::pair<int, descriptor> parse_isf_header(std::string_view source);
  void parse_shadertoy_json(const std::string& json);

  std::string write_isf() const;

private:
  void parse_isf();
  void parse_raw_raster_pipeline();
  void parse_shadertoy();
  void parse_glsl_sandbox();
  void parse_geometry_filter();
  void parse_vsa();
  void parse_csf();
};
}
