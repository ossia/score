#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_gfx_export.h>

#include <optional>
#include <vector>

#include <verdigris>
namespace Gfx::Settings
{

struct GraphicsApis
{
  const QString OpenGL{"OpenGL"};
  const QString Vulkan{"Vulkan"};
  const QString Metal{"Metal"};
  const QString D3D11{"Direct3D 11"};
  const QString D3D12{"Direct3D 12"};
  operator QStringList() const noexcept;
};

struct HardwareVideoDecoder
{
  const QString Auto{"Auto"};
  const QString None{"None"};
  const QString CUDA{"CUDA"};
  const QString QSV{"Intel QuickSync"};
  const QString VDPAU{"VDPAU"};
  const QString VAAPI{"VA-API"};
  const QString D3D{"Direct3D 11"};
  const QString D3D12{"Direct3D 12"};
  const QString DXVA{"DXVA2"};
  const QString VideoToolbox{"Video Toolbox"};
  const QString V4L2{"V4L2-M2M"};
  const QString VulkanVideo{"Vulkan Video"};
  operator QStringList() const noexcept;
};

class SCORE_PLUGIN_GFX_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QString m_GraphicsApi{};
  QString m_HardwareDecode{};
  int m_DecodingThreads{};
  double m_Rate{};
  int m_Samples{1};
  bool m_VSync{};
  int m_Buffers{3};
  int m_GpuIndex{-1};

public:
  Model(
      const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
      const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, QString, HardwareDecode)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, int, DecodingThreads)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, double, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, int, Samples)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, bool, VSync)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, int, Buffers)
  //! Index in availableVulkanGpus(), -1 for the default.
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, int, GpuIndex)

public:
  score::gfx::GraphicsApi graphicsApiEnum() const noexcept;
  QString getGraphicsApi() const;
  void initGraphicsApi(QString);
  void setGraphicsApi(QString);
  void GraphicsApiChanged(QString arg)
      E_SIGNAL(SCORE_PLUGIN_GFX_EXPORT, GraphicsApiChanged, arg)
  SCORE_SETTINGS_PROPERTY(QString, GraphicsApi)

  // Required as D3D12 requires a minimum of 2 samples
  int resolveSamples(score::gfx::GraphicsApi) const noexcept;
  static int resolveSamples(score::gfx::GraphicsApi, int requested) noexcept;
};

SCORE_SETTINGS_PARAMETER(Model, GraphicsApi)
SCORE_SETTINGS_PARAMETER(Model, HardwareDecode)
SCORE_SETTINGS_PARAMETER(Model, DecodingThreads)
SCORE_SETTINGS_PARAMETER(Model, Rate)
SCORE_SETTINGS_PARAMETER(Model, Samples)
SCORE_SETTINGS_PARAMETER(Model, VSync)
SCORE_SETTINGS_PARAMETER(Model, Buffers)
SCORE_SETTINGS_PARAMETER(Model, GpuIndex)

//! Selects the GPU, by index in availableVulkanGpus(), the outputs created from
//! now on use; -1 for the backend's default. Never touches the environment.
SCORE_PLUGIN_GFX_EXPORT
void applyGpuIndex(int index) noexcept;

SCORE_PLUGIN_GFX_EXPORT
int requestedGpuIndex() noexcept;

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
  //! VK_NV_present_barrier is exposed: swap chains on this device can join a
  //! present barrier (frame lock across outputs and, with Quadro Sync, hosts).
  bool presentBarrier{};
  //! Only reported on Windows: the DXGI adapter of the same GPU.
  std::optional<GpuLuid> luid;
};
//! The Vulkan physical devices, in vkEnumeratePhysicalDevices order.
SCORE_PLUGIN_GFX_EXPORT
std::vector<GpuDescription> availableVulkanGpus();

//! The adapter of the selected GPU, unless none is selected, it is unavailable
//! or QT_D3D_ADAPTER_INDEX was set at startup.
SCORE_PLUGIN_GFX_EXPORT
std::optional<GpuLuid> requestedD3DAdapterLuid();

SCORE_PLUGIN_GFX_EXPORT
QShaderVersion shaderVersionForAPI(score::gfx::GraphicsApi) noexcept;

//! On OpenGL, the version for the context the rhi actually got, desktop or ES.
SCORE_PLUGIN_GFX_EXPORT
QShaderVersion shaderVersionForRhi(score::gfx::GraphicsApi, QRhi&) noexcept;

/**
 * @brief The configured sample count, or a usable default with no application.
 *
 * The gfx graph is not only driven by ossia-score: the PipeWire round-trip
 * harness and the corpus renderer construct a bare QApplication and no
 * score::ApplicationInterface at all. score::AppContext() then binds a
 * reference to a null instance, which is a hard UBSan failure and a plain
 * null dereference in a build without one.
 */
SCORE_PLUGIN_GFX_EXPORT
int samplesForCurrentApplication(score::gfx::GraphicsApi) noexcept;

//! The configured graphics API, or a platform default with no application.
SCORE_PLUGIN_GFX_EXPORT
score::gfx::GraphicsApi graphicsApiForCurrentApplication() noexcept;

//! The configured render rate in Hz, or 60 with no application.
SCORE_PLUGIN_GFX_EXPORT
double renderRateForCurrentApplication() noexcept;
}
