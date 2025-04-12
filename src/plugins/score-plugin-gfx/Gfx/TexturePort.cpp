#include "TexturePort.hpp"

#include "GfxDevice.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Process/Dataflow/AudioPortComboBox.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Inspector/InspectorLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QSpinBox>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::TextureInlet)
W_OBJECT_IMPL(Gfx::TextureOutlet)

namespace Gfx
{
MODEL_METADATA_IMPL_CPP(TextureInlet);
MODEL_METADATA_IMPL_CPP(TextureOutlet);

class GraphPreviewWidget : public QWidget
{
public:
  GraphPreviewWidget(const TextureOutlet& outlet, Gfx::DocumentPlugin& plug)
      : outlet_p{&outlet}
      , plug{&plug}
  {
    setLayout(new Inspector::VBoxLayout{this});
    auto window = std::make_unique<score::gfx::ScreenNode>(true);
    node = window.get();
    screenId = plug.context.register_preview_node(std::move(window));
    if(screenId != -1)
    {
      timerId = startTimer(16);
    }
  }

  void timerEvent(QTimerEvent*)
  {
    const auto& w = node->window();
    if(!w)
      return;

    if(!outlet_p)
      return;

    auto& outlet = *outlet_p;

    if(outlet.nodeId != nodeId)
    {
      if(e)
      {
        if(plug)
          plug->context.disconnect_preview_node(*e);
        e = std::nullopt;
      }

      if(outlet.nodeId != -1)
      {
        nodeId = outlet.nodeId;
        e = {{nodeId, 0}, {screenId, 0}};

        if(plug)
          plug->context.connect_preview_node(*e);
      }
    }

    if(!container)
    {
      qwindow = w.get();
      this->window = w;

      container = QWidget::createWindowContainer(qwindow, this);
      container->setMinimumWidth(100);
      container->setMaximumWidth(300);
      container->setMinimumHeight(200);
      container->setMaximumHeight(200);
      this->layout()->addWidget(container);
    }
  }

  ~GraphPreviewWidget()
  {
    if(qwindow)
    {
      // Take back ownership of the window
      qwindow->setParent(nullptr);
      qwindow->close();
      QChildEvent ev(QEvent::ChildRemoved, qwindow);
      ((QObject*)container)->event(&ev);
    }

    // We "garbage collect" the window
    QTimer::singleShot(1, [w = this->window] { });
    if(plug)
      plug->context.unregister_preview_node(screenId);
  }

private:
  QPointer<const TextureOutlet> outlet_p;
  QPointer<Gfx::DocumentPlugin> plug;
  score::gfx::ScreenNode* node{};
  std::optional<Gfx::EdgeSpec> e;

  std::shared_ptr<score::gfx::Window> window;

  QWindow* qwindow{};
  QWidget* container{};

  int screenId = -1;
  int nodeId = -1;
  int timerId{};
};

TextureInlet::~TextureInlet() { }

TextureInlet::TextureInlet(Id<Process::Port> c, QObject* parent)
    : Process::Inlet{std::move(c), parent}
{
}

TextureInlet::TextureInlet(DataStream::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
  if(m_textureFilter == ossia::texture_filter::NONE)
    m_textureFilter = ossia::texture_filter::NEAREST;
}
TextureInlet::TextureInlet(JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
  if(m_textureFilter == ossia::texture_filter::NONE)
    m_textureFilter = ossia::texture_filter::NEAREST;
}
TextureInlet::TextureInlet(DataStream::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
  if(m_textureFilter == ossia::texture_filter::NONE)
    m_textureFilter = ossia::texture_filter::NEAREST;
}
TextureInlet::TextureInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
  if(m_textureFilter == ossia::texture_filter::NONE)
    m_textureFilter = ossia::texture_filter::NEAREST;
}

std::optional<QSize> TextureInlet::renderSize() const noexcept
{
  return m_renderSize;
}

void TextureInlet::setRenderSize(std::optional<QSize> sz)
{
  if(m_renderSize != sz)
  {
    m_renderSize = sz;
    renderSizeChanged(m_renderSize);
  }
}

void TextureInlet::unsetRenderSize()
{
  if(m_renderSize)
  {
    m_renderSize = std::nullopt;
    renderSizeChanged(m_renderSize);
  }
}

