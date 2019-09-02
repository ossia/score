#include "DoubleSpinBox.hpp"

#include <QKeyEvent>

bool score::DoubleSpinboxWithEnter::event(QEvent* event)
{
  switch (event->type())
  {
    case QEvent::ShortcutOverride:
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      switch (keyEvent->key())
      {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
          editingFinished();
          keyEvent->accept();
          break;
        default:
          break;
      }
      break;
    }

    case QEvent::KeyPress:
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      switch (keyEvent->key())
      {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
          editingFinished();
          keyEvent->accept();
          break;
        default:
          return QDoubleSpinBox::event(event);
      }
    }

    case QEvent::FocusOut:
    {
      editingFinished();
      break;
    }

    default:
      break;
  }

  return QDoubleSpinBox::event(event);
}
