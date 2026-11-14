#pragma once
#include "Scenario/Commands/ScriptEditCommand.hpp"

#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <YSFX/ProcessMetadata.hpp>
#include <YSFX/StateGuard.hpp>

#include <score/widgets/PluginWindow.hpp>

#include <score_plugin_ysfx_export.h>

#include <array>
#include <bitset>
#include <memory>
#include <vector>
#include <verdigris>

#if __has_include(<ysfx-s.h>)
#include <ysfx-s.h>
#else
#include <ysfx.h>
#endif

namespace YSFX
{
//! The script's sliders and its @serialize data, while the guard keeps the
//! audio thread out of the script
QByteArray readYSFXState(ysfx_t& fx, StateGuard& guard);
void loadYSFXState(ysfx_t& fx, StateGuard& guard, const QByteArray& state);

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
  //! Shared with the executor's node, which runs the same script
  std::shared_ptr<StateGuard> stateGuard{std::make_shared<StateGuard>()};

  std::vector<Process::Preset> builtinPresets() const noexcept override;

  //! The MIDI the script sent when it was loaded, before it could play: its
  //! first @block runs then
  std::vector<std::vector<uint8_t>> takeMidiBeforePlay() noexcept;

  PROPERTY(QString, script READ text WRITE setText NOTIFY scriptChanged)

private:
  friend class Window;
  //! @init, then @slider and @block on no frames
  void startScript();
  void recreatePorts();
  //! A change of the control sets the slider of the script
  void connectSlider(Process::ControlInlet& control, uint32_t slider);
  //! The controls loaded with the process, connected to the script's sliders
  void connectSliders();
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset&) override;
  //! A preset of its bank, or of the same script, sets sliders: only another
  //! script makes other ports
  bool presetChangesPorts(const Process::Preset& preset) const noexcept override;
  Process::Preset savePreset() const noexcept override;
  [[nodiscard]] Process::ScriptChangeResult reload();

  ysfx_bank_t* m_bank{};
  QString m_jsfx_path;
  QString m_text;

  // If we reload something, to preserve state when the object doesn't create
  QByteArray m_saved_state;

  std::bitset<ysfx_max_sliders> m_sliderBeingChanged{};
  std::vector<std::vector<uint8_t>> m_midiBeforePlay;
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
  bool m_retina{};
  void rebuild();
#if __has_include(<ysfx-s.h>)
  //! The controls of the sliders in masks show their value
  void showSliders(const std::array<uint64_t, ysfx_max_slider_groups>& masks);
  //! The controls of the sliders in masks take their value
  void editSliders(const std::array<uint64_t, ysfx_max_slider_groups>& masks);
#endif
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
