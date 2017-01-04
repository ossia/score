#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <QLabel>
#include <algorithm>

#include "JS/Commands/EditScript.hpp"
#include "JSInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <QVBoxLayout>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/model/path/Path.hpp>

#include <iscore/widgets/JS/JSEdit.hpp>
class QVBoxLayout;
class QWidget;
namespace iscore
{
class Document;
} // namespace iscore
namespace JS
{
template<typename Widg, typename T>
void JSWidgetBase::init(Widg* self, T& model)
{
  m_edit = new JSEdit;
  m_edit->setPlainText(model.script());

  m_errorLabel = new QLabel{self};

  con(model, &T::scriptError,
      self, [=] (int line, const QString& err){
    m_edit->setError(line);
    m_errorLabel->setText(err);
    m_errorLabel->setVisible(true);
  });
  con(model, &T::scriptOk,
      self, [=] (){
    m_edit->clearError();
    m_errorLabel->setText("");
    m_errorLabel->setVisible(false);
  });
  con(model, &T::scriptChanged,
      self, [=] (const QString& str){ on_modelChanged(str);});

  QObject::connect(
        m_edit, &JSEdit::focused,
        self, &Widg::pressed);

  QObject::connect(
        m_edit, &JSEdit::editingFinished,
        self, &Widg::on_textChange);


  on_modelChanged(model.script());
  m_script = m_edit->toPlainText();
}

void JSWidgetBase::on_modelChanged(const QString& script)
{
  m_script = script;
  auto cur = m_edit->textCursor().position();

  m_edit->setPlainText(script);
  if(cur < m_script.size())
  {
    auto c = m_edit->textCursor();
    c.setPosition(cur);
    m_edit->setTextCursor(std::move(c));
  }
}

InspectorWidget::InspectorWidget(
    const JS::ProcessModel& JSModel,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{JSModel, parent}
    , JSWidgetBase{doc.commandStack}
{
  setObjectName("JSInspectorWidget");
  setParent(parent);
  auto lay = new QVBoxLayout;


  this->init(this, JSModel);

  lay->addWidget(m_edit);
  lay->addWidget(m_errorLabel);
  this->setLayout(lay);
}


void InspectorWidget::on_textChange(const QString& newTxt)
{
  if (newTxt == m_script)
    return;

  auto cmd = new JS::EditScript{process(), newTxt};

  m_dispatcher.submitCommand(cmd);
}

StateInspectorWidget::StateInspectorWidget(
    const JS::StateProcess& JSModel,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : StateProcessInspectorWidgetDelegate_T{JSModel, parent}
    , JSWidgetBase{doc.commandStack}
{
  setObjectName("JSInspectorWidget");
  setParent(parent);
  auto lay = new QVBoxLayout;

  this->init(this, JSModel);

  lay->addWidget(m_edit);
  lay->addWidget(m_errorLabel);
  this->setLayout(lay);
}


void StateInspectorWidget::on_textChange(const QString& newTxt)
{
  if (newTxt == m_script)
    return;

  auto cmd = new JS::EditStateScript{process(), newTxt};

  m_dispatcher.submitCommand(cmd);
}
}
