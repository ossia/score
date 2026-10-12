#include "AudioRouting.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioDevice.hpp>
#include <Audio/RoutingMatrix.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace Audio
{
namespace
{
Device::Node*
explorerNode(Explorer::DeviceDocumentPlugin& plug, const Device::DeviceInterface& dev)
{
  for(auto& n : plug.rootNode())
    if(n.is<Device::DeviceSettings>()
       && n.get<Device::DeviceSettings>().name == dev.settings().name)
      return &n;
  return nullptr;
}

struct Ports
{
  std::vector<RoutedPort> ports;
  std::vector<State::Address> addresses;
};

void collectPorts(const Device::Node& node, const std::string& kind, Ports& res)
{
  for(auto& child : node)
  {
    if(child.is<Device::AddressSettings>())
    {
      const auto& attr = child.get<Device::AddressSettings>().extendedAttributes;
      auto k = attr.find("audio-kind");
      auto m = attr.find("audio-mapping");
      if(k != attr.end() && m != attr.end())
      {
        auto k_str = ossia::any_cast<std::string>(&k->second);
        auto mapping = ossia::any_cast<ossia::audio_mapping>(&m->second);
        if(k_str && mapping && *k_str == kind)
        {
          auto addr = Device::address(child).address;
          res.ports.push_back({addr.path.join('/'), *mapping});
          res.addresses.push_back(std::move(addr));
        }
      }
    }
    collectPorts(child, kind, res);
  }
}

class RoutingSection final : public QWidget
{
public:
  RoutingSection(
      const QString& title, const QString& empty, RoutingMatrix::Direction d,
      QWidget* parent)
      : QWidget{parent}
      , matrix{new RoutingMatrix{d, this}}
      , add{new QToolButton{this}}
      , m_empty{new QLabel{empty, this}}
  {
    auto lay = new QVBoxLayout{this};
    lay->setContentsMargins(0, 0, 0, 0);
    auto top = new QHBoxLayout;
    auto label = new QLabel{title, this};
    auto f = label->font();
    f.setBold(true);
    label->setFont(f);
    top->addWidget(label);
    top->addStretch(1);
    top->addWidget(add);
    lay->addLayout(top);
    lay->addWidget(matrix, 1);
    m_empty->setWordWrap(true);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setEnabled(false);
    lay->addWidget(m_empty, 1);
  }

  void setPorts(Ports p, int cardChannels)
  {
    addresses = std::move(p.addresses);
    matrix->setCardChannels(cardChannels);
    matrix->setPorts(std::move(p.ports));
    const bool none = matrix->ports().empty();
    matrix->setVisible(!none);
    m_empty->setVisible(none);
  }

  RoutingMatrix* matrix{};
  QToolButton* add{};
  std::vector<State::Address> addresses;

private:
  QLabel* m_empty{};
};

class RoutingWindow final : public QWidget
{
public:
  RoutingWindow(
      const score::DocumentContext& ctx, Dataflow::AudioDevice& dev, QWidget* parent)
      : QWidget{parent, Qt::Window}
      , m_context{ctx}
      , m_device{&dev}
  {
    setObjectName(QStringLiteral("AudioRouting"));
    setWindowTitle(tr("Audio routing"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 640);

    auto lay = new QVBoxLayout{this};
    auto help = new QLabel{RoutingMatrix::help(), this};
    help->setWordWrap(true);
    lay->addWidget(help);

    auto split = new QSplitter{Qt::Vertical, this};
    split->setChildrenCollapsible(false);
    m_inputs = new RoutingSection{
        tr("Inputs: the sound card's channels that each input port takes"),
        tr("No input port. Add one to bring channels of the sound card into the "
           "score."),
        RoutingMatrix::Direction::FromCard, split};
    m_outputs = new RoutingSection{
        tr("Outputs: the sound card's channels where each output port goes"),
        tr("No output port. Add one to send sound to chosen channels of the sound "
           "card."),
        RoutingMatrix::Direction::ToCard, split};
    split->addWidget(m_inputs);
    split->addWidget(m_outputs);
    lay->addWidget(split, 1);

    auto note = new QLabel{
        tr("Virtual ports are not here: they carry sound between processes, not to "
           "the sound card. Set the output or the input of a process to one."),
        this};
    note->setWordWrap(true);
    note->setEnabled(false);
    lay->addWidget(note);

    auto bottom = new QHBoxLayout;
    auto clear = new QPushButton{tr("Disconnect everything"), this};
    bottom->addWidget(clear);
    bottom->addStretch(1);
    auto close = new QPushButton{tr("Close"), this};
    bottom->addWidget(close);
    lay->addLayout(bottom);

    for(auto [sec, kind, text] :
        {std::tuple{m_inputs, std::string{"in"}, tr("Add an input port...")},
         std::tuple{m_outputs, std::string{"out"}, tr("Add an output port...")}})
    {
      sec->add->setText(text);
      sec->add->setAutoRaise(true);
      connect(sec->add, &QToolButton::clicked, this, [this, kind = kind] {
        if(m_device)
          addAudioPort(m_context, *m_device, this, kind);
      });
      sec->matrix->onRoute = [this, sec = sec](RoutingMatrix::Changes c) {
        route({{sec, std::move(c)}});
      };
    }

    connect(clear, &QPushButton::clicked, this, [this] { disconnectEverything(); });
    connect(close, &QPushButton::clicked, this, &QWidget::close);

    auto refresh = [this] { requestRefresh(); };
    connect(&dev, &Dataflow::AudioDevice::portsChanged, this, refresh);
    connect(&dev, &Dataflow::AudioDevice::changed, this, refresh);
    auto& set = m_context.app.settings<Audio::Settings::Model>();
    connect(&set, &Audio::Settings::Model::DefaultInChanged, this, refresh);
    connect(&set, &Audio::Settings::Model::DefaultOutChanged, this, refresh);
    refresh();
  }

private:
  void requestRefresh()
  {
    if(m_refreshPending)
      return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this] {
      m_refreshPending = false;
      refresh();
    });
  }

  std::pair<int, int> cardChannels() const
  {
    if(m_device)
      if(auto p = m_device->getProtocol())
        return {int(p->audio_ins.size()), int(p->audio_outs.size())};
    auto& set = m_context.app.settings<Audio::Settings::Model>();
    return {set.getDefaultIn(), set.getDefaultOut()};
  }

  void refresh()
  {
    Ports ins, outs;
    if(m_device)
    {
      auto& plug = m_context.plugin<Explorer::DeviceDocumentPlugin>();
      if(auto node = explorerNode(plug, *m_device))
      {
        collectPorts(*node, "in", ins);
        collectPorts(*node, "out", outs);
      }
    }
    const auto [n_in, n_out] = cardChannels();
    m_inputs->setPorts(std::move(ins), n_in);
    m_outputs->setPorts(std::move(outs), n_out);
  }

  void route(std::vector<std::pair<RoutingSection*, RoutingMatrix::Changes>> changes)
  {
    Routes routes;
    for(auto& [sec, c] : changes)
      for(auto& [port, mapping] : c)
        if(port >= 0 && port < int(sec->addresses.size()))
          routes.emplace_back(sec->addresses[port], std::move(mapping));
    applyRouting(m_context, std::move(routes));
  }

  void disconnectEverything()
  {
    std::vector<std::pair<RoutingSection*, RoutingMatrix::Changes>> changes;
    for(auto sec : {m_inputs, m_outputs})
    {
      RoutingMatrix::Changes c;
      const auto& ports = sec->matrix->ports();
      for(int p = 0; p < int(ports.size()); p++)
      {
        auto m = ports[p].mapping;
        if(std::all_of(m.begin(), m.end(), [](int ch) { return ch == unrouted; }))
          continue;
        std::fill(m.begin(), m.end(), unrouted);
        c.emplace_back(p, std::move(m));
      }
      if(!c.empty())
        changes.emplace_back(sec, std::move(c));
    }
    route(std::move(changes));
  }

  const score::DocumentContext& m_context;
  QPointer<Dataflow::AudioDevice> m_device;
  RoutingSection* m_inputs{};
  RoutingSection* m_outputs{};
  bool m_refreshPending{};
};
}

