#include "SearchWidget.hpp"
// SearchWidget
#include <State/Expression.hpp>
#include <State/MessageListSerialization.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/OfflineAction/OfflineAction.hpp>

#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/ArrowButton.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia-qt/invoke.hpp>

//TODO remove test
#include "SearchReplaceWidget.hpp"
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
  act->setStatusTip(tr("Find And Replace"));
  addAction(act, QLineEdit::TrailingPosition);
  connect(act, &QAction::triggered, this, [=] {
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
void add_if_contains(const T& o, const QString& str, Selection& sel)
{
  const auto& obj = o.metadata();
  if(obj.getName().contains(str) || obj.getComment().contains(str)
     || obj.getLabel().contains(str))
  {
    sel.append(o);
  }
}
void add_if_contains(
    const Scenario::CommentBlockModel& o, const QString& str, Selection& sel)
{
  if(o.content().contains(str))
  {
    sel.append(o);
  }
}

static void selectProcessPortsWithAddress(
    const std::vector<State::AddressAccessor>& addresses, Process::ProcessModel& proc,
    Selection& sel)
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

void SearchWidget::search()
{
  const QString& stxt = text();
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
    auto opt = State::parseAddressAccessor(stxt);
    if(opt)
    {
      addresses.push_back(*opt);
    }
  }

  auto* doc = m_ctx.documents.currentDocument();

  auto scenarioModel = doc->focusManager().get();
  Selection sel{};

  if(scenarioModel)
  {
    State::MessageList listCache;
    std::vector<QString> messagesCache;
    // Serialize ALL the things
    for(const auto& obj : scenarioModel->children())
    {
      if(auto state = qobject_cast<const StateModel*>(obj))
      {
        listCache.clear();
        messagesCache.clear();

        auto& root = state->messages().rootNode();

        // First look for addresses containing the looked-up address
        bool must_add
            = Process::hasMatchingAddress(root, addresses, listCache, messagesCache);

        // If not found, then look for addresses containing the raw string
        if(!must_add)
          must_add = Process::hasMatchingText(root, stxt, listCache, messagesCache);
        // FIXME look into state processes?

        // Try to add if the searched text is in the name of the state
        if(must_add)
          sel.append(state);
        else
          add_if_contains(*state, stxt, sel);
      }
      else if(auto event = qobject_cast<const EventModel*>(obj))
      {
        add_if_contains(*event, stxt, sel);
      }
      else if(auto ts = qobject_cast<const TimeSyncModel*>(obj))
      {
        add_if_contains(*ts, stxt, sel);
      }
      else if(auto cmt = qobject_cast<const CommentBlockModel*>(obj))
      {
        add_if_contains(*cmt, stxt, sel);
      }
      else if(auto interval = qobject_cast<const IntervalModel*>(obj))
      {
        for(auto& proc : interval->processes)
        {
          selectProcessPortsWithAddress(addresses, proc, sel);

          add_if_contains(proc, stxt, sel);
        }

        add_if_contains(*interval, stxt, sel);
      }
    }
  }

  score::SelectionDispatcher d{doc->context().selectionStack};
  d.select(sel);
}

}
