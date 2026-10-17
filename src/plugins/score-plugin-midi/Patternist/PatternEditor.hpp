#pragma once
#include <QPointer>
#include <QWidget>

#include <vector>

class QGraphicsScene;
class QGraphicsView;
class QHBoxLayout;
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
class Editor final : public QWidget
{
public:
  Editor(ProcessModel& model, const score::DocumentContext& ctx, QWidget* parent);
  ~Editor() override;

private:
  void resizeEvent(QResizeEvent* event) override;
  void updateBank();
  void updatePatternControls();
  void fitGrid();

  QPointer<ProcessModel> m_model;
  Controller* m_controller{};

  QHBoxLayout* m_bank{};
  std::vector<QToolButton*> m_bankButtons;
  QToolButton* m_removePattern{};
  QSpinBox* m_steps{};
  QSpinBox* m_rate{};
  int m_playing{-1};
  int m_pending{-1};

  QGraphicsScene* m_scene{};
  QGraphicsView* m_graphics{};
  View* m_view{};
};
}
