#include "ConsolePanel.hpp"

#include <JS/ApplicationPlugin.hpp>

#include <score/widgets/HelpInteraction.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QJSValueIterator>
#include <QQmlEngine>

namespace JS
{

PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_engine{ctx.guiApplicationPlugin<JS::ApplicationPlugin>().m_engine}
    , m_widget{new QWidget}
{
  m_engine.installExtensions(QJSEngine::ConsoleExtension);
  m_engine.globalObject().setProperty(
      "ActionContext", m_engine.newQObject(new ActionContext));

  auto lay = new QVBoxLayout;
  m_widget->setLayout(lay);
  score::setHelp(m_widget, 
      QObject::tr("This panel prompts the scripting console \n"
                  "still in early development"));

  m_edit = new QPlainTextEdit{m_widget};
  m_edit->setTextInteractionFlags(Qt::TextEditorInteraction);

  lay->addWidget(m_edit, 1);
  m_lineEdit = new QLineEdit{m_widget};
  lay->addWidget(m_lineEdit, 0);

  m_edit->appendPlainText(
      "Welcome to the ossia score scripting console!\n"
      "The console provides a JavaScript ES7 environment.\n"
      "Read more about the avaiable functions here:\n"
      "-> https://ossia.io/score-docs/panels/console.html\n\n");

  // TODO ctrl-space !
  connect(m_lineEdit, &QLineEdit::editingFinished, this, [this] {
    auto txt = m_lineEdit->text();
    if(!txt.isEmpty())
    {
      evaluate(txt);
      m_lineEdit->clear();
      m_edit->verticalScrollBar()->setValue(m_edit->verticalScrollBar()->maximum());
    }
  });
}

QJSEngine& PanelDelegate::engine() noexcept
{
  return m_engine;
}

void PanelDelegate::evaluate(const QString& txt)
{
  m_edit->appendPlainText("> " + txt);
  auto res = m_engine.evaluate(txt);
  if(res.isError())
  {
    m_edit->appendPlainText("ERROR: " + res.toString() + "\n");
  }
  else if(!res.isUndefined())
  {
    m_edit->appendPlainText(res.toString() + "\n");
  }
}

void PanelDelegate::compute(const QString& txt, std::function<void(QVariant)> r)
{
  m_edit->appendPlainText("> " + txt);
  auto res = m_engine.evaluate(txt);
  if(res.isError())
  {
    m_edit->appendPlainText("ERROR: " + res.toString() + "\n");
  }
  else if(!res.isUndefined())
  {
    if(r)
      r(res.toVariant());
  }
  else
  {
    qDebug() << res.toString();
  }
}

QMenu* PanelDelegate::addMenu(QMenu* cur, QStringList names)
{
  QMenu* newmenu{};

  for(int i = 0; i < names.size() - 1; i++)
  {
    for(auto act : cur->findChildren<QMenu*>(QString{}, Qt::FindDirectChildrenOnly))
    {
      if(act->title() == names[i])
      {
        cur = act;
        goto ok;
      }
    }

    newmenu = new QMenu{names[i], cur};
    cur->addMenu(newmenu);
    cur = newmenu;

  ok:
    continue;
  }
  return cur;
}

void PanelDelegate::importModule(const QString& path)
{
  QJSValue mod = m_engine.importModule(path);
  if(auto init = mod.property("initialize"); init.isCallable())
    init.call();
  if(auto init = mod.property("actions"); init.isArray())
  {
    QJSValueIterator it(init);
    while(it.hasNext())
    {
      if(it.next())
      {
        auto obj = it.value();
        if(obj.isObject())
        {
          auto name = obj.property("name");
          auto context = obj.property("context");
          auto action = obj.property("action");
          auto shortcut = obj.property("shortcut");
          if(!name.isString())
            return;
          if(context.toString() != "Menu")
            return;
          if(!action.isCallable())
            return;

          auto names = name.toString().split('/');
          if(names.empty())
            return;

          score::Menu& script = this->context().menus.get().at(score::Menus::Scripts());
          if(auto act = script.menu()->actions()[0];
             act->objectName() == "DefaultScriptMenuAction")
            script.menu()->removeAction(act);

          auto menu = addMenu(script.menu(), names);

          auto act = new QAction{names.back(), this};
          if(auto str = shortcut.toString(); !str.isEmpty())
          {
            act->setShortcut(QKeySequence::fromString(str));
          }
          connect(
              act, &QAction::triggered, this, [action = std::move(action)]() mutable {
                auto res = action.call();
                if(res.isError())
                {
                  qDebug() << "Script error: " << res.errorType() << res.toString();
                }
              });
          menu->addAction(act);
        }
      }
    }
  }

  auto obj = m_engine.globalObject();
  obj.setProperty(QFileInfo{path}.baseName(), mod);
  if(mod.isString() || mod.isError())
    m_edit->appendPlainText(mod.toString());
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false,
      false,
      Qt::BottomDockWidgetArea,
      0,
      QObject::tr("Console"),
      "console",
      QObject::tr("Ctrl+Shift+C")};

  return status;
}
}
