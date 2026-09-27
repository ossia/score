#pragma once
#include <QEvent>
#include <QPointer>
#include <QWidget>

#include <functional>

namespace score
{
//! Runs `f` once, at the first press, wheel or touch the user gives `widget`
//! (a graphics view's viewport, say), then goes away. For what a view does on
//! its own only until the user starts to act in it. Owned by `context`: gone
//! with it, and `f` with it.
class FirstUserInput final : public QObject
{
public:
  FirstUserInput(QWidget* widget, QObject* context, std::function<void()> f)
      : QObject{context}
      , m_widget{widget}
      , m_f{std::move(f)}
  {
    widget->installEventFilter(this);
  }

  ~FirstUserInput() override
  {
    if(m_widget)
      m_widget->removeEventFilter(this);
  }

private:
  bool eventFilter(QObject*, QEvent* event) override
  {
    switch(event->type())
    {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonDblClick:
      case QEvent::Wheel:
      case QEvent::TabletPress:
      case QEvent::TouchBegin:
        if(auto f = std::exchange(m_f, {}))
          f();
        deleteLater();
        break;
      default:
        break;
    }
    return false;
  }

  QPointer<QWidget> m_widget;
  std::function<void()> m_f;
};
}
