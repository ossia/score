#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <Gfx/Graph/renderstate.hpp>
#include <verdigris>

#include <score_plugin_gfx_export.h>
namespace Gfx::Settings
{

struct GraphicsApis
{
  const QString OpenGL{"OpenGL"};
  const QString Vulkan{"Vulkan"};
  const QString Metal{"Metal"};
  const QString D3D11{"Direct3D 11"};
  operator QStringList() const {
    QStringList lst;
#ifndef QT_NO_OPENGL
    lst += OpenGL;
#endif
#if QT_CONFIG(vulkan)
    lst += Vulkan;
#endif
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    lst += D3D11;
#endif
#ifdef Q_OS_WIN
    lst += Metal;
#endif
    return lst;
  }
};

class Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QString m_GraphicsApi;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  GraphicsApi graphicsApiEnum() const noexcept;
  QString getGraphicsApi() const;
  void setGraphicsApi(QString);
  void GraphicsApiChanged(QString arg) E_SIGNAL(SCORE_PLUGIN_GFX_EXPORT, GraphicsApiChanged, arg)
  PROPERTY(QString, GraphicsApi, &Model::getGraphicsApi, &Model::setGraphicsApi, &Model::GraphicsApiChanged)
};

SCORE_SETTINGS_PARAMETER(Model, GraphicsApi)
}
