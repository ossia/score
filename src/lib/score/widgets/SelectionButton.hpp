#pragma once
#include <score/selection/Selection.hpp>

#include <QToolButton>
#include <QString>
#include <QWidget>

class QPushButton;

namespace score
{
class SelectionDispatcher;
}
class SCORE_LIB_BASE_EXPORT SelectionButton final : public QToolButton
{
public:
  SelectionButton(
      const QString& text,
      Selection target,
      score::SelectionDispatcher& disp,
      QWidget* parent);

  template <typename Obj>
  static SelectionButton* make(Obj* obj, score::SelectionDispatcher& disp, QWidget* parent)
  {
    auto ptr = const_cast<std::remove_const_t<Obj>*>(obj);
    return new SelectionButton{QString::number(*obj->id().val()), Selection{ptr}, disp, parent};
  }

  template <typename Obj>
  static SelectionButton*
  make(const QString& text, Obj* obj, score::SelectionDispatcher& disp, QWidget* parent)
  {
    auto ptr = const_cast<std::remove_const_t<Obj>*>(obj);
    auto but = new SelectionButton{text, Selection{ptr}, disp, parent};

    but->setToolTip(QString::number(obj->id().val()));
    return but;
  }

private:
  score::SelectionDispatcher& m_dispatcher;
};
