#pragma once
#include <Threedim/CalibrationPlan.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QDialog>
#include <QPointer>
#include <QSize>
#include <QStringList>
#include <QTimer>

#include <ossia/network/value/value.hpp>

#include <vector>

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

  void refreshDevices();
  bool setUp();
  void start();
  void abort();

private:
  const score::DocumentContext* context() const;
  void log(const QString& s);
  void poll();
  void finish();
  void restoreDevice();
  bool updateDevice(const Gfx::WindowSettings& ws);
  bool pushValue(Process::ProcessModel* p, const QString& inlet, ossia::value v);
  std::optional<Gfx::WindowSettings> deviceSettings(const QString& name) const;

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
  QPushButton* m_setupBtn{};
  QPushButton* m_startBtn{};
  QPushButton* m_abortBtn{};
  QProgressBar* m_progress{};
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
  QString m_deviceName;
  std::optional<Gfx::WindowSettings> m_original;
  CalibrationPlan m_plan;
  qint64 m_startMs{};
  qint64 m_blendMs{};
  qint64 m_lastSave{};
  double m_estimateMs{};
  QTimer m_timer;
};

class CalibrationApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  using score::GUIApplicationPlugin::GUIApplicationPlugin;
  score::GUIElements makeGUIElements() override;

private:
  QPointer<CalibrationWizard> m_wizard;
};
}
