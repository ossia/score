#pragma once

#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <Scenario/Commands/CommandAPI.hpp>
#include <JS/Commands/ScriptMacro.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <QQmlContext>
#include <QJSEngine>
#include <wobjectimpl.h>

namespace JS
{

class EditJsContext
    : public QObject
{
  W_OBJECT(EditJsContext)
public:
  EditJsContext()
  {

  }

  QObject* document()
  {
     return score::GUIAppContext().documents.currentDocument();
  } W_SLOT(document)

  QObject* find(QString p)
  {
    auto doc = document();
    const auto meta = doc->findChildren<score::ModelMetadata*>();
    for(auto m : meta)
    {
      if(m->getName() == p)
      {
        return m->parent();
      }
    }
    return nullptr;
  } W_SLOT(find)

  void automate(QObject* interval, QString addr)
  {
    auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
    if(!itv)
      return;
    auto& ctx = score::GUIAppContext().documents.currentDocument()->context();
    Scenario::Command::Macro m{new ScriptMacro, ctx};
    m.automate(*itv, addr);
    m.commit();

  } W_SLOT(automate)
};

W_OBJECT_IMPL(EditJsContext)

class PanelDelegate final
    : public QObject
    , public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QWidget}
  {
    m_engine.globalObject().setProperty("Score", m_engine.newQObject(new EditJsContext));
    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);
    m_edit = new QPlainTextEdit{m_widget};
    m_edit->setTextInteractionFlags(Qt::TextBrowserInteraction);
    lay->addWidget(m_edit, 1);
    m_lineEdit = new QLineEdit{m_widget};
    lay->addWidget(m_lineEdit, 0);

    connect(m_lineEdit, &QLineEdit::editingFinished,
            this, [=] {
      auto txt = m_lineEdit->text();
      if(!txt.isEmpty())
      {
        m_edit->appendPlainText(">> " + txt);
        auto res = m_engine.evaluate(txt);
        if(res.isError())
        {
          m_edit->appendPlainText("ERROR: " + res.toString() + "\n");
        }
        else
        {
          m_edit->appendPlainText(res.toString() + "\n");
        }
        m_lineEdit->clear();
      }
    });
  }

private:
  QWidget* widget() override { return m_widget; }

  const score::PanelStatus& defaultPanelStatus() const override
  {
    static const score::PanelStatus status{false, Qt::BottomDockWidgetArea, 0,
                                           QObject::tr("Console"),
                                           QObject::tr("Ctrl+Shift+C")};

    return status;
  }

  QWidget* m_widget{};
  QPlainTextEdit* m_edit{};
  QLineEdit* m_lineEdit{};
  QJSEngine m_engine;
};

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("6060a63c-26b1-4ec6-a468-27e72530ac69")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {
    return std::make_unique<PanelDelegate>(ctx);
  }
};
}
