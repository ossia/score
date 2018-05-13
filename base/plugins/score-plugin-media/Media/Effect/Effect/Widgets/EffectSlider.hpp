#pragma once
#include <QWidget>
#include <wobjectdefs.h>
#include <State/Address.hpp>
namespace Process
{
class ControlInlet;
}
namespace score
{
class DoubleSlider;
}

namespace Media
{
namespace Effect
{
class EffectSlider : public QWidget
{
  W_OBJECT(EffectSlider)
public:
  EffectSlider(Process::ControlInlet& fx, bool is_output, QWidget* parent);

  ~EffectSlider() override;

  double scaledValue;
  score::DoubleSlider* m_slider{};

public:
  void createAutomation(const State::Address& arg_1, double min, double max) W_SIGNAL(createAutomation, arg_1, min, max);

private:
  void contextMenuEvent(QContextMenuEvent* event) override;
  void on_paramDeleted();

  Process::ControlInlet& m_param;
  float m_min{0.};
  float m_max{1.};

  QAction* m_addAutomAction{};
};
}
}
