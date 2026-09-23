#include <Gfx/Graph/Quirks.hpp>

#include <QtGui/private/qrhi_p.h>

namespace score::gfx
{
void Quirks::populate(QRhi& rhi)
{
  if(qEnvironmentVariableIntValue("SCORE_GFX_NO_QUIRKS") > 0)
    return;

  static constexpr uint32_t nvidia_vendor_id = 0x10de;
  emptyPassSampleDescentLosesDevice
      = rhi.backend() == QRhi::Vulkan && rhi.driverInfo().vendorId == nvidia_vendor_id;
}
}
