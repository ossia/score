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
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
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
  m_tableWidget = new QTableWidget{self};

  m_tableWidget->insertColumn(0);
  m_tableWidget->insertColumn(1);
  m_tableWidget->setColumnCount(2);
  m_tableWidget->setRowCount(model.customProperties().size());
  m_tableWidget->horizontalHeader()->hide();
  m_tableWidget->verticalHeader()->hide();
  m_tableWidget->setAlternatingRowColors(true);
  int i = 0;
  for(const auto& e : model.customProperties())
  {
    auto key = new QTableWidgetItem(QString(e.first));
    m_tableWidget->setItem(i, 0, key);
    auto val = new QTableWidgetItem(e.second.toString());
    m_tableWidget->setItem(i, 1, val);
    i++;
  }

  con(model, &T::scriptError,
      self, [=] (int line, const QString& err){
    m_edit->setError(line);
    m_errorLabel->setText(err);
    m_errorLabel->setVisible(true);
  });
  con(model, &T::scriptOk,
      self, [=] (){
    m_edit->clearError();
    m_errorLabel->clear();
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
  if(parent) // So that it is not shown in the process
    lay->addWidget(m_tableWidget);
  else
  {
    delete m_tableWidget;
    m_tableWidget = nullptr;
  }
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
