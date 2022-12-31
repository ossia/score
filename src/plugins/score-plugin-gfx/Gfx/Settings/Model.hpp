#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_gfx_export.h>

#include <verdigris>
namespace Gfx::Settings
{

struct GraphicsApis
{
  const QString OpenGL{"OpenGL"};
  const QString Vulkan{"Vulkan"};
  const QString Metal{"Metal"};
  const QString D3D11{"Direct3D 11"};
  operator QStringList() const noexcept;
};

struct HardwareVideoDecoder
{
  const QString None{"None"};
  const QString CUDA{"CUDA"};
  const QString QSV{"Intel QuickSync"};
  const QString VDPAU{"VDPAU"};
  const QString VAAPI{"VA-API"};
  const QString D3D{"Direct3D 11"};
  const QString DXVA{"DXVA2"};
  const QString VideoToolbox{"Video Toolbox"};
  operator QStringList() const noexcept;
};

class Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QString m_GraphicsApi{};
  QString m_HardwareDecode{};
  double m_Rate{};
  int m_Samples{1};
  bool m_VSync{};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, QString, HardwareDecode)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, double, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, int, Samples)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_GFX_EXPORT, bool, VSync)

public:
  score::gfx::GraphicsApi graphicsApiEnum() const noexcept;
  QString getGraphicsApi() const;
  void initGraphicsApi(QString);
  void setGraphicsApi(QString);
  void GraphicsApiChanged(QString arg)
      E_SIGNAL(SCORE_PLUGIN_GFX_EXPORT, GraphicsApiChanged, arg)
  SCORE_SETTINGS_PROPERTY(QString, GraphicsApi)
};

SCORE_SETTINGS_PARAMETER(Model, GraphicsApi)
SCORE_SETTINGS_PARAMETER(Model, HardwareDecode)
SCORE_SETTINGS_PARAMETER(Model, Rate)
SCORE_SETTINGS_PARAMETER(Model, Samples)
SCORE_SETTINGS_PARAMETER(Model, VSync)
}