void applyRouting(const score::DocumentContext& ctx, Routes routes)
{
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  MacroCommandDispatcher<ChangeRouting> disp{ctx.commandStack};
  for(auto& [address, mapping] : routes)
  {
    auto node = Device::try_getNodeFromAddress(plug.rootNode(), address);
    if(!node || !node->is<Device::AddressSettings>())
      continue;
    auto settings = node->get<Device::AddressSettings>();
    settings.extendedAttributes["audio-mapping"] = std::move(mapping);
    disp.submit(new Explorer::Command::UpdateAddressSettings{
        plug, Device::NodePath{*node}, settings});
  }
  disp.commit();
}

void addAudioPort(
    const score::DocumentContext& ctx, Dataflow::AudioDevice& dev, QWidget* parent,
    const std::string& kind)
{
  auto proto
      = ctx.app.interfaces<Device::ProtocolFactoryList>().get(dev.settings().protocol);
  if(!proto)
    return;
  Device::AddressSettings proposed;
  proposed.extendedAttributes["audio-kind"] = kind;
  proposed.extendedAttributes["audio-channels"] = 2;
  std::unique_ptr<Device::AddressDialog> dial{
      proto->makeEditAddressDialog(proposed, dev, ctx, parent)};
  if(!dial || dial->exec() != QDialog::Accepted)
    return;

  auto stgs = dial->getSettings();
  stgs.name = stgs.name.trimmed();
  while(stgs.name.startsWith('/'))
    stgs.name.remove(0, 1);
  if(stgs.name.isEmpty())
    return;

  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  RedoMacroCommandDispatcher<Explorer::Command::AddAddresses> disp{ctx.commandStack};
  if(!explorerNode(plug, dev))
    disp.submit(new Explorer::Command::LoadDevice{plug, dev.settings()});
  if(auto node = explorerNode(plug, dev))
    disp.submit(new Explorer::Command::AddAddress{
        plug, Device::NodePath{*node}, InsertMode::AsChild, stgs});
  disp.commit();
}

void showAudioRouting(const score::DocumentContext& ctx)
{
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  auto dev = qobject_cast<Dataflow::AudioDevice*>(plug.list().audioDevice());
  auto view = ctx.document.view();
  if(!dev || !view)
    return;
  auto parent = view->viewDelegate().getWidget();
  if(!parent)
    return;

  auto w = parent->findChild<QWidget*>(
      QStringLiteral("AudioRouting"), Qt::FindDirectChildrenOnly);
  if(!w)
    w = new RoutingWindow{ctx, *dev, parent};
  w->show();
  w->raise();
  w->activateWindow();
}
}