void TextureInlet::setupExecution(
    ossia::inlet& inl, QObject* exec_context) const noexcept
{
  ossia::texture_inlet& exec = *safe_cast<ossia::texture_inlet*>(&inl);
  if(auto sz = m_renderSize)
    exec.data.size = {(int32_t)sz->width(), (int32_t)sz->height()};
  exec.data.address_u = m_textureAddressMode;
  exec.data.address_v = m_textureAddressMode;
  exec.data.address_w = m_textureAddressMode;
  exec.data.min_filter = m_textureFilter;
  exec.data.mag_filter = m_textureFilter;
  exec.data.format = m_textureFormat;

  connect(
      this, &TextureInlet::renderSizeChanged, exec_context,
      [&exec](std::optional<QSize> v) {
    if(v)
      exec.data.size = {v->width(), v->height()};
    else
      exec.data.size.reset();
  });

  connect(
      this, &TextureInlet::textureFormatChanged, exec_context,
      [&exec](ossia::texture_format v) { exec.data.format = v; });

  connect(
      this, &TextureInlet::textureFilterChanged, exec_context,
      [&exec](ossia::texture_filter v) {
    exec.data.min_filter = v;
    exec.data.mag_filter = v;
  });

  connect(
      this, &TextureInlet::textureAddressModeChanged, exec_context,
      [&exec](ossia::texture_address_mode v) {
    exec.data.address_u = v;
    exec.data.address_v = v;
    exec.data.address_w = v;
  });
}

TextureOutlet::~TextureOutlet() { }

TextureOutlet::TextureOutlet(Id<Process::Port> c, QObject* parent)
    : Process::Outlet{std::move(c), parent}
{
}

