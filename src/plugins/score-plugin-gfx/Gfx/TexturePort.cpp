#include <Process/PreviewSettings.hpp>
#include "TexturePort.hpp"

#include "GfxDevice.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Process/Dataflow/PortAddressComboBox.hpp>
#include <Process/Process.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/Utils.hpp>
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
}
TextureInlet::TextureInlet(JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(DataStream::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

ShaderSamplerSource::~ShaderSamplerSource() = default;

const isf::sampler_config*
declaredSampler(const isf::descriptor& desc, const QString& name) noexcept
{
  const auto n = name.toStdString();
  for(const auto& inp : desc.inputs)
    if(inp.name == n)
      if(auto* cfg = score::gfx::inputSamplerConfig(inp);
         cfg && score::gfx::declaresSampler(*cfg))
        return cfg;
  return nullptr;
}

const isf::sampler_config* TextureInlet::shaderSampler() const noexcept
{
  if(auto* src = dynamic_cast<const ShaderSamplerSource*>(parent()))
    return src->shaderSampler(*this);
  return nullptr;
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
  // A field left unset is sent as such: the renderer then takes the
  // shader's, or the default (score::gfx::resolveSamplerFields).
  const auto unsetFilter = (ossia::texture_filter)score::gfx::unsetSamplerField;
  const auto unsetAddress = (ossia::texture_address_mode)score::gfx::unsetSamplerField;
  const auto address = m_textureAddressMode.value_or(unsetAddress);
  exec.data.address_u = address;
  exec.data.address_v = address;
  exec.data.address_w = address;
  exec.data.min_filter = m_textureFilter.value_or(unsetFilter);
  exec.data.mag_filter = m_textureFilter.value_or(unsetFilter);
  exec.data.mipmap_mode = m_textureMipmapMode.value_or(unsetFilter);
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
      [&exec, unsetFilter](std::optional<ossia::texture_filter> v) {
    exec.data.min_filter = v.value_or(unsetFilter);
    exec.data.mag_filter = v.value_or(unsetFilter);
  });

  connect(
      this, &TextureInlet::textureMipmapModeChanged, exec_context,
      [&exec, unsetFilter](std::optional<ossia::texture_filter> v) {
    exec.data.mipmap_mode = v.value_or(unsetFilter);
  });

  connect(
      this, &TextureInlet::textureAddressModeChanged, exec_context,
      [&exec, unsetAddress](std::optional<ossia::texture_address_mode> v) {
    exec.data.address_u = v.value_or(unsetAddress);
    exec.data.address_v = v.value_or(unsetAddress);
    exec.data.address_w = v.value_or(unsetAddress);
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
      // A spin box shows its special text for its minimum: with a size set
      // that is 1, which must not read "Auto".
      sb->setSpecialValueText(sz ? QString{} : QObject::tr("Auto"));
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

namespace
{
// What an inlet's unset sampler fields resolve to: the shader's JSON, where
// it declares them, else the defaults (shader board U34).
struct UnsetSampler
{
  score::gfx::RenderTargetSpecs resolved;
  bool filterFromShader{}, addressFromShader{}, mipmapFromShader{}, compare{};
};

UnsetSampler unsetSampler(const TextureInlet& inlet)
{
  UnsetSampler u;
  u.resolved.address_u = u.resolved.address_v = u.resolved.address_w
      = QRhiSampler::ClampToEdge;
  if(auto* cfg = inlet.shaderSampler())
  {
    score::gfx::resolveSamplerFields(u.resolved, *cfg);
    u.filterFromShader
        = !cfg->filter.empty() || !cfg->min_filter.empty() || !cfg->mag_filter.empty();
    u.addressFromShader = !cfg->wrap.empty() || !cfg->wrap_s.empty()
                          || !cfg->wrap_t.empty() || !cfg->wrap_r.empty();
    u.mipmapFromShader = !cfg->mipmap_mode.empty();
    u.compare = score::gfx::declaresCompare(*cfg);
  }
  return u;
}

QString filterName(QRhiSampler::Filter f)
{
  switch(f)
  {
    case QRhiSampler::None:
      return QObject::tr("None");
    case QRhiSampler::Nearest:
      return QObject::tr("Nearest");
    default:
      return QObject::tr("Linear");
  }
}

QString addressName(QRhiSampler::AddressMode a)
{
  switch(a)
  {
    case QRhiSampler::Repeat:
      return QObject::tr("Repeat");
    case QRhiSampler::Mirror:
      return QObject::tr("Mirror");
    default:
      return QObject::tr("Clamp to edge");
  }
}

//! The first entry of a sampler field's combo box: the field left unset,
//! named after what it resolves to.
QString autoEntry(bool fromShader, const QString& value)
{
  return fromShader ? QObject::tr("Shader: %1").arg(value)
                    : QObject::tr("Default: %1").arg(value);
}

//! A comparison sampler is the shader's: its rows are shown, not editable.
void disableIfCompare(const UnsetSampler& u, QWidget* w)
{
  if(!u.compare)
    return;
  w->setEnabled(false);
  w->setToolTip(QObject::tr(
      "Set by the shader: it samples this input with a depth comparison "
      "(COMPARE)."));
}

//! A combo box over a sampler field: entry 0 leaves it unset, the others set
//! it to their value. Picking one submits Command.
template <typename Command, typename T>
QComboBox* samplerFieldCombo(
    const TextureInlet& inlet, const score::DocumentContext& ctx, QWidget* parent,
    const QString& autoText, std::vector<std::pair<QString, T>> values,
    std::optional<T> (TextureInlet::*get)() const,
    void (TextureInlet::*changed)(std::optional<T>))
{
  auto combo = new QComboBox{parent};
  combo->addItem(autoText, -1);
  for(auto& [text, v] : values)
    combo->addItem(text, (int)v);
  combo->setItemData(
      0,
      QObject::tr("Leave the setting to the shader, or the default when the "
                  "shader declares none."),
      Qt::ToolTipRole);
  auto indexOf = [combo](std::optional<T> v) {
    return v ? std::max(0, combo->findData((int)*v)) : 0;
  };
  combo->setCurrentIndex(indexOf((inlet.*get)()));
  QObject::connect(&inlet, changed, combo, [combo, indexOf](std::optional<T> v) {
    if(int idx = indexOf(v); idx != combo->currentIndex())
      combo->setCurrentIndex(idx);
  });
  QObject::connect(
      combo, &QComboBox::currentIndexChanged, &inlet,
      [&ctx, &inlet, combo, get](int idx) {
    const int data = combo->itemData(idx).toInt();
    std::optional<T> v;
    if(data >= 0)
      v = (T)data;
    if(v != (inlet.*get)())
      CommandDispatcher<>{ctx.commandStack}.submit<Command>(inlet, v);
  });
  return combo;
}
}

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

  const auto unset = unsetSampler(inlet);

  // Filter
  {
    using enum ossia::texture_filter;
    auto combo = samplerFieldCombo<ChangeTextureInletFilter, ossia::texture_filter>(
        inlet, ctx, parent,
        autoEntry(unset.filterFromShader, filterName(unset.resolved.mag_filter)),
        {{QObject::tr("Nearest"), NEAREST}, {QObject::tr("Linear"), LINEAR}},
        &TextureInlet::textureFilter, &TextureInlet::textureFilterChanged);
    disableIfCompare(unset, combo);
    lay.addRow("Filter", combo);
  }

  // Address mode
  {
    using enum ossia::texture_address_mode;
    auto combo
        = samplerFieldCombo<ChangeTextureInletAddressMode, ossia::texture_address_mode>(
            inlet, ctx, parent,
            autoEntry(unset.addressFromShader, addressName(unset.resolved.address_u)),
            {{QObject::tr("Repeat"), REPEAT},
             {QObject::tr("Clamp to edge"), CLAMP_TO_EDGE},
             {QObject::tr("Mirror"), MIRROR}},
            &TextureInlet::textureAddressMode, &TextureInlet::textureAddressModeChanged);
    disableIfCompare(unset, combo);
    lay.addRow("Address mode", combo);
  }

  // Mipmaps
  {
    using enum ossia::texture_filter;
    auto combo = samplerFieldCombo<ChangeTextureInletMipmapMode, ossia::texture_filter>(
        inlet, ctx, parent,
        autoEntry(unset.mipmapFromShader, filterName(unset.resolved.mipmap_mode)),
        {{QObject::tr("None"), NONE},
         {QObject::tr("Nearest"), NEAREST},
         {QObject::tr("Linear"), LINEAR}},
        &TextureInlet::textureMipmapMode, &TextureInlet::textureMipmapModeChanged);
    score::setHelp(
        combo,
        QObject::tr(
            "When not None, the render target gets a mip chain, regenerated "
            "every frame, and is sampled across its levels."));
    disableIfCompare(unset, combo);
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

namespace Gfx
{
namespace
{
constexpr auto defaultFilter = ossia::texture_filter::LINEAR;
constexpr auto defaultAddressMode = ossia::texture_address_mode::CLAMP_TO_EDGE;
constexpr auto defaultMipmapMode = ossia::texture_filter::NONE;

// A filter saved as None is nearest (None is only a mipmap mode).
ossia::texture_filter savedFilter(ossia::texture_filter f) noexcept
{
  return f == ossia::texture_filter::NONE ? ossia::texture_filter::NEAREST : f;
}
}
}

template <>
void DataStreamReader::read(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
  m_stream << p.m_renderSize
           << p.m_textureFormat.value_or(ossia::texture_format::RGBA8)
           << p.m_textureAddressMode.value_or(Gfx::defaultAddressMode)
           << p.m_textureFilter.value_or(Gfx::defaultFilter)
           << p.m_textureFormat.has_value()
           << p.m_textureMipmapMode.value_or(Gfx::defaultMipmapMode)
           << p.m_textureAddressMode.has_value() << p.m_textureFilter.has_value()
           << p.m_textureMipmapMode.has_value();
}
template <>
void DataStreamWriter::write(Gfx::TextureInlet& p)
{
  ossia::texture_format fmt{};
  bool fmt_set{};
  ossia::texture_address_mode address{};
  ossia::texture_filter filter{}, mipmap{};
  bool address_set{}, filter_set{}, mipmap_set{};
  m_stream >> p.m_renderSize >> fmt >> address >> filter >> fmt_set >> mipmap
      >> address_set >> filter_set >> mipmap_set;
  if(fmt_set)
    p.m_textureFormat = fmt;
  else
    p.m_textureFormat = std::nullopt;
  p.m_textureAddressMode = address_set ? std::optional{address} : std::nullopt;
  p.m_textureFilter = filter_set ? std::optional{Gfx::savedFilter(filter)} : std::nullopt;
  p.m_textureMipmapMode = mipmap_set ? std::optional{mipmap} : std::nullopt;
}

// "Filter", "AddressMode" and "MipmapMode" hold the value used when the shader
// declares none, as documents always saved them; "FilterSet", ... whether the
// user set it. A document without those, saved before, set what differs from
// the default (shader board U34).
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
  obj["Filter"] = p.m_textureFilter.value_or(Gfx::defaultFilter);
  obj["FilterSet"] = p.m_textureFilter.has_value();
  obj["AddressMode"] = p.m_textureAddressMode.value_or(Gfx::defaultAddressMode);
  obj["AddressModeSet"] = p.m_textureAddressMode.has_value();
  obj["MipmapMode"] = p.m_textureMipmapMode.value_or(Gfx::defaultMipmapMode);
  obj["MipmapModeSet"] = p.m_textureMipmapMode.has_value();
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
  }

  const auto field = [this]<typename T>(
                         const char* key, const char* setKey, T def, std::optional<T>& out) {
    out = std::nullopt;
    auto v = obj.tryGet(std::string{key});
    if(!v)
      return;
    T value{};
    value <<= *v;
    bool set = value != def;
    if(auto s = obj.tryGet(std::string{setKey}))
      set = s->toBool();
    if(set)
      out = value;
  };
  field("Filter", "FilterSet", Gfx::defaultFilter, p.m_textureFilter);
  if(p.m_textureFilter)
    p.m_textureFilter = Gfx::savedFilter(*p.m_textureFilter);
  field("AddressMode", "AddressModeSet", Gfx::defaultAddressMode, p.m_textureAddressMode);
  field("MipmapMode", "MipmapModeSet", Gfx::defaultMipmapMode, p.m_textureMipmapMode);
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
