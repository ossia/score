#pragma once
#include "Scenario/Commands/ScriptEditCommand.hpp"

#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <YSFX/ProcessMetadata.hpp>

#include <score/widgets/PluginWindow.hpp>

#include <score_plugin_ysfx_export.h>

#include <bitset>
#include <memory>
#include <verdigris>

#if __has_include(<ysfx-s.h>)
#include <ysfx-s.h>
#else
#include <ysfx.h>
#endif

namespace YSFX
{
class ProcessModel;
class Window;
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

  // Set script from a file path (initial load)
  void setInitialScript(const QString& path);
  QString script() const noexcept;

  // Live coding: set script from text content
  [[nodiscard]] Process::ScriptChangeResult setText(const QString& text);
  const QString& text() const noexcept { return m_text; }

  void scriptChanged(const QString& str) W_SIGNAL(scriptChanged, str);
  void programChanged() W_SIGNAL(programChanged);

  bool validate(const QString& txt) const noexcept;

  void errorMessage(int arg_1, const QString& arg_2) const
      W_SIGNAL(errorMessage, arg_1, arg_2);

  std::shared_ptr<ysfx_t> fx;

  std::vector<Process::Preset> builtinPresets() const noexcept override;

  PROPERTY(QString, script READ text WRITE setText NOTIFY scriptChanged)

private:
  friend class Window;
  void recreatePorts();
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset&) override;
  Process::Preset savePreset() const noexcept override;
  [[nodiscard]] Process::ScriptChangeResult reload();

  ysfx_bank_t* m_bank{};
  QString m_jsfx_path;
  QString m_text;

  std::bitset<ysfx_max_sliders> m_sliderBeingChanged{};
};

class Window : public score::PluginWindow
{
public:
  Window(ProcessModel& e, const score::DocumentContext& ctx, QWidget* parent);
  ~Window();

  QPointer<ProcessModel> m_model;

  std::shared_ptr<ysfx_t> fx;
  ysfx_gfx_config_t conf{};

private:
  QImage m_frame;
  std::vector<std::string> m_droppedFiles;
  int m_mouseHeldMenuEvent{};
  bool m_mouseHeld{};
  bool m_retina{};
  void rebuild();
  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void refreshTimer();
  void updateState();

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
};

struct LayerFactory;
}
