#pragma once
#include <Threedim/CalibrationPlan.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/network/value/value.hpp>

#include <QDialog>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <optional>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
namespace score
{
class Document;
}
namespace Process
{
class ProcessModel;
}

namespace Threedim
{
//! Sets up Structured Light Calibration (and Projector Blend Maps) for a
//! multi-window device and a camera, runs it, and assigns the result.
class CalibrationWizard final : public QDialog
{
public:
  CalibrationWizard(score::Document& doc, QWidget* parent = nullptr);
  ~CalibrationWizard() override;

  const score::Document* document() const noexcept { return m_doc; }
  //! Between Set up and the end of the run: the device's output stage is off.
  bool busy() const noexcept { return m_session.active(); }

  void refreshDevices();
  bool setUp();
  void start();
  void abort();
  //! The document is going away: stop without touching it.
  void detach();

  void done(int r) override;

  //! Last values of a process's outlets, read on the execution thread.
  struct OutletTap;

private:
  const score::DocumentContext* context() const;
  void log(const QString& s);
  void poll();
  void finish();
  void stopRun(const QString& why);
  void updateButtons();
  bool updateDevice(const QString& device, const Gfx::WindowSettings& ws);
  bool pushValue(Process::ProcessModel* p, const QString& inlet, ossia::value v);
  void requestTap(Process::ProcessModel* p, const std::shared_ptr<OutletTap>& tap);
  ossia::value inletValue(Process::ProcessModel* p, const QString& inlet) const;
  std::optional<Gfx::WindowSettings> deviceSettings(const QString& name) const;
  double estimateMs() const;

  QPointer<score::Document> m_doc;
  QComboBox* m_device{};
  QLabel* m_info{};
  QComboBox* m_camera{};
  QComboBox* m_positions{};
  QComboBox* m_mode{};
  QLineEdit* m_name{};
  QSpinBox* m_settle{};
  QCheckBox* m_blend{};
  QCheckBox* m_photometric{};
  QPushButton* m_refreshBtn{};
  QPushButton* m_setupBtn{};
  QPushButton* m_startBtn{};
  QPushButton* m_abortBtn{};
  QProgressBar* m_progress{};
  QLabel* m_status{};
  QPlainTextEdit* m_log{};

  enum class Phase
  {
    Idle,
    Ready,
    Starting,
    Capturing,
    Blending,
    Done
  } m_phase{Phase::Idle};
  QPointer<Process::ProcessModel> m_slc;
  QPointer<Process::ProcessModel> m_blendProc;
  CalibrationSession m_session;
  CalibrationPlan m_plan;
  std::shared_ptr<OutletTap> m_slcTap, m_blendTap;
  int m_seenTap{};
  bool m_seenRunning{};
  qint64 m_runStartMs{};
  qint64 m_startMs{};
  qint64 m_blendMs{};
  qint64 m_lastChangeMs{};
  double m_estimateMs{};
  QTimer m_timer;
};

class CalibrationApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  using score::GUIApplicationPlugin::GUIApplicationPlugin;
  score::GUIElements makeGUIElements() override;
  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;
  void on_closeDocument(score::Document& doc) override;

private:
  QPointer<CalibrationWizard> m_wizard;
  QPointer<QAction> m_action;
};
}
