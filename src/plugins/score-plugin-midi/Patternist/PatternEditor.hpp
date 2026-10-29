#pragma once
#include <score_plugin_midi_export.h>
#include <QPointer>
#include <QWidget>

#include <array>
#include <vector>

class QGraphicsScene;
class QGraphicsView;
class QAction;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QSpinBox;
class QToolButton;
namespace score
{
struct DocumentContext;
}
namespace Patternist
{
class ProcessModel;
class View;
class Controller;

//! The full editor: the pattern bank, the pattern tools and the grid with mute
//! and solo per lane. Opens as the process's UI, in a window or docked.
class SCORE_PLUGIN_MIDI_TEST_EXPORT Editor final : public QWidget
{
public:
  Editor(ProcessModel& model, const score::DocumentContext& ctx, QWidget* parent);
  ~Editor() override;

private:
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  QWidget* makeLanePanel();
  void updateLanePanel();
  void showStepMenu(int lane, int step, QPoint screen);
  void updateBank();
  void updatePatternControls();
  void fitGrid();
  //! The selected steps when there are some, the whole pattern otherwise.
  void setRings(bool b);
  void copy();
  void paste();
  void clear();
  void addShortcuts();

  QPointer<ProcessModel> m_model;
  Controller* m_controller{};

  QHBoxLayout* m_bank{};
  std::vector<QToolButton*> m_bankButtons;
  QToolButton* m_removePattern{};
  QToolButton* m_paste{};
  QLabel* m_status{};
  QToolButton* m_select{};

  QSpinBox* m_laneLength{};
  QSpinBox* m_laneDivision{};
  QSpinBox* m_laneVelocity{};
  QCheckBox* m_perHit[4]{};
  std::array<QWidget*, 2> m_perHitRows{};
  QFormLayout* m_laneForm{};
  QAction* m_addAccent{};
  QAction* m_addSlide{};
  QSpinBox* m_pulses{};
  QSpinBox* m_rotation{};
  QComboBox* m_combine{};
  QSpinBox* m_steps{};
  QSpinBox* m_rate{};
  int m_playing{-1};
  int m_pending{-1};

  QGraphicsScene* m_scene{};
  QGraphicsView* m_graphics{};
  View* m_view{};
};
}
