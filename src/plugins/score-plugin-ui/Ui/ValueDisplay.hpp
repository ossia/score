#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <State/ValuePrettyPrint.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <boost/container/devector.hpp>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QPainter>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

namespace Ui::ValueDisplay
{
template <halp::static_string lit, typename T>
struct raw_port
{
  halp_meta(is_event, true)
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  T* value{};
};

struct Node
{
  halp_meta(name, "Value display")
  halp_meta(c_name, "Display")
  halp_meta(category, "Monitoring")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Visualize an input value")
  halp_meta(uuid, "3f4a41f2-fa39-420f-ab0f-0af6b8409edb")
  halp_flag(fully_custom_item);

  static constexpr int max_log = 100;

  struct
  {
    // Reading the port directly rather than through a control input: a control
    // input is fed with get_data().back(), so only the last value of a tick
    // would ever be seen, and a burst - a note-off immediately followed by a
    // note-on, several values written at the same sample - would show up as a
    // single entry.
    raw_port<"in", ossia::value_port> port;

    halp::spinbox_i32<"Log", halp::range{1, max_log, 1}> log;

    // Spread nested lists and maps over indented lines instead of a single one.
    halp::toggle<"Pretty"> pretty;
  } inputs;

  struct
  {
    // [sequence number of the first entry, values...]
    struct : halp::val_port<"values", std::optional<ossia::value>>
    {
      enum widget
      {
        control
      };
    } values;
  } outputs;

  // The engine -> UI queue for control outputs is drained keeping only the last
  // entry, so a push has to carry everything the layer may still need. That is
  // never more than the log length, which bounds the ring exactly.
  boost::container::devector<ossia::value> m_ring;
  int m_next_seq = 0;

  void prepare(halp::setup) noexcept
  {
    m_ring.clear();
    m_next_seq = 0;
  }

  void operator()()
  {
    outputs.values.value.reset();

    if(!inputs.port.value)
      return;

    const auto& data = inputs.port.value->get_data();
    if(data.empty())
      return;

    // Never more than what the layer displays: with a log of 1 this sends the
    // last value and nothing else.
    const int keep = std::clamp(inputs.log.value, 1, max_log);

    for(const ossia::timed_value& tv : data)
    {
      m_ring.push_back(tv.value);
      m_next_seq++;
      if(std::ssize(m_ring) > keep)
        m_ring.pop_front();
    }
    while(std::ssize(m_ring) > keep)
      m_ring.pop_front();

    std::vector<ossia::value> payload;
    payload.reserve(1 + m_ring.size());
    payload.push_back(int(m_next_seq - std::ssize(m_ring)));
    for(const auto& v : m_ring)
      payload.push_back(v);

    outputs.values.value = ossia::value{std::move(payload)};
  }

  struct Layer : public Process::EffectLayerView
  {
  public:
    boost::container::devector<ossia::value> values;
    int m_next_seq = 0;

    Process::ControlInlet* log_inlet{};
    Process::ControlInlet* pretty_inlet{};

    // The rendered text. Rebuilt when the values, the log length or the pretty
    // toggle change - not on every paint - through m_buf, whose capacity is
    // kept across rebuilds.
    QString txt_cache;
    std::string m_buf;

    int logging() const noexcept
    {
      return std::clamp(ossia::convert<int>(log_inlet->value()), 1, max_log);
    }

    bool pretty() const noexcept
    {
      return pretty_inlet && ossia::convert<bool>(pretty_inlet->value());
    }

    void rebuildText()
    {
      m_buf.clear();
      if(pretty())
      {
        for(auto& line : this->values)
        {
          State::prettyPrintValue(m_buf, line);
          m_buf.push_back('\n');
        }
      }
      else
      {
        for(auto& line : this->values)
        {
          State::printValue(m_buf, line);
          m_buf.push_back('\n');
        }
      }
      txt_cache = QString::fromUtf8(m_buf.data(), m_buf.size());
      update();
    }

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons(Qt::NoButton);

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto& value_inlet = *process.inlets()[0];
      if(auto* fact = portFactory.get(value_inlet.concreteKey()))
        if(auto* port = fact->makePortItem(value_inlet, doc, this, this))
          port->setPos(0, 5);

      log_inlet = static_cast<Process::ControlInlet*>(process.inlets()[1]);
      if(process.inlets().size() > 2)
        pretty_inlet = qobject_cast<Process::ControlInlet*>(process.inlets()[2]);

      auto* out = static_cast<Process::ControlOutlet*>(process.outlets()[0]);
      connect(
          out, &Process::ControlOutlet::valueChanged, this,
          [this](const ossia::value& v) { on_values(v); });

      connect(
          log_inlet, &Process::ControlInlet::valueChanged, this,
          [this](const ossia::value& v) {
        while(std::ssize(values) > logging())
          values.pop_back();
        rebuildText();
          });

      if(pretty_inlet)
        connect(
            pretty_inlet, &Process::ControlInlet::valueChanged, this,
            [this](const ossia::value&) { rebuildText(); });
    }

    void on_values(const ossia::value& v)
    {
      auto* list = v.target<std::vector<ossia::value>>();
      if(!list || list->size() < 2)
        return;

      const int base = ossia::convert<int>((*list)[0]);
      const int n = std::ssize(*list) - 1;

      // The batch is entirely older than what we hold: the process restarted.
      if(base + n < m_next_seq)
      {
        values.clear();
        m_next_seq = base;
      }

      for(int i = 0; i < n; i++)
      {
        const int seq = base + i;
        if(seq < m_next_seq)
          continue;
        values.push_front((*list)[i + 1]);
        m_next_seq = seq + 1;
      }

      while(std::ssize(values) > logging())
        values.pop_back();

      rebuildText();
    }

    void reset()
    {
      values.clear();
      m_next_seq = 0;
      rebuildText();
    }

    void paint_impl(QPainter* p) const override
    {
      if(txt_cache.isEmpty())
        return;

      const auto& skin = score::Skin::instance();
      p->setFont(skin.MonoFontSmall);
      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(skin.Light.main.pen1_solid_flat_miter);
      p->drawText(boundingRect().adjusted(10, 0, 0, 0), txt_cache);
      p->setRenderHint(QPainter::Antialiasing, false);
    }
  };

  struct Presenter : public Process::EffectLayerPresenter
  {
    using Process::EffectLayerPresenter::EffectLayerPresenter;
    void fillContextMenu(
        QMenu& menu, QPoint pos, QPointF scenepos,
        const Process::LayerContextMenuManager&) override
    {
      auto cp = menu.addAction(tr("Copy value"));
      connect(cp, &QAction::triggered, this, [this] {
        auto& v = *static_cast<Layer*>(this->m_view);
        qApp->clipboard()->setText(v.txt_cache);
      });
    }
  };
};
}
