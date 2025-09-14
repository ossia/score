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
};

/**
 * @brief UBO shared across all entities shown with the same camera.
 */
struct ModelCameraUBO
{
  float mvp[16]{};
  float mv[16]{};
  float model[16]{};
  float view[16]{};
  float projection[16]{};
  float modelNormal[9]{};
  float padding[3]; // Needed as a mat3 needs a bit more space...
  float fov = 90.;
};

static_assert(
    sizeof(ModelCameraUBO) == sizeof(float) * (16 + 16 + 16 + 16 + 16 + 9 + 3 + 1));

/**
 * @brief UBO shared across all entities shown on the same output.
 */
struct OutputUBO
{
  float clipSpaceCorrMatrix[16]{};

  float renderSize[2]{};
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
