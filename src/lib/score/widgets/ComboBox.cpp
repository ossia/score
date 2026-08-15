#include "ComboBox.hpp"

#include <QAbstractItemView>
#include <QKeyEvent>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::ComboBoxWithEnter)

namespace score
{
ComboBoxWithEnter::ComboBoxWithEnter(QWidget* parent)
    : QComboBox{parent}
{
}

ComboBoxWithEnter::~ComboBoxWithEnter() = default;

bool ComboBoxWithEnter::event(QEvent* event)
{
  switch(event->type())
  {
    // Claim the keys, but act on the key press: handling both fires twice.
    case QEvent::ShortcutOverride: {
      auto* keyEvent = static_cast<QKeyEvent*>(event);
      switch(keyEvent->key())
      {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
          keyEvent->accept();
          return true;
        default:
          break;
      }
      break;
    }

    case QEvent::KeyPress: {
      auto* keyEvent = static_cast<QKeyEvent*>(event);
      // The drop-down reads the keyboard while it is up.
      if(view()->isVisible())
        break;

      switch(keyEvent->key())
      {
        case Qt::Key_Enter:
        case Qt::Key_Return:
          keyEvent->accept();
          editingFinished();
          return true;
        case Qt::Key_Escape:
          keyEvent->accept();
          editingCancelled();
          return true;
        default:
          break;
      }
      break;
    }

    case QEvent::FocusOut: {
      // The drop-down holds the focus while it is open.
      if(!view()->isVisible())
        editingFinished();
      break;
    }

    default:
      break;
  }

  return QComboBox::event(event);
}
}
