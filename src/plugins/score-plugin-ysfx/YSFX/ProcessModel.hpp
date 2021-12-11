#pragma once
#include <YSFX/ProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <Effect/EffectFactory.hpp>
#include <Control/DefaultEffectItem.hpp>

#include <score_plugin_ysfx_export.h>

#include <QDialog>
#include <memory>
#include <verdigris>

#include <ysfx.h>

namespace YSFX
{
class ProcessModel;
class SCORE_PLUGIN_YSFX_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(YSFX::ProcessModel)
  W_OBJECT(ProcessModel)
public:
  static constexpr bool hasExternalUI() noexcept { return true; }

  explicit ProcessModel(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
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

private:
  QString m_script;
};

class Window : public QDialog
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
  void timerEvent(QTimerEvent* event) override;
};

using LayerFactory = Process::EffectLayerFactory_T<
  ProcessModel,
  Process::DefaultEffectItem,
  Window>;

}
