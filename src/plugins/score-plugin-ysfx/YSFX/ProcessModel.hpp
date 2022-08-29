#pragma once
#include <Process/Process.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <YSFX/ProcessMetadata.hpp>
#include <ysfx.h>

#include <score/widgets/PluginWindow.hpp>

#include <score_plugin_ysfx_export.h>

#include <memory>
#include <verdigris>

namespace YSFX
{
class ProcessModel;
class SCORE_PLUGIN_YSFX_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(YSFX::ProcessModel)
  W_OBJECT(ProcessModel)
public:
  bool hasExternalUI() const noexcept;

  explicit ProcessModel(
      const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~ProcessModel() override;

  void setScript(const QString& path);
  QString script() const noexcept;

  std::shared_ptr<ysfx_t> fx;

  std::vector<Process::Preset> builtinPresets() const noexcept override;

private:
  void loadPreset(const Process::Preset&) override;
  ysfx_bank_t* m_bank{};
  QString m_script;
};

class Window : public score::PluginWindow
{
public:
  Window(const ProcessModel& e, const score::DocumentContext& ctx, QWidget* parent);
  ~Window();

  const ProcessModel& m_model;

  std::shared_ptr<ysfx_t> fx;
  ysfx_gfx_config_t conf{};

private:
  QImage m_frame;
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void refreshTimer();
};

using LayerFactory
    = Process::EffectLayerFactory_T<ProcessModel, Process::DefaultEffectItem, Window>;

}
