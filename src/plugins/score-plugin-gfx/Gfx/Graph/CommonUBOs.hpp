#pragma once

#include <cstdint>

namespace score::gfx
{
#pragma pack(push, 1)
/**
 * @brief UBO specific to individual processes / nodes.
 */
struct ProcessUBO
{
  float time{};
  float timeDelta{};
  float progress{};
  float sampleRate{};

  int32_t passIndex{};
  int32_t frameIndex{};

  float renderSize[2]{2048, 2048};
  float date[4]{0.f, 0.f, 0.f, 0.f};

  // Mirrors gl_NumWorkGroups for CSF compute shaders. Populated by
  // RenderedCSFNode just before dispatch so the libisf-injected
  // `#define gl_NumWorkGroups isf_process_uniforms.NUMWORKGROUPS_`
  // resolves to real dispatch counts on every backend (especially D3D
  // where SPIRV-Cross refuses to emit the built-in directly).
  // std140 packs uvec3 into a vec4 slot — the trailing word is padding.
  uint32_t numWorkgroups[3]{};
  uint32_t _numWorkgroups_pad{};
};

/**
 * @brief UBO shared across all entities shown with the same camera.
 */
struct ModelCameraUBO
{
  // clang-format off
  float mvp[16]{};
  float mv[16]{};
  float model[16]{
      1., 0., 0., 0.,
      0., 1., 0., 0.,
      0., 0., 1., 0.,
      0., 0., 0., 1.,
  };
  float view[16]{};
  float projection[16]{};
  float modelNormal[9]{};
  float padding[3]; // Needed as a mat3 needs a bit more space...
  float fov = 90.f;
  // NB: must NOT be named `near`/`far` — those are legacy macros defined by
  // <windows.h>; naming members after them forces an #undef that then breaks
  // any Windows system header (mmeapi.h, combaseapi.h) included afterwards.
  float znear = 0.001f;  //!< Used by non-matrix projections (fulldome, …) for reverse-Z depth.
  float zfar = 10000.f;  //!< idem.
  // clang-format on
};

static_assert(
    sizeof(ModelCameraUBO)
    == sizeof(float) * (16 + 16 + 16 + 16 + 16 + 9 + 3 + 1 + 1 + 1));

/**
 * @brief UBO shared across all entities shown on the same output.
 */
struct OutputUBO
{
  float clipSpaceCorrMatrix[16]{};

  float renderSize[2]{};

  // MSAA sample count of the bound output target. Mirrors
  // RenderList::samples(); shaders need it because gl_NumSamples is
  // stripped by glslang under SPIR-V. The trailing pad keeps the UBO
  // aligned to a vec4 boundary (std140-friendly).
  int32_t sampleCount{1};
  int32_t _pad0{0};
};

/**
 * @brief UBO shared across all video objects.
 */
struct VideoMaterialUBO
{
  float scale[2]{1.f, 1.f};
  float textureSize[2]{1.f, 1.f};
};

#pragma pack(pop)
}