TextureOutlet::TextureOutlet(DataStream::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(JSONObject::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(DataStream::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}

struct TextureSizeWidget : public QWidget
{
  explicit TextureSizeWidget(
      const TextureInlet& port, const score::DocumentContext& ctx, QWidget* parent)
      : QWidget{parent}
      , m_model{port}
      , m_ongoingDispatcher{ctx.commandStack}
  {
    auto sz_lay = new score::MarginLess<QHBoxLayout>{};
    setLayout(sz_lay);
    m_enabled = new QCheckBox{};
    score::setHelp(
        m_enabled,
        QObject::tr(
            "When enabled, the render target will use the given size: every input "
            "process will render to a texture of said size upon execution. "
            "Otherwise, it will "
            "use the default render size, usually the window viewport size."));
    auto rs = port.renderSize();
    m_enabled->setChecked(bool(rs));

    m_sz_w = new QSpinBox{};
    m_sz_w->setRange(0, 65535);
    m_sz_w->setEnabled(m_enabled->isChecked());
    if(rs)
      m_sz_w->setValue(rs->width());
    else
      m_sz_w->setValue(defaultSize().width());
    m_sz_h = new QSpinBox{};
    m_sz_h->setRange(0, 65535);
    m_sz_h->setEnabled(m_enabled->isChecked());
    if(rs)
      m_sz_h->setValue(rs->height());
    else
      m_sz_h->setValue(defaultSize().height());

    sz_lay->addWidget(m_enabled);
    sz_lay->addWidget(m_sz_w);
    sz_lay->addWidget(m_sz_h);
    sz_lay->setAlignment(m_enabled, Qt::AlignRight);
    QObject::connect(m_enabled, SignalUtils::QCheckBox_checkStateChanged(), this, [this](int state) {
      m_sz_w->setEnabled(state);
      m_sz_h->setEnabled(state);
      if(state)
      {
        update_size();
      }
      else
      {
        m_ongoingDispatcher.submit(m_model, std::nullopt);
      }
      m_ongoingDispatcher.commit();
    });

    QObject::connect(
        m_sz_w, &QSpinBox::valueChanged, this, &TextureSizeWidget::update_size);
    QObject::connect(
        m_sz_h, &QSpinBox::valueChanged, this, &TextureSizeWidget::update_size);
    QObject::connect(
        m_sz_w, &QSpinBox::editingFinished, this, &TextureSizeWidget::commit);
    QObject::connect(
        m_sz_h, &QSpinBox::editingFinished, this, &TextureSizeWidget::commit);

    connect(
        &m_model, &TextureInlet::renderSizeChanged, this,
        &TextureSizeWidget::on_renderSizeChanged);
  }

  void on_renderSizeChanged(std::optional<QSize> sz)
  {
    const QSignalBlocker blck_w{m_sz_w};
    const QSignalBlocker blck_h{m_sz_h};
    const QSignalBlocker blck_cb{m_enabled};
    const bool enabled = bool(sz);
    m_enabled->setChecked(enabled);
    m_sz_w->setEnabled(enabled);
    m_sz_h->setEnabled(enabled);
    if(enabled)
    {
      m_sz_w->setValue(sz->width());
      m_sz_h->setValue(sz->height());
    }
  }

  void update_size()
  {
    m_ongoingDispatcher.submit(m_model, QSize{m_sz_w->value(), m_sz_h->value()});
  }

  void commit() { m_ongoingDispatcher.commit(); }

  QSize defaultSize() const noexcept
  {
    return QSize{
        1280, 720}; // FIXME use the actual one from the device viewport if any instead.
  }

  const TextureInlet& m_model;
  SingleOngoingCommandDispatcher<ChangeTextureInletRenderSize> m_ongoingDispatcher;

  QCheckBox* m_enabled{};
  QSpinBox* m_sz_w{};
  QSpinBox* m_sz_h{};
};

void TextureInletFactory::setupInletInspector(
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();

  auto cond
      = [](Device::DeviceInterface& dev) { return qobject_cast<GfxInputDevice*>(&dev); };

  lay.addRow(Process::makeDeviceCombo(cond, device.list(), port, ctx, parent));

  auto& inlet = safe_cast<const TextureInlet&>(port);
  // Size
  lay.addRow("Size", new TextureSizeWidget{inlet, ctx, parent});

  // Format
  {
    using enum ossia::texture_format;
    auto combo = new QComboBox{parent};
    combo->addItem("RGBA8", (int)RGBA8);
    combo->addItem("RGBA16F", (int)RGBA16F);
    combo->addItem("RGBA32F", (int)RGBA32F);
    combo->addItem("R8", (int)R8);
    combo->addItem("R16", (int)R16);
    combo->addItem("R16F", (int)R16F);
    combo->addItem("R32F", (int)R32F);
    static const auto map = ossia::flat_map<ossia::texture_format, int>{
        {RGBA8, 0}, {RGBA16F, 1}, {RGBA32F, 2}, {R8, 3}, {R16, 4}, {R16F, 5}, {R32F, 6}};
    if(auto it = map.find(inlet.textureFormat()); it != map.end())
      combo->setCurrentIndex(it->second);
    QObject::connect(
        &inlet, &TextureInlet::textureFormatChanged, combo,
        [combo](ossia::texture_format fmt) {
      if((int)fmt != combo->currentData())
      {
        if(auto it = map.find(fmt); it != map.end())
          combo->setCurrentIndex(it->second);
      }
    });
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, &inlet, [&ctx, &inlet, combo](int idx) {
      auto fmt = (ossia::texture_format)combo->itemData(idx).toInt();
      if(fmt != inlet.textureFormat())
        CommandDispatcher<>{ctx.commandStack}.submit<ChangeTextureInletFormat>(
            inlet, fmt);
    });

    lay.addRow("Format", combo);
  }

  // Filter
  {
    using enum ossia::texture_filter;
    auto combo = new QComboBox{parent};
    combo->addItem("Nearest", ossia::texture_filter::NEAREST);
    combo->addItem("Linear", ossia::texture_filter::LINEAR);
    combo->setCurrentIndex((int(inlet.textureFilter()) - 1));

    QObject::connect(
        &inlet, &TextureInlet::textureFilterChanged, combo,
        [combo](ossia::texture_filter fmt) {
      if((int)fmt != combo->currentData())
        combo->setCurrentIndex((int)fmt - 1);
    });
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, &inlet, [&ctx, &inlet](int idx) {
      auto mode = (ossia::texture_filter)(idx + 1);
      if(mode != inlet.textureFilter())
        CommandDispatcher<>{ctx.commandStack}.submit<ChangeTextureInletFilter>(
            inlet, mode);
    });

    lay.addRow("Filter", combo);
  }

  // Address mode
  {
    using enum ossia::texture_address_mode;
    auto combo = new QComboBox{parent};
    combo->addItem("Repeat", ossia::texture_address_mode::REPEAT);
    combo->addItem("Clamp to edge", ossia::texture_address_mode::CLAMP_TO_EDGE);
    combo->addItem("Mirror", ossia::texture_address_mode::MIRROR);
    combo->setCurrentIndex((int)inlet.textureAddressMode());

    QObject::connect(
        &inlet, &TextureInlet::textureAddressModeChanged, combo,
        [combo](ossia::texture_address_mode fmt) {
      if((int)fmt != combo->currentData())
        combo->setCurrentIndex((int)fmt);
    });
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, &inlet, [&ctx, &inlet](int idx) {
      auto mode = (ossia::texture_address_mode)idx;
      if(mode != inlet.textureAddressMode())
        CommandDispatcher<>{ctx.commandStack}.submit<ChangeTextureInletAddressMode>(
            inlet, (ossia::texture_address_mode)idx);
    });

    lay.addRow("Address mode", combo);
  }
}
void TextureOutletFactory::setupOutletInspector(
    const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  auto cond = [](Device::DeviceInterface& dev) {
    return qobject_cast<GfxOutputDevice*>(&dev);
  };
  lay.addRow(Process::makeDeviceCombo(cond, device.list(), port, ctx, parent));

  auto& outlet = safe_cast<const TextureOutlet&>(port);
  lay.addRow(new GraphPreviewWidget{outlet, ctx.plugin<Gfx::DocumentPlugin>()});
}
}

