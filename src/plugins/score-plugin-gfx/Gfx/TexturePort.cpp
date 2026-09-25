#include <Process/PreviewSettings.hpp>
#include "TexturePort.hpp"

#include "GfxDevice.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Process/Dataflow/PortAddressComboBox.hpp>
#include <Process/Process.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Widgets/RhiPreviewWidget.hpp>
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
      , m_ctx{&plug.context}
  {
    setLayout(new Inspector::VBoxLayout{this});

    m_rhiWidget = new RhiPreviewWidget(this);
    m_rhiWidget->setMinimumWidth(100);
    m_rhiWidget->setMaximumWidth(300);
    m_rhiWidget->setMinimumHeight(200);
    m_rhiWidget->setMaximumHeight(200);
    layout()->addWidget(m_rhiWidget);

    m_enabled = Process::PreviewSettings::instance().enabled() && !m_ctx.isNull();
    if(m_enabled)
      m_rhiWidget->useContext(m_ctx, outlet.graphicsPort());
    else
      hide(); // setVisible(true) on a not-yet-parented widget would pop up a window

    // Only poll while something is being previewed; the toggle brings the timer
    // back rather than leaving a 60Hz tick running for a disabled preview.
    QObject::connect(
        &Process::PreviewSettings::instance(),
        &Process::PreviewSettings::enabledChanged, this, [this](bool on) {
      if(on && m_timer == 0)
        m_timer = startTimer(16);
      else if(!on)
        syncEnabled();
    });

    // TextureOutlet::nodeId has no notifier — poll for changes so a
    // process re-instantiation rewires the preview to the new producer.
    // graphicsPort() is the registered endpoint: the outlet's own index,
    // not port zero, which is a different outlet whenever a value outlet is
    // declared before the texture.
    if(m_enabled)
      m_timer = startTimer(16);
  }

  //! Brings the widget in line with the setting, stopping the tick when off.
  void syncEnabled()
  {
    if(!outlet_p || !m_rhiWidget)
      return;

    const bool on = Process::PreviewSettings::instance().enabled() && !m_ctx.isNull();
    if(on == m_enabled)
      return;

    m_enabled = on;
    if(on)
    {
      m_rhiWidget->useContext(m_ctx, outlet_p->graphicsPort());
    }
    else
    {
      m_rhiWidget->detach();
      if(m_timer != 0)
      {
        killTimer(m_timer);
        m_timer = 0;
      }
    }
    setVisible(on);
  }

  void timerEvent(QTimerEvent*) override
  {
    if(!outlet_p || !m_rhiWidget)
      return;

    syncEnabled();

    if(m_enabled)
      m_rhiWidget->setProducer(outlet_p->graphicsPort());
  }

  ~GraphPreviewWidget() override = default;

private:
  QPointer<const TextureOutlet> outlet_p;
  QPointer<GfxContext> m_ctx;
  RhiPreviewWidget* m_rhiWidget{};
  bool m_enabled{};
  int m_timer{};
};

TextureInlet::~TextureInlet() { }

TextureInlet::TextureInlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Process::Inlet{name, std::move(c), parent}
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
  exec.data.mipmap_mode = m_textureMipmapMode;
  exec.data.format = m_textureFormat.value_or(ossia::texture_format::RGBA8);
  exec.data.format_set = m_textureFormat.has_value();

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
      [&exec](std::optional<ossia::texture_format> v) {
    exec.data.format = v.value_or(ossia::texture_format::RGBA8);
    exec.data.format_set = v.has_value();
  });

  connect(
      this, &TextureInlet::textureFilterChanged, exec_context,
      [&exec](ossia::texture_filter v) {
    exec.data.min_filter = v;
    exec.data.mag_filter = v;
  });

  connect(
      this, &TextureInlet::textureMipmapModeChanged, exec_context,
      [&exec](ossia::texture_filter v) { exec.data.mipmap_mode = v; });

  connect(
      this, &TextureInlet::textureAddressModeChanged, exec_context,
      [&exec](ossia::texture_address_mode v) {
    exec.data.address_u = v;
    exec.data.address_v = v;
    exec.data.address_w = v;
  });
}

ossia::gfx::port_index TextureOutlet::graphicsPort() const noexcept
{
  if(nodeId >= 0)
  {
    if(auto process = qobject_cast<const Process::ProcessModel*>(parent()))
    {
      const auto& outlets = process->outlets();
      for(int i = 0; i < outlets.size(); ++i)
        if(outlets[i] == this)
          return {nodeId, i};
    }
  }
  return {-1, -1};
}

