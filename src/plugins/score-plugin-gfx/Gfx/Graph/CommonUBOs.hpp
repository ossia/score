#pragma once
#include <ossia/detail/packed_struct.hpp>

#include <cstdint>

namespace score::gfx
{

/**
 * @brief UBO specific to individual processes / nodes.
 */
packed_struct ProcessUBO
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
end_packed_struct

    /**
 * @brief UBO shared across all entities shown with the same camera.
 */
    packed_struct ModelCameraUBO
{
  float mvp[16]{};
  float mv[16]{};
  float model[16]{};
  float view[16]{};
  float projection[16]{};
  float modelNormal[9]{};
  float padding[3]; // Needed as a mat3 needs a bit more space...
};
end_packed_struct

    static_assert(
        sizeof(ModelCameraUBO) == sizeof(float) * (16 + 16 + 16 + 16 + 16 + 9 + 3));

/**
 * @brief UBO shared across all entities shown on the same output.
 */
packed_struct OutputUBO
{
  float clipSpaceCorrMatrix[16]{};
  float texcoordAdjust[2]{};

  float renderSize[2]{};
};
end_packed_struct

}
