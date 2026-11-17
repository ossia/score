#pragma once
#include <Gfx/WindowDevice.hpp>
#include <Threedim/CalibrationPlan.hpp>
#include <Threedim/CalibrationRun.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QDialog>
#include <QPointer>
#include <QTimer>

#include <array>
#include <memory>
#include <optional>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
namespace score
{
class Document;
struct DocumentContext;
}
namespace ossia::net
{
class parameter_base;
}

namespace Threedim
{
//! Calibrates the projectors of a multi-window device with a camera, as a
//! tool of its own: the patterns go straight to the device's windows and the
//! camera is read without playing the score or adding anything to it. At the
//! end, each output follows its calibration (and the blend maps).
class CalibrationWizard final : public QDialog
{
public:
  CalibrationWizard(score::Document& doc, QWidget* parent = nullptr);
  ~CalibrationWizard() override;

  const score::Document* document() const noexcept { return m_doc; }
  bool busy() const noexcept { return m_run && m_run->running(); }

  void refreshDevices();
  //! recompute: from the saved measurements, without capturing.
  bool start(bool recompute = false);
  void abort();
  //! The document is going away: stop without touching it.
  void detach();

  void done(int r) override;

  //! The running node of a multi-window device, or null.
  static score::gfx::MultiWindowNode*
  multiWindowNode(const score::DocumentContext& ctx, const QString& device);
  //! The parameter at `address` ("Device:/path"), or null.
  static ossia::net::parameter_base*
  parameterAt(const score::DocumentContext& ctx, const QString& address);

private:
  const score::DocumentContext* context() const;
  void log(const QString& s);
  void poll();
  void finish();
  void updateButtons();
  void updatePlan();
  bool updateDevice(const QString& device, const Gfx::WindowSettings& ws);
  std::optional<Gfx::WindowSettings> deviceSettings(const QString& name) const;
  double estimateMs(const CalibrationPlan& plan, const StructuredLightSession::Settings& s) const;
  //! The plan of the device at its windows' physical pixel size.
  CalibrationPlan livePlan(const Gfx::WindowSettings& ws, QStringList* notes = nullptr) const;
  StructuredLightSession::Settings sessionSettings() const;
  void loadOptions();
  void saveOptions() const;

  QPointer<score::Document> m_doc;
  QComboBox* m_device{};
  QLabel* m_info{};
  QComboBox* m_camera{};
  QComboBox* m_positions{};
  QComboBox* m_mode{};
  QLineEdit* m_name{};
  QLineEdit* m_outputs{};
  QSpinBox* m_settle{};
  QDoubleSpinBox* m_fov{};
  QComboBox* m_lens{};
  QLineEdit* m_lensK{};
  QCheckBox* m_estimateLens{};
  QDoubleSpinBox* m_level{};
  QSpinBox* m_stripe{};
  QSpinBox* m_frames{};
  QDoubleSpinBox* m_exposure{};
  QCheckBox* m_blend{};
  QComboBox* m_display{};
  QCheckBox* m_flat{};
  QCheckBox* m_black{};
  QComboBox* m_geometry{};
  std::array<QDoubleSpinBox*, 6> m_box{};
  QLineEdit* m_model{};
  std::array<QDoubleSpinBox*, 3> m_camPos{};
  std::array<QDoubleSpinBox*, 3> m_camRot{};
  QCheckBox* m_refinePose{};
  QWidget* m_boxRow{};
  QWidget* m_modelRow{};
  QWidget* m_poseRow{};
  void updateGeometryRows();
  QCheckBox* m_photometric{};
  QPushButton* m_refreshBtn{};
  QPushButton* m_startBtn{};
  QPushButton* m_abortBtn{};
  QPushButton* m_recomputeBtn{};
  QProgressBar* m_progress{};
  QLabel* m_status{};
  QPlainTextEdit* m_log{};

  std::unique_ptr<CalibrationRun> m_run;
  QString m_runDevice;
  qint64 m_runStartMs{};
  bool m_recomputing{};
  double m_estimateMs{};
  QTimer m_timer;
  //! Camera devices this dialog opened while the score was stopped: kept
  //! open between runs, closed with the dialog.
  QStringList m_openedCameras;
  void closeCameras();
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