TextureOutlet::~TextureOutlet() { }

TextureOutlet::TextureOutlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Process::Outlet{name, std::move(c), parent}
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
            "Otherwise the size is automatic: the size of the render target the "
            "process renders into, else the render size of the output, usually "
            "the window viewport size. "
            "An inlet with a single cable, no size and no format set may read "
            "the texture its input process publishes directly instead."));
    auto rs = port.renderSize();
    m_enabled->setChecked(bool(rs));

    m_sz_w = new QSpinBox{};
    m_sz_w->setMaximum(65535);
    m_sz_w->setSpecialValueText(QObject::tr("Auto"));
    m_sz_h = new QSpinBox{};
    m_sz_h->setMaximum(65535);
    m_sz_h->setSpecialValueText(QObject::tr("Auto"));
    showSize(rs);

    sz_lay->addWidget(m_enabled);
    sz_lay->addWidget(m_sz_w);
    sz_lay->addWidget(m_sz_h);
    sz_lay->setAlignment(m_enabled, Qt::AlignRight);
    QObject::connect(m_enabled, SignalUtils::QCheckBox_checkStateChanged(), this, [this](int state) {
      if(state)
      {
        showSize(initialSize());
        update_size();
      }
      else
      {
        showSize(std::nullopt);
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
    m_enabled->setChecked(bool(sz));
    showSize(sz);
  }

  void showSize(std::optional<QSize> sz)
  {
    const QSignalBlocker blck_w{m_sz_w};
    const QSignalBlocker blck_h{m_sz_h};
    if(sz)
      m_lastSize = *sz;
    for(auto* sb : {m_sz_w, m_sz_h})
    {
      sb->setEnabled(bool(sz));
      sb->setMinimum(sz ? 1 : 0);
    }
    m_sz_w->setValue(sz ? sz->width() : 0);
    m_sz_h->setValue(sz ? sz->height() : 0);
  }

  void update_size()
  {
    m_lastSize = QSize{m_sz_w->value(), m_sz_h->value()};
    m_ongoingDispatcher.submit(m_model, m_lastSize);
  }

  void commit() { m_ongoingDispatcher.commit(); }

  QSize initialSize() const noexcept
  {
    if(m_lastSize.width() > 0 && m_lastSize.height() > 0)
      return m_lastSize;
    return QSize{1280, 720};
  }

  const TextureInlet& m_model;
  SingleOngoingCommandDispatcher<ChangeTextureInletRenderSize> m_ongoingDispatcher;

  QCheckBox* m_enabled{};
  QSpinBox* m_sz_w{};
  QSpinBox* m_sz_h{};
  QSize m_lastSize{};
};

void TextureInletFactory::setupInletInspector(
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  lay.addRow(port.name(), Process::makePortAddressCombo(port, ctx, parent));

  auto& inlet = safe_cast<const TextureInlet&>(port);
  // Size
  lay.addRow("Size", new TextureSizeWidget{inlet, ctx, parent});

  // Format
  {
    using enum ossia::texture_format;
    auto combo = new QComboBox{parent};
    combo->addItem(QObject::tr("Auto"), -1);
    combo->addItem("RGBA8", (int)RGBA8);
    combo->addItem("RGBA16F", (int)RGBA16F);
    combo->addItem("RGBA32F", (int)RGBA32F);
    combo->addItem("R8", (int)R8);
    combo->addItem("R16", (int)R16);
    combo->addItem("R16F", (int)R16F);
    combo->addItem("R32F", (int)R32F);
    score::setHelp(
        combo,
        QObject::tr(
            "Format of the render target. Auto uses RGBA8, or the texture "
            "the input process publishes when the inlet may read it directly."));
    auto formatIndex = [combo](std::optional<ossia::texture_format> fmt) {
      return combo->findData(fmt ? (int)*fmt : -1);
    };
    combo->setCurrentIndex(std::max(0, formatIndex(inlet.textureFormat())));
    QObject::connect(
        &inlet, &TextureInlet::textureFormatChanged, combo,
        [combo, formatIndex](std::optional<ossia::texture_format> fmt) {
      if(int idx = formatIndex(fmt); idx >= 0 && idx != combo->currentIndex())
        combo->setCurrentIndex(idx);
    });
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, &inlet, [&ctx, &inlet, combo](int idx) {
      const int data = combo->itemData(idx).toInt();
      std::optional<ossia::texture_format> fmt;
      if(data >= 0)
        fmt = (ossia::texture_format)data;
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

  // Mipmaps
  {
    auto combo = new QComboBox{parent};
    combo->addItem(QObject::tr("None"), (int)ossia::texture_filter::NONE);
    combo->addItem(QObject::tr("Nearest"), (int)ossia::texture_filter::NEAREST);
    combo->addItem(QObject::tr("Linear"), (int)ossia::texture_filter::LINEAR);
    score::setHelp(
        combo,
        QObject::tr(
            "When not None, the render target gets a mip chain, regenerated "
            "every frame, and is sampled across its levels."));
    combo->setCurrentIndex(std::max(0, combo->findData((int)inlet.textureMipmapMode())));

    QObject::connect(
        &inlet, &TextureInlet::textureMipmapModeChanged, combo,
        [combo](ossia::texture_filter mode) {
      if(int idx = combo->findData((int)mode); idx >= 0 && idx != combo->currentIndex())
        combo->setCurrentIndex(idx);
    });
    QObject::connect(
        combo, &QComboBox::currentIndexChanged, &inlet, [&ctx, &inlet, combo](int idx) {
      auto mode = (ossia::texture_filter)combo->itemData(idx).toInt();
      if(mode != inlet.textureMipmapMode())
        CommandDispatcher<>{ctx.commandStack}.submit<ChangeTextureInletMipmapMode>(
            inlet, mode);
    });

    lay.addRow("Mipmaps", combo);
  }
}
void TextureOutletFactory::setupOutletInspector(
    const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  lay.addRow(port.name(), Process::makePortAddressCombo(port, ctx, parent));

  auto& outlet = safe_cast<const TextureOutlet&>(port);
  lay.addRow(new GraphPreviewWidget{outlet, ctx.plugin<Gfx::DocumentPlugin>()});
}
}

template <>
void DataStreamReader::read(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
  m_stream << p.m_renderSize
           << p.m_textureFormat.value_or(ossia::texture_format::RGBA8)
           << p.m_textureAddressMode << p.m_textureFilter
           << p.m_textureFormat.has_value() << p.m_textureMipmapMode;
}
template <>
void DataStreamWriter::write(Gfx::TextureInlet& p)
{
  ossia::texture_format fmt{};
  bool fmt_set{};
  m_stream >> p.m_renderSize >> fmt >> p.m_textureAddressMode >> p.m_textureFilter
      >> fmt_set >> p.m_textureMipmapMode;
  if(fmt_set)
    p.m_textureFormat = fmt;
  else
    p.m_textureFormat = std::nullopt;
}

template <>
void JSONReader::read(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
  if(p.m_renderSize)
  {
    obj["RenderSize"] = *p.m_renderSize;
  }
  obj["Format"] = p.m_textureFormat.value_or(ossia::texture_format::RGBA8);
  obj["FormatSet"] = p.m_textureFormat.has_value();
  obj["Filter"] = p.m_textureFilter;
  obj["AddressMode"] = p.m_textureAddressMode;
  obj["MipmapMode"] = p.m_textureMipmapMode;
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
  p.m_textureFormat = std::nullopt;
  if(auto fmt_v = obj.tryGet("Format"))
  {
    ossia::texture_format fmt{};
    fmt <<= *fmt_v;
    bool fmt_set = fmt != ossia::texture_format::RGBA8;
    if(auto set_v = obj.tryGet("FormatSet"))
      fmt_set = set_v->toBool();
    if(fmt_set)
      p.m_textureFormat = fmt;
    p.m_textureFilter <<= obj["Filter"];
    p.m_textureAddressMode <<= obj["AddressMode"];
  }
  if(auto mip = obj.tryGet("MipmapMode"))
    p.m_textureMipmapMode <<= *mip;
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

GeometryInlet::GeometryInlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Process::Inlet{name, std::move(c), parent}
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

GeometryOutlet::GeometryOutlet(const QString& name, Id<Process::Port> c, QObject* parent)
    : Process::Outlet{name, std::move(c), parent}
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

namespace Gfx
{
void GeometryInletFactory::setupInletInspector(
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  lay.addRow(port.name(), Process::makePortAddressCombo(port, ctx, parent));
}

void GeometryOutletFactory::setupOutletInspector(
    const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  lay.addRow(port.name(), Process::makePortAddressCombo(port, ctx, parent));
}
}
