#pragma once
#include <QString>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace score::gfx
{
struct GpuLuid
{
  quint32 low{};
  qint32 high{};
};

struct GpuDescription
{
  int index{};
  QString name;
  QString type;
  //! Stable across driver updates and device order changes: the Vulkan device
  //! UUID in hex, or vendor:device:name when the driver has none.
  QString key;
  //! Only reported on Windows: the DXGI adapter of the same GPU.
  std::optional<GpuLuid> luid;
};

//! The Vulkan physical devices, in vkEnumeratePhysicalDevices order,
//! enumerated once per process.
SCORE_PLUGIN_GFX_EXPORT
const std::vector<GpuDescription>& availableGpus();

//! Selects the GPU of that key for the outputs created from now on, or the
//! backend's default when empty or when no device has it (false then).
//! GUI thread.
SCORE_PLUGIN_GFX_EXPORT
bool selectGpu(const QString& key);

//! The selected GPU's index in availableGpus(), -1 for the default.
SCORE_PLUGIN_GFX_EXPORT
int selectedGpuIndex() noexcept;

//! The adapter of the selected GPU for Direct3D, unless none is selected, it
//! has no LUID, or QT_D3D_ADAPTER_INDEX was set at startup.
SCORE_PLUGIN_GFX_EXPORT
std::optional<GpuLuid> selectedD3DAdapterLuid() noexcept;
}
