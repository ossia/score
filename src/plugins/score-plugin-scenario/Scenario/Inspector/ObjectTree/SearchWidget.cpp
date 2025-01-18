#include "SearchWidget.hpp"
// SearchWidget
#include <State/Expression.hpp>
#include <State/MessageListSerialization.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/OfflineAction/OfflineAction.hpp>

#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Inspector/ObjectTree/SearchReplaceWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/ArrowButton.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia-qt/invoke.hpp>

namespace Scenario
{

SearchWidget::SearchWidget(const score::GUIApplicationContext& ctx)
    : score::SearchLineEdit{nullptr}
    , m_ctx{ctx}
{
  setAcceptDrops(true);
  ossia::qt::run_async(this, [this] {
    if(auto widget = Explorer::findDeviceExplorerWidgetInstance(score::GUIAppContext()))
    {
      connect(
          widget, &Explorer::DeviceExplorerWidget::findAddresses, this,
          &SearchWidget::on_findAddresses);
    }
  });

  if(auto acts = this->actions(); !acts.empty())
  {
    auto a = acts.back();
    this->removeAction(a);
    delete a;
  }

  auto act = new QAction{this};
  act->setIcon(QIcon(":/icons/search.png"));
  score::setHelp(act, tr("Find And Replace"));
  addAction(act, QLineEdit::TrailingPosition);
  connect(act, &QAction::triggered, this, [this] {
    auto sr = new SearchReplaceWidget(m_ctx);
    auto txt = this->text();
    if(txt.startsWith("address="))
      txt.remove("address=");
    sr->setFindTarget(txt);
    sr->show();
  });
}

void SearchWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if(formats.contains(score::mime::messagelist()))
  {
    event->accept();
  }
}

void SearchWidget::on_findAddresses(QStringList strlst)
{
  QString searchTxt = "address=";
  if(!strlst.empty())
    searchTxt += strlst.first();

  for(int i = 1; i < strlst.size(); i++)
  {
    searchTxt += ",";
    searchTxt += strlst[i];
  }
  setText(searchTxt);
  search();
}

void SearchWidget::dropEvent(QDropEvent* ev)
{
  auto& mime = *ev->mimeData();

  // TODO refactor this with AutomationPresenter and AddressLineEdit
  if(mime.formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();
    if(nl.empty())
      return;

    // We only take the first node.
    const Device::Node& node = nl.front().second;
    // TODO refactor with CreateCurves and AutomationDropHandle
    if(node.is<Device::AddressSettings>())
    {
      const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
      Device::FullAddressSettings as;
      static_cast<Device::AddressSettingsCommon&>(as) = addr;
      as.address = nl.front().first;

      setText(as.address.toString());
      returnPressed();
    }
  }
  else if(mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if(!ml.empty())
    {
      setText(ml[0].address.toString());
      returnPressed();
    }
  }
}

template <typename T>
[[nodiscard]]
bool add_if_contains(const T& o, const QString& str, Selection& sel)
{
  const auto& obj = o.metadata();
  if(obj.getName().contains(str) || obj.getComment().contains(str)
     || obj.getLabel().contains(str))
  {
    sel.append(o);
    return true;
  }
  return false;
}
[[nodiscard]]
bool add_if_contains(
    const Scenario::CommentBlockModel& o, const QString& str, Selection& sel)
{
  if(o.content().contains(str))
  {
    sel.append(o);
    return true;
  }
  return false;
}

static void selectProcessPortsWithAddress(
    const std::vector<State::AddressAccessor>& addresses,
    const Process::ProcessModel& proc, Selection& sel)
{
  auto search = [&sel, &addresses](auto& port) {
    auto& port_addr = port.address();

    for(auto& search_addr : addresses)
    {
      if(port_addr.address == search_addr.address)
      {
        sel.append(port);
        break;
      }
    }

    port.forChildInlets([&](Process::Inlet& p) {
      for(auto& search_addr : addresses)
      {
        if(p.address().address == search_addr.address)
        {
          sel.append(&p);
          break;
        }
      }
    });
  };

  for(const Process::Inlet* port : proc.inlets())
  {
    search(*port);
  }

  for(const Process::Outlet* port : proc.outlets())
  {
    search(*port);
  }
}

static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::ProcessModel& proc, Selection& sel);
static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Process::ProcessModel& proc, Selection& sel);
static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::IntervalModel& interval, Selection& sel);
static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::IntervalModel& interval, Selection& sel)
{
  for(auto& proc : interval.processes)
  {
    search_rec(stxt, addresses, proc, sel);
  }

  (void)add_if_contains(interval, stxt, sel);
}

