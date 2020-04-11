// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSInspectorWidget.hpp"

#include "JS/Commands/EditScript.hpp"

#include <Inspector/InspectorWidgetBase.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/Path.hpp>
#include <score/widgets/JS/JSEdit.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/tools/Bind.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::InspectorWidget)
class QVBoxLayout;
namespace JS
{
template <typename Widg, typename T>
void JSWidgetBase::init(Widg* self, T& model)
{
  m_edit = new JSEdit;
  m_edit->setPlainText(model.script());

  m_errorLabel = new QLabel{self};

  con(model, &T::errorMessage, self, [=](int line, const QString& err) {
    m_edit->setError(line);
    m_errorLabel->setText(err);
    m_errorLabel->setVisible(true);
  });
  con(model, &T::scriptOk, self, [=]() {
    m_edit->clearError();
    m_errorLabel->clear();
    m_errorLabel->setVisible(false);
  });
  con(model, &T::scriptChanged, self, [=](const QString& str) {
    on_modelChanged(str);
  });

  QObject::connect(m_edit, &JSEdit::focused, self, &Widg::pressed);

  QObject::connect(
      m_edit, &JSEdit::editingFinished, self, &Widg::on_textChange);

  on_modelChanged(model.script());
  m_script = m_edit->toPlainText();
}

void JSWidgetBase::on_modelChanged(const QString& script)
{
  m_script = script;
  auto cur = m_edit->textCursor().position();

  m_edit->setPlainText(script);
  if (cur < m_script.size())
  {
    auto c = m_edit->textCursor();
    c.setPosition(cur);
    m_edit->setTextCursor(std::move(c));
  }
}

InspectorWidget::InspectorWidget(
    const JS::ProcessModel& JSModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{JSModel, parent}
    , JSWidgetBase{doc.commandStack}
{
  setObjectName("JSInspectorWidget");
  setParent(parent);
  auto lay = new score::MarginLess<QVBoxLayout>{this};

  this->init(this, JSModel);

  lay->addWidget(m_edit);
  lay->addWidget(m_errorLabel);

  updateControls(doc);

  con(JSModel, &JS::ProcessModel::qmlDataChanged, this, [&] {
    updateControls(doc);
  });
}
void InspectorWidget::updateControls(const score::DocumentContext& doc)
{
  delete m_ctrlWidg;
  m_ctrlWidg = nullptr;

  auto& proc = process();
  if (!proc.m_dummyObject)
    return;

  m_ctrlWidg = new QWidget;
  auto clay = new QFormLayout{m_ctrlWidg};
  this->layout()->addWidget(m_ctrlWidg);

  {
    auto cld_inlet = proc.m_dummyObject->findChildren<Inlet*>();
    int i = 0;
    auto get_control = [&](int i) -> Process::ControlInlet& {
      return *static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    };
    for (auto ctrl : cld_inlet)
    {
      if (auto fslider = qobject_cast<FloatSlider*>(ctrl))
      {
        clay->addRow(
            ctrl->objectName(),
            WidgetFactory::FloatSlider::make_widget(
                *fslider, get_control(i), doc, this, this));
      }
      else if (auto islider = qobject_cast<IntSlider*>(ctrl))
      {
        clay->addRow(
            ctrl->objectName(),
            WidgetFactory::IntSlider::make_widget(
                *islider, get_control(i), doc, this, this));
      }
      else if (auto toggle = qobject_cast<Toggle*>(ctrl))
      {
        clay->addRow(
            ctrl->objectName(),
            WidgetFactory::Toggle::make_widget(
                *toggle, get_control(i), doc, this, this));
      }
      else if (auto edit = qobject_cast<LineEdit*>(ctrl))
      {
        clay->addRow(
            ctrl->objectName(),
            WidgetFactory::LineEdit::make_widget(
                *edit, get_control(i), doc, this, this));
      }
      else if (auto en = qobject_cast<Enum*>(ctrl))
      {
        clay->addRow(
            ctrl->objectName(),
            WidgetFactory::Enum::make_widget(
                *en, get_control(i), doc, this, this));
      }

      i++;
    }
  }
}

void InspectorWidget::on_textChange(const QString& newTxt)
{
  if (newTxt == m_script)
    return;

  auto cmd = new score::StaticPropertyCommand<JS::ProcessModel::p_script>{process(), newTxt};

  m_dispatcher.submit(cmd);
}
}