template <>
void DataStreamReader::read(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
  m_stream << p.m_renderSize << p.m_textureFormat << p.m_textureAddressMode
           << p.m_textureFilter;
}
template <>
void DataStreamWriter::write(Gfx::TextureInlet& p)
{
  m_stream >> p.m_renderSize >> p.m_textureFormat >> p.m_textureAddressMode
      >> p.m_textureFilter;
}

template <>
void JSONReader::read(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
  if(p.m_renderSize)
  {
    obj["RenderSize"] = *p.m_renderSize;
  }
  obj["Format"] = p.m_textureFormat;
  obj["Filter"] = p.m_textureFilter;
  obj["AddressMode"] = p.m_textureAddressMode;
}
template <>
void JSONWriter::write(Gfx::TextureInlet& p)
{
  if(auto sz = obj.tryGet("RenderSize"))
  {
    QSize s;
    s <<= *sz;
    p.m_renderSize = s;
  }
  else
  {
    p.m_renderSize = std::nullopt;
  }
  if(auto fmt = obj.tryGet("Format"))
  {
    p.m_textureFormat <<= *fmt;
    p.m_textureFilter <<= obj["Filter"];
    p.m_textureAddressMode <<= obj["AddressMode"];
  }
}

template <>
void DataStreamReader::read(const Gfx::TextureOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void DataStreamWriter::write(Gfx::TextureOutlet& p)
{
}

template <>
void JSONReader::read(const Gfx::TextureOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void JSONWriter::write(Gfx::TextureOutlet& p)
{
}

W_OBJECT_IMPL(Gfx::GeometryInlet)
W_OBJECT_IMPL(Gfx::GeometryOutlet)

namespace Gfx
{

MODEL_METADATA_IMPL_CPP(GeometryInlet)
MODEL_METADATA_IMPL_CPP(GeometryOutlet)

GeometryInlet::~GeometryInlet() { }

GeometryInlet::GeometryInlet(Id<Process::Port> c, QObject* parent)
    : Process::Inlet{std::move(c), parent}
{
}

GeometryInlet::GeometryInlet(DataStream::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
GeometryInlet::GeometryInlet(JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
GeometryInlet::GeometryInlet(DataStream::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
GeometryInlet::GeometryInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

GeometryOutlet::~GeometryOutlet() { }

GeometryOutlet::GeometryOutlet(Id<Process::Port> c, QObject* parent)
    : Process::Outlet{std::move(c), parent}
{
}

GeometryOutlet::GeometryOutlet(DataStream::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
GeometryOutlet::GeometryOutlet(JSONObject::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
GeometryOutlet::GeometryOutlet(DataStream::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
GeometryOutlet::GeometryOutlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}

}

template <>
void DataStreamReader::read(const Gfx::GeometryInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void DataStreamWriter::write(Gfx::GeometryInlet& p)
{
}

template <>
void JSONReader::read(const Gfx::GeometryInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void JSONWriter::write(Gfx::GeometryInlet& p)
{
}

template <>
void DataStreamReader::read(const Gfx::GeometryOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void DataStreamWriter::write(Gfx::GeometryOutlet& p)
{
}

template <>
void JSONReader::read(const Gfx::GeometryOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void JSONWriter::write(Gfx::GeometryOutlet& p)
{
}
