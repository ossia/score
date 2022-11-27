#include "SearchReplaceWidget.hpp"

#include <State/Address.hpp>
#include <State/Expression.hpp>

#include <Process/Commands/EditPort.hpp>

#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/ReplaceAddresses.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/TimeSync/SetTrigger.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>

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
    //target valid
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
  if(!m_oldAddress.isSet())
  {
    return;
  }
  else
  {
    m_matches.clear();
    std::vector<State::AddressAccessor> addresses;
    addresses.push_back(State::AddressAccessor{m_oldAddress});
    auto* doc = m_ctx.documents.currentDocument();

    QList<QObject*> itemsToSearch;
    if(auto selection = doc->selectionStack().currentSelection(); !selection.empty())
    {
      for(QObject* ptr : selection)
      {
        itemsToSearch.push_back(ptr);
        itemsToSearch.append(
            ptr->findChildren<QObject*>(QString{}, Qt::FindChildrenRecursively));
      }
    }
    else
    {
      itemsToSearch
          = doc->findChildren<QObject*>(QString{}, Qt::FindChildrenRecursively);
    }
    ossia::remove_duplicates(itemsToSearch);
    if(itemsToSearch.empty())
      return;

    Selection sel{};

    {
      State::MessageList listCache;
      std::vector<QString> messagesCache;
      // Serialize ALL the things
      for(const auto& obj : itemsToSearch)
      {
        if(auto state = qobject_cast<const StateModel*>(obj))
        {
          auto& root = state->messages().rootNode();

          // First look for addresses containing the looked-up address
          bool must_add
              = Process::hasMatchingAddress(root, addresses, listCache, messagesCache);
          listCache.clear();
          messagesCache.clear();

          // If not found, then look for addresses containing the raw string
          //          if(!must_add)
          //            must_add = Process::hasMatchingText(root, stxt, listCache, messagesCache);
          // FIXME look into state processes?

          // Try to add if the searched text is in the name of the state
          if(must_add)
            m_matches.push_back(obj);
        }
        else if(auto event = qobject_cast<const EventModel*>(obj))
        {
          if(State::findAddressInExpression(event->condition(), m_oldAddress))
          {
            m_matches.push_back(obj);
            sel.append(*event);
          }
        }
        else if(auto ts = qobject_cast<const TimeSyncModel*>(obj))
        {
          if(State::findAddressInExpression(ts->expression(), m_oldAddress))
            m_matches.push_back(obj);
          sel.append(*ts);
        }
        else if(auto port = qobject_cast<Process::Inlet*>(obj))
        {
          if(State::addressIsChildOf(m_oldAddress, port->address().address))
          {
            m_matches.push_back(port);
          }
        }
        else if(auto port = qobject_cast<Process::Outlet*>(obj))
        {
          if(State::addressIsChildOf(m_oldAddress, port->address().address))
          {
            m_matches.push_back(port);
          }
        }
        /*
        else if(auto cmt = qobject_cast<const CommentBlockModel*>(obj))
        {
        }
        else if(auto interval = qobject_cast<const IntervalModel*>(obj))
        {
        }
        */
      }
    }
    ossia::remove_duplicates(m_matches);
    // score::SelectionDispatcher d{doc->context().selectionStack};
    // d.select(sel);
  }
}

void SearchReplaceWidget::replace()
{
  auto& ctx = m_ctx.documents.currentDocument()->context();
  MacroCommandDispatcher<Command::ReplaceAddresses> disp{ctx.commandStack};
  if(!m_newAddress.isSet() || m_newAddress.path.isEmpty())
    return;

  for(auto* obj : m_matches)
  {
    if(auto state = qobject_cast<const StateModel*>(obj))
    {
      disp.submit(
          new Command::RenameAddressesInState(*state, m_oldAddress, m_newAddress));
    }
    else if(auto event = qobject_cast<EventModel*>(obj))
    {
      auto expr = event->condition();
      State::replaceAddress(expr, m_oldAddress, m_newAddress);
      disp.submit(new Command::SetCondition(*event, std::move(expr)));
    }
    else if(auto ts = qobject_cast<TimeSyncModel*>(obj))
    {
      auto expr = ts->expression();
      State::replaceAddress(expr, m_oldAddress, m_newAddress);
      disp.submit(new Command::SetTrigger(*ts, expr));
    }

    else if(auto port = qobject_cast<Process::Port*>(obj))
    {
      auto addr = port->address();
      State::rerootAddress(addr.address, m_oldAddress, m_newAddress);
      disp.submit(new Process::ChangePortAddress(*port, addr));
    }
  }

  disp.commit();
}

QString SearchReplaceWidget::getObjectName(const QObject* o)
{
  auto& ctx = m_ctx.documents.currentDocument()->context();

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
    res += " ( ";
    res += process;
    res += " - Input )";
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
    res += " ( ";
    res += process;
    res += " - Output )";
    return res;
  }
  return {};
}

}