static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::EventModel& obj, Selection& sel)
{
  if(add_if_contains(obj, stxt, sel))
    return;
  for(auto& addr : addresses)
  {
    if(State::findAddressInExpression(obj.condition(), addr.address))
    {
      sel.append(obj);
      return;
    }
  }
}

static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::StateModel& obj, Selection& sel)
{
  static thread_local State::MessageList listCache;
  static thread_local std::vector<QString> messagesCache;
  listCache.clear();
  messagesCache.clear();

  auto& root = obj.messages().rootNode();

  // First look for addresses containing the looked-up address
  bool must_add = Process::hasMatchingAddress(root, addresses, listCache, messagesCache);

  // If not found, then look for addresses containing the raw string
  if(!must_add)
    must_add = Process::hasMatchingText(root, stxt, listCache, messagesCache);
  // FIXME look into state processes?

  // Try to add if the searched text is in the name of the state
  if(must_add)
    sel.append(&obj);
  else
    (void)add_if_contains(obj, stxt, sel);
}

static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::TimeSyncModel& obj, Selection& sel)
{
  if(add_if_contains(obj, stxt, sel))
    return;
  for(auto& addr : addresses)
  {
    if(State::findAddressInExpression(obj.expression(), addr.address))
    {
      sel.append(obj);
      return;
    }
  }
}

static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Scenario::ProcessModel& proc, Selection& sel)
{
  for(auto& obj : proc.states)
    search_rec(stxt, addresses, obj, sel);

  for(auto& obj : proc.events)
    search_rec(stxt, addresses, obj, sel);

  for(auto& obj : proc.timeSyncs)
    search_rec(stxt, addresses, obj, sel);

  for(auto& obj : proc.comments)
    (void)add_if_contains(obj, stxt, sel);

  for(auto& obj : proc.intervals)
    search_rec(stxt, addresses, obj, sel);
}

static void search_rec(
    const QString& stxt, const std::vector<State::AddressAccessor>& addresses,
    const Process::ProcessModel& proc, Selection& sel)
{
  if(auto scenario = qobject_cast<const Scenario::ProcessModel*>(&proc))
  {
    return search_rec(stxt, addresses, *scenario, sel);
  }
  selectProcessPortsWithAddress(addresses, proc, sel);

  (void)add_if_contains(proc, stxt, sel);
}

void SearchWidget::search()
{
  QString stxt = text();
  std::vector<State::AddressAccessor> addresses;

  int idx = stxt.indexOf("=");
  if(idx >= 0)
  {
    QString substr = stxt.mid(0, idx);
    if(substr == "address")
    {
      QString addrstr = stxt.mid(idx + 1);
      if(auto spaceidx = addrstr.indexOf(" ") >= 0)
        addrstr = substr.mid(0, spaceidx);

      int comma = addrstr.indexOf(",");
      int offset = 0;
      while(comma >= 0)
      {
        auto sub = addrstr.mid(offset, comma);
        auto optaddr = State::parseAddressAccessor(sub);
        if(optaddr)
          addresses.push_back(*optaddr);
        offset = comma + 1;
        comma = addrstr.indexOf(",", offset);
      }
      auto sub = addrstr.mid(offset, comma);
      auto optaddr = State::parseAddressAccessor(sub);
      if(optaddr)
        addresses.push_back(*optaddr);
    }
  }

  if(addresses.empty())
  {
    if(stxt.startsWith("address="))
    {
      if(auto opt = State::parseAddressAccessor(stxt.remove("address=")))
        addresses.push_back(*opt);
    }
    else
    {
      if(auto opt = State::parseAddressAccessor(stxt))
        addresses.push_back(*opt);
    }
  }

  auto* doc = m_ctx.documents.currentDocument();
  auto& model = score::IDocument::modelDelegate<ScenarioDocumentModel>(*doc);
  auto& bs = model.baseScenario();

  Selection sel{};
  search_rec(stxt, addresses, bs.startState(), sel);
  search_rec(stxt, addresses, bs.startEvent(), sel);
  search_rec(stxt, addresses, bs.startTimeSync(), sel);
  search_rec(stxt, addresses, bs.endState(), sel);
  search_rec(stxt, addresses, bs.endEvent(), sel);
  search_rec(stxt, addresses, bs.endTimeSync(), sel);
  search_rec(stxt, addresses, bs.interval(), sel);
  sel.removeDuplicates();

  score::SelectionDispatcher d{doc->context().selectionStack};
  d.select(sel);
}

}
