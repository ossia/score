#include "SearchReplaceWidget.hpp"

#include <State/Address.hpp>
#include <State/Expression.hpp>

#include <Process/Commands/EditPort.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/ReplaceAddresses.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/TimeSync/SetTrigger.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Process/ScenarioSelection.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

namespace Scenario
{
SearchReplaceWidget::SearchReplaceWidget(const score::GUIApplicationContext& ctx)
    : QDialog{nullptr}
    , m_ctx{ctx}
{
  setAttribute(Qt::WA_DeleteOnClose);
  QFormLayout* form = new QFormLayout();
  this->setLayout(form);
  QLineEdit* findLine = new QLineEdit();
  QLineEdit* replaceLine = new QLineEdit();
  QListWidget* resultList = new QListWidget();
  QPushButton* findButton = new QPushButton("Find");
  QPushButton* replaceButton = new QPushButton("Replace");
  form->addRow("Find", findLine);
  form->addRow("Results", resultList);
  form->addRow("Replace", replaceLine);
  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->addWidget(findButton);
  buttons->addWidget(replaceButton);
  form->addRow(buttons);

  connect(
      findLine, &QLineEdit::editingFinished, [=] { setFindTarget(findLine->text()); });
  connect(findButton, &QPushButton::pressed, [=] {
    search();
    resultList->clear();
    for(const auto o : m_matches)
    {
      resultList->addItem(getObjectName(o));
    }
    //  Selection s
    for(int i = 0; i < resultList->count(); i++)
    {
      auto item = resultList->item(i);
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    }
  });

  connect(replaceLine, &QLineEdit::editingFinished, [=]() {
    setReplaceTarget(replaceLine->text());
  });
  connect(replaceButton, &QPushButton::pressed, [=]() { replace(); });
}
void SearchReplaceWidget::setFindTarget(const QString& fTarget)
{
  if(auto addr = State::parseAddress(fTarget))
  {
    m_oldAddress = addr.value();
  }
}

void SearchReplaceWidget::setReplaceTarget(const QString& rTarget)
{
  if(auto addr = State::parseAddress(rTarget))
  {
    m_newAddress = addr.value();
  }
}

void SearchReplaceWidget::search()
{
  m_matches = findByAddress(m_ctx.documents.currentDocument()->context(), m_oldAddress);
}

void SearchReplaceWidget::replace()
{
  auto& ctx = m_ctx.documents.currentDocument()->context();
  Scenario::Command::Macro m{new Command::ReplaceAddresses, ctx};
  m.findAndReplace(m_matches, m_oldAddress, m_newAddress);
  m.commit();
}

QString SearchReplaceWidget::getObjectName(const QObject* o)
{
  if(auto state = qobject_cast<const StateModel*>(o))
  {
    //TODO - left

    return QString{state->metadata().getName()};
  }
  else if(auto event = qobject_cast<const EventModel*>(o))
  {
    //event condition
    return QString{event->metadata().getName()};
  }
  else if(auto ts = qobject_cast<const TimeSyncModel*>(o))
  {
    //ts expression()
    return QString{ts->metadata().getName()};
  }

  else if(auto inlet = qobject_cast<const Process::Inlet*>(o))
  {
    //address directement
    auto a = score::IDocument::unsafe_path(o);
    QString process = "process";
    auto m = Process::parentProcess(o);
    if(m)
      process = m->prettyName();

    QString portName = inlet->name();
    QString res;
    res += portName;
    res += " (";
    res += process;
    res += " - Input)";
    return res;
  }
  else if(auto outlet = qobject_cast<const Process::Outlet*>(o))
  {
    auto a = score::IDocument::unsafe_path(o);
    QString process = "process";
    auto m = Process::parentProcess(o);
    if(m)
      process = m->prettyName();

    QString portName = outlet->name();
    QString res;
    res += portName;
    res += " (";
    res += process;
    res += " - Output)";
    return res;
  }
  return {};
}

}
