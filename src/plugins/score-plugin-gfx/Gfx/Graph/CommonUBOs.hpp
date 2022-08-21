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

  int32_t passIndex{};
  int32_t frameIndex{};

  float date[4]{0.f, 0.f, 0.f, 0.f};
  float mouse[4]{0.5f, 0.5f, 0.5f, 0.5f};
  float channelTime[4]{0.5f, 0.5f, 0.5f, 0.5f};

  float sampleRate{};
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
};

    static_assert(
        sizeof(ModelCameraUBO) == sizeof(float) * (16 + 16 + 16 + 16 + 16 + 9 + 3));

/**
 * @brief UBO shared across all entities shown on the same output.
 */
struct OutputUBO
{
  float clipSpaceCorrMatrix[16]{};
  float texcoordAdjust[2]{};

  float renderSize[2]{};
};

#pragma pack(pop)
}
