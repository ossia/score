#pragma once
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Monitoring/EventRing.hpp>
#include <Process/Monitoring/MonitorLink.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/detail/parse_strict.hpp>
#include <Process/Monitoring/Monitor.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneHoverEvent>
#include <QPainter>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/Ring.hpp>
#include <QGraphicsSceneMouseEvent>
#include <Ui/Monitor/Paint.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>


namespace Ui
{
using value_out = halp::callback<"out", ossia::value>;
};
namespace Ui::SignalDisplay
{
enum class Mode
{
  Timeline,
  Trail,
  XY,
  Bars,
  Array,
  Waterfall,
  Histogram
};
inline constexpr std::string_view mode_names[]{
    "Timeline", "Trail", "XY", "Bars", "Array", "Waterfall", "Histogram"};

static constexpr int max_components = 32;
static constexpr int max_array = 2048;

//! One value as received: when, and its components.
struct Sample
{
  //! Position in the parent interval, 0 to 1
  float position{};
  //! Seconds since execution started
  double time{};
  int count{};
  std::array<float, max_components> v{};
};

struct ArrayFrame
{
  int count{};
  std::array<float, max_array> v{};
};

struct SignalMonitor final : Process::Monitor
{
  Process::EventRing<Sample, 2048> samples;
  Process::LatestValue<ArrayFrame> arrays;
};

struct Node
{
  halp_meta(name, "Signal display")
  halp_meta(c_name, "SignalDisplay")
  halp_meta(category, "Monitoring/Control")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/common-practices/"
      "4-audio.html#analysing-an-audio-signal")
  halp_meta(
      description,
      "Plot a value stream: along the timeline, as a rolling trail, as XY, "
      "bars, an array grid or waterfall, or a histogram")
  halp_meta(uuid, "9906e563-ddeb-4ecd-908c-952baee2a0a5")
  halp_flag(fully_custom_item);
  halp_flag(temporal);
  halp_flag(loops_by_default);
  halp_flag(ossia_show_ports_by_default);

  using vec_type = boost::container::small_vector<float, 8>;
  struct
  {
    struct : halp::val_port<"in", std::optional<ossia::value>>
    {
      enum widget
      {
        control
      };
    } port;
    //! Off: each row spans the lowest to the highest value it received.
    halp::toggle<"Fixed range"> fixed;
    halp::spinbox_f32<"Min", halp::range{-1e6, 1e6, 0.}> min;
    halp::spinbox_f32<"Max", halp::range{-1e6, 1e6, 1.}> max;

    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[7]{mode_names[0], mode_names[1], mode_names[2], mode_names[3],
                                   mode_names[4], mode_names[5], mode_names[6]};
        Mode init{Mode::Timeline};
      };
    } mode;
    struct : halp::spinbox_f32<"History", halp::range{0.1, 600., 10.}>
    {
      halp_meta(description, "Seconds shown by the trail, XY and histogram, and over which their range is fitted")
    } history;
    struct : halp::spinbox_i32<"Columns", halp::range{0, 512, 0}>
    {
      halp_meta(description, "Cells per row of the array grid; 0 picks one (32 for a DMX universe)")
    } columns;
    struct : halp::combobox_t<"Palette", Ui::Monitor::ColorMap>
    {
      halp_meta(description, "Colours of the array grid and the waterfall, from the lowest value to the highest")
      struct range
      {
        std::string_view values[6]{"Inferno", "Viridis", "Grey", "Magma", "Turbo", "Blue to red"};
        Ui::Monitor::ColorMap init{Ui::Monitor::ColorMap::Viridis};
      };
    } palette;
  } inputs;

  struct
  {
    struct : halp::val_port<"out", std::optional<vec_type>>
    {
      enum widget
      {
        control
      };
    } port;
  } outputs;

  struct value_visitor
  {
    vec_type& ret;
    void operator()() const noexcept { }
    void operator()(ossia::impulse) const noexcept { ret.push_back(1.f); }
    void operator()(int v) const noexcept { ret.push_back(v); }
    void operator()(float v) const noexcept { ret.push_back(v); }
    void operator()(bool v) const noexcept { ret.push_back(v ? 1.f : 0.f); }
    void operator()(std::string_view v) const noexcept
    {
      if(auto res = ossia::parse_strict<float>(v))
        ret.push_back(*res);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> arr) const noexcept
    {
      ret.insert(ret.end(), arr.begin(), arr.end());
    }
    void operator()(const std::vector<ossia::value>& arr) const noexcept
    {
      ret.reserve(1 + arr.size());
      for(auto& val : arr)
      {
        ret.push_back(ossia::convert<float>(val));
      }
    }
    void operator()(const ossia::value_map_type& arr) const noexcept
    {
      ret.reserve(1 + arr.size());
      for(auto& [k, v] : arr)
      {
        ret.push_back(ossia::convert<float>(v));
      }
    }
  };

  //! The components of a value, without allocating: at most `out.size()`.
  struct fixed_visitor
  {
    float* out;
    int capacity;
    int count{};
    void push(float f) noexcept
    {
      if(count < capacity)
        out[count++] = f;
    }
    void operator()() noexcept { }
    void operator()(ossia::impulse) noexcept { push(1.f); }
    void operator()(int v) noexcept { push(v); }
    void operator()(float v) noexcept { push(v); }
    void operator()(bool v) noexcept { push(v ? 1.f : 0.f); }
    void operator()(std::string_view v) noexcept
    {
      if(auto res = ossia::parse_strict<float>(v))
        push(*res);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> arr) noexcept
    {
      for(float f : arr)
        push(f);
    }
    void operator()(const std::vector<ossia::value>& arr) noexcept
    {
      for(auto& val : arr)
        push(ossia::convert<float>(val));
    }
    void operator()(const ossia::value_map_type& arr) noexcept
    {
      for(auto& [k, v] : arr)
        push(ossia::convert<float>(v));
    }
  };

  //! The values at the bottom and at the top of a row which received values
  //! from seen_min to seen_max. Equal: the row is drawn flat.
  static std::pair<float, float> row_range(
      bool fixed, float min, float max, float seen_min, float seen_max) noexcept
  {
    if(fixed)
      return {min, max};
    return {seen_min, seen_max};
  }

  std::shared_ptr<SignalMonitor> monitor = std::make_shared<SignalMonitor>();
  double elapsed{};
  ArrayFrame array;

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk)
  {
    elapsed += (tk.end_in_flicks - tk.start_in_flicks) / 705600000.;
    if(auto& opt_v = inputs.port.value)
    {
      const auto& v = *opt_v;
      const float val = ossia::convert<float>(v);
      if(ossia::safe_isnan(val) || ossia::safe_isinf(val))
        return;

      if(monitor->watched())
      {
        Sample s;
        s.position = float(tk.relative_position);
        s.time = elapsed;
        fixed_visitor vis{s.v.data(), max_components};
        v.apply(vis);
        s.count = vis.count;
        monitor->samples.push(s);

        if(inputs.mode.value == Mode::Array || inputs.mode.value == Mode::Waterfall)
        {
          fixed_visitor av{array.v.data(), max_array};
          v.apply(av);
          array.count = av.count;
          monitor->arrays.produce(array);
        }
      }

      vec_type ret;
      ret.push_back(float(tk.relative_position));
      v.apply(value_visitor{ret});
      outputs.port.value = static_cast<vec_type&&>(ret);
    }
  }

  struct Layer : public Process::EffectLayerView
  {
    static constexpr int timestamp_index = 0;
    static constexpr int first_value_index = 1;

  public:
    Scenario::IntervalModel* m_interval{};
    // Absent from processes which have not been reloaded since they were added
    const Process::ControlInlet* m_fixed{};
    const Process::ControlInlet* m_min{};
    const Process::ControlInlet* m_max{};
    const Process::ControlInlet* m_mode{};
    const Process::ControlInlet* m_history{};
    const Process::ControlInlet* m_columns{};
    const Process::ControlInlet* m_palette{};
    const score::CommandStackFacade& m_stack;
    Ui::Monitor::DoubleClick m_doubleClick;
    Process::MonitorLink* m_link{};

    // Timeline mode: [position, values...] per value received
    std::vector<vec_type> m_values;
    vec_type min = {0.};
    vec_type max = {1.};
    int num_rows = 0;

    // The other modes: the samples of the last `history` seconds
    boost::circular_buffer<Sample> m_recent;
    std::array<float, max_components> m_seen_min{}, m_seen_max{};
    int m_components{};
    ArrayFrame m_array;
    QImage m_waterfall;
    int m_waterfall_row{};
    uint64_t m_dropped{};
    std::optional<QPointF> m_hover;

    //! Reused by the painting, which would otherwise allocate on every paint
    static constexpr int fade_steps = 16;
    mutable QPolygonF m_polyline;
    mutable std::array<QList<QLineF>, fade_steps> m_fade;

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_interval{Scenario::closestParentInterval(process.parent())}
        , m_stack{doc.commandStack}
    {
      // For the double click; a press goes on to the node
      setAcceptedMouseButtons(Qt::LeftButton);
      setAcceptHoverEvents(true);
      auto inl = process.inlets().front();

      // In the nodal view the layer shows its inlet; in the timeline the
      // process header does
      if(parent && parent->type() == Process::NodeItem::Type)
      {
        inl->displayHandledExplicitly = true;
        Ui::Monitor::showInlet(process, 0, doc, this, this);
      }
      else
      {
        inl->displayHandledExplicitly = false;
      }

      if(m_interval)
      {
        connect(
            m_interval, &Scenario::IntervalModel::executionEvent, this,
            [this](Scenario::IntervalExecutionEvent ev) {
          switch(ev)
          {
            case Scenario::IntervalExecutionEvent::Playing:
            case Scenario::IntervalExecutionEvent::Stopped:
              reset();
              break;
            default:
              break;
          }
        });
      }

      using namespace Ui::Monitor;
      m_fixed = controlInlet(process, 1);
      m_min = controlInlet(process, 2);
      m_max = controlInlet(process, 3);
      m_mode = controlInlet(process, 4);
      m_history = controlInlet(process, 5);
      m_columns = controlInlet(process, 6);
      m_palette = controlInlet(process, 7);
      ModeSwitch::make(m_mode, doc, this, this);
      repaintOn(this, m_fixed, m_min, m_max, m_mode, m_history, m_columns, m_palette);
      // The range is only kept up to date for the modes that read it
      if(m_mode)
        connect(m_mode, &Process::ControlInlet::valueChanged, this, [this] { fitRanges(); });
      // The waterfall keeps the colours its rows were drawn with
      if(m_palette)
        connect(m_palette, &Process::ControlInlet::valueChanged, this, [this] {
          m_waterfall = {};
        });

      m_link = new Process::MonitorLink{
          process, *this,
          [this](Process::Monitor& m) { refresh(static_cast<SignalMonitor&>(m)); },
          [this](Process::Monitor*) { reset(); }, this};
    }

    Mode mode() const { return Ui::Monitor::enumValue(m_mode, Mode::Timeline, Mode::Histogram); }
    const std::array<QRgb, 256>& palette() const
    {
      using namespace Ui::Monitor;
      return colorMap(enumValue(m_palette, ColorMap::Viridis, ColorMap::CoolWarm));
    }
    double history() const { return std::max(0.1f, Ui::Monitor::floatValue(m_history, 10.f)); }
    bool fixedRange() const
    {
      return m_fixed && m_min && m_max && ossia::convert<bool>(m_fixed->value());
    }

    void reset()
    {
      min = {};
      max = {};
      this->num_rows = 0;
      m_values.clear();
      m_recent.clear();
      m_components = 0;
      fitRanges();
      m_waterfall = {};
      update();
    }

    //! A double click starts the histogram over, and fits the other modes'
    //! scales to what they show: the Min and Max of a fixed range, as one
    //! undoable edit, or the range the values span now
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
    {
      if(!m_doubleClick(*ev))
      {
        ev->ignore();
        return;
      }
      ev->accept();
      if(mode() == Mode::Histogram)
      {
        reset();
        return;
      }
      if(!fixedRange())
      {
        fitRanges();
        m_waterfall = {};
        update();
        return;
      }
      float lo = std::numeric_limits<float>::max(), hi = std::numeric_limits<float>::lowest();
      if(mode() == Mode::Array || mode() == Mode::Waterfall)
      {
        for(int i = 0; i < m_array.count; i++)
        {
          lo = std::min(lo, m_array.v[i]);
          hi = std::max(hi, m_array.v[i]);
        }
      }
      else
      {
        for(const auto& s : m_recent)
          for(int i = 0; i < s.count; i++)
          {
            lo = std::min(lo, s.v[i]);
            hi = std::max(hi, s.v[i]);
          }
      }
      if(lo > hi)
        return;
      if(lo == hi)
      {
        lo -= 0.5f;
        hi += 0.5f;
      }
      Ui::Monitor::setControls(m_stack, {{m_min, lo}, {m_max, hi}});
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* ev) override
    {
      m_hover = ev->pos();
      if(mode() == Mode::Array)
        update();
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override
    {
      m_hover.reset();
      update();
    }

    void refresh(SignalMonitor& mon)
    {
      bool changed = false;
      mon.samples.drain([&](const Sample& s) {
        changed = true;
        addTimeline(s);
        addRecent(s);
      });
      m_dropped = mon.samples.dropped();
      if(mon.arrays.consume(m_array))
      {
        changed = true;
        if(mode() == Mode::Waterfall)
          addWaterfallRow();
      }
      // Forget what scrolled out of the history
      if(!m_recent.empty())
      {
        const double horizon = m_recent.back().time - history();
        while(!m_recent.empty() && m_recent.front().time < horizon)
        {
          m_recent.pop_front();
          changed = true;
        }
      }
      if(changed)
      {
        if(usesHistoryRange())
          fitRanges();
        update();
      }
    }

    void addTimeline(const Sample& s)
    {
      const int N = s.count + 1;
      this->num_rows = std::max(this->num_rows, N - 1);
      if(int(min.size()) < N)
        min.resize(N, std::numeric_limits<float>::max());
      if(int(max.size()) < N)
        max.resize(N, std::numeric_limits<float>::lowest());

      vec_type vv;
      vv.resize(N, boost::container::default_init);
      vv[timestamp_index] = s.position;
      for(int i = 0; i < s.count; i++)
      {
        const float r = s.v[i];
        vv[i + 1] = r;
        min[i + 1] = std::min(min[i + 1], r);
        max[i + 1] = std::max(max[i + 1], r);
      }

      // Handle looping: clear when we jump back in time
      if(!m_values.empty() && !m_values.back().empty())
        if(vv[timestamp_index] < m_values.back()[timestamp_index])
        {
          m_values.clear();
          this->num_rows = 0;
        }

      m_values.push_back(std::move(vv));
    }

    void addRecent(const Sample& s)
    {
      m_components = std::max(m_components, s.count);
      Ui::Monitor::pushRing(m_recent, s, 50000);
    }

    bool usesHistoryRange() const
    {
      switch(mode())
      {
        case Mode::Trail:
        case Mode::XY:
        case Mode::Bars:
        case Mode::Histogram:
          return true;
        default:
          return false;
      }
    }

    //! What each component spans over the history shown.
    void fitRanges()
    {
      m_seen_min.fill(std::numeric_limits<float>::max());
      m_seen_max.fill(std::numeric_limits<float>::lowest());
      for(const auto& s : m_recent)
        for(int i = 0; i < s.count; i++)
        {
          m_seen_min[i] = std::min(m_seen_min[i], s.v[i]);
          m_seen_max[i] = std::max(m_seen_max[i], s.v[i]);
        }
      for(int i = 0; i < max_components; i++)
        if(m_seen_min[i] > m_seen_max[i])
        {
          m_seen_min[i] = 0.f;
          m_seen_max[i] = 1.f;
        }
    }

    //! Range of component i: the fixed one, or what the history spans.
    std::pair<float, float> rangeOf(int i) const
    {
      const bool fixed = fixedRange();
      auto r = Node::row_range(
          fixed, fixed ? ossia::convert<float>(m_min->value()) : 0.f,
          fixed ? ossia::convert<float>(m_max->value()) : 1.f,
          i < m_components ? m_seen_min[i] : 0.f, i < m_components ? m_seen_max[i] : 1.f);
      if(r.first == r.second)
      {
        r.first -= 0.5f;
        r.second += 0.5f;
      }
      return r;
    }

    void addWaterfallRow()
    {
      const auto r = boundingRect().adjusted(28, 20, -2, -2);
      const int w = std::max(8, int(r.width())), h = std::max(8, int(r.height()));
      if(m_waterfall.width() != w || m_waterfall.height() != h)
      {
        m_waterfall = QImage(w, h, QImage::Format_RGB32);
        m_waterfall.fill(palette()[0]);
        m_waterfall_row = 0;
      }
      if(m_array.count == 0)
        return;
      const auto& lut = palette();
      auto [lo, hi] = arrayRange();
      // Rows go up the image as they come: from the newest down, the image
      // reads in its own order, wrapping once
      m_waterfall_row = (m_waterfall_row - 1 + h) % h;
      auto* line = reinterpret_cast<QRgb*>(m_waterfall.scanLine(m_waterfall_row));
      for(int x = 0; x < w; x++)
      {
        const int i = std::min(m_array.count - 1, x * m_array.count / w);
        const int idx = std::clamp(int((m_array.v[i] - lo) / (hi - lo) * 255.f), 0, 255);
        line[x] = lut[idx];
      }
    }

    std::pair<float, float> arrayRange() const
    {
      if(fixedRange())
      {
        float lo = ossia::convert<float>(m_min->value()), hi = ossia::convert<float>(m_max->value());
        if(lo == hi)
          hi = lo + 1;
        return {lo, hi};
      }
      float lo = 0, hi = 1;
      for(int i = 0; i < m_array.count; i++)
      {
        lo = std::min(lo, m_array.v[i]);
        hi = std::max(hi, m_array.v[i]);
      }
      // Values that look like DMX or 8-bit colour keep a 0 to 255 range
      if(hi > 1 && hi <= 255 && lo >= 0)
        hi = 255;
      return {lo, hi};
    }

    void draw_row_simple(QPainter* p, qreal w, qreal h, int row_index, auto to_01) const
    {
      for(int start_idx = 0; start_idx < std::ssize(m_values) - 1; start_idx++)
      {
        int end_idx = start_idx + 1;

        const auto* p0 = &m_values[start_idx];
        const auto& v0 = *p0;
        const auto N0 = std::ssize(*p0);
        if(N0 <= row_index)
          continue;
        const auto* p1 = &m_values[end_idx];
        const auto& v1 = *p1;
        const auto N1 = std::ssize(*p1);
        if(N1 <= row_index)
          continue;
        QPointF start = {v0[timestamp_index] * w, to_01(v0[row_index]) * h};
        QPointF end = {v1[timestamp_index] * w, to_01(v1[row_index]) * h};
        p->drawLine(start, end);
      }
    }

    void draw_row_constant(QPainter* p, qreal w, qreal h, int row_index) const
    {
      for(auto& val : m_values)
      {
        if(std::ssize(val) > row_index)
        {
          QPointF p0 = {val[timestamp_index] * w, 0.5 * h};

          p->drawPoint(p0);
        }
      }
    }

    void paint_impl(QPainter* p) const override
    {
      switch(mode())
      {
        case Mode::Timeline:
          paintTimeline(p);
          break;
        case Mode::Trail:
          paintTrail(p);
          break;
        case Mode::XY:
          paintXY(p);
          break;
        case Mode::Bars:
          paintBars(p);
          break;
        case Mode::Array:
          paintArray(p);
          break;
        case Mode::Waterfall:
          paintWaterfall(p);
          break;
        case Mode::Histogram:
          paintHistogram(p);
          break;
      }
      if(m_dropped > 0 && mode() != Mode::Timeline)
        Ui::Monitor::drawReadout(
            *p, boundingRect().bottomLeft() - QPointF{0, 16},
            {QStringLiteral("%1 dropped").arg(m_dropped)});
    }

    void paintTimeline(QPainter* p) const
    {
      if(m_values.size() < 2 || this->num_rows == 0)
        return;

      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(score::Skin::instance().Light.main.pen1_solid_flat_miter);

      const auto w = m_defaultWidth > 0 ? m_defaultWidth : boundingRect().width();
      // Below the strip of the mode switch, as the other modes
      constexpr double top = 20.;
      const auto h = std::max(0., height() - top) / num_rows;
      p->translate(QPointF{0, top});

      const bool fixed = fixedRange();
      const float fixed_min = fixed ? ossia::convert<float>(m_min->value()) : 0.f;
      const float fixed_max = fixed ? ossia::convert<float>(m_max->value()) : 1.f;

      for(int row = 0; row < num_rows; ++row)
      {
        const int row_index = row + 1;

        const auto [min, max] = Node::row_range(
            fixed, fixed_min, fixed_max, this->min[row_index], this->max[row_index]);
        // Values out of a fixed range stay in their row
        if(fixed)
          p->setClipRect(QRectF{0., 0., w, h});
        if(min != max)
        {
          draw_row_simple(p, w, h, row_index, [min, max](float v) {
            return 1.f - (v - min) / (max - min);
          });
        }
        else
        {
          draw_row_constant(p, w, h, row_index);
        }

        p->translate(QPointF{0, h});
      }

      p->setRenderHint(QPainter::Antialiasing, false);
      p->restore();
    }

    void paintTrail(QPainter* p) const
    {
      using namespace Ui::Monitor;
      if(m_recent.empty() || m_components == 0)
        return;
      p->save();
      const QRectF area = boundingRect().adjusted(28, 20, -2, -14);
      const int rows = std::min(m_components, 16);
      const double rh = area.height() / rows;
      const double now = m_recent.back().time;
      const double span = history();
      for(int row = 0; row < rows; row++)
      {
        const QRectF r{area.left(), area.top() + row * rh, area.width(), rh};
        const auto [lo, hi] = rangeOf(row);
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setPen(gridColor());
        p->drawLine(r.bottomLeft(), r.bottomRight());
        // The top of each row, and its bottom when there is room between the
        // two and the next row's top
        if(rh >= 16)
          drawLabel(*p, QRectF(0, r.top(), area.left() - 2, 10), Qt::AlignRight | Qt::AlignTop, QString::number(hi, 'g', 3));
        if(rh >= 40)
          drawLabel(*p, QRectF(0, r.bottom() - 12, area.left() - 2, 10), Qt::AlignRight | Qt::AlignBottom, QString::number(lo, 'g', 3));
        auto& line = m_polyline;
        line.resize(0);
        for(const auto& s : m_recent)
        {
          if(s.count <= row)
            continue;
          const double x = r.right() - (now - s.time) / span * r.width();
          const double y = r.bottom() - std::clamp((s.v[row] - lo) / (hi - lo), 0.f, 1.f) * r.height();
          // Held values are steps: a value stays until the next one comes
          if(!line.isEmpty())
            line << QPointF{x, line.back().y()};
          line << QPointF{x, y};
        }
        if(!line.isEmpty())
          line << QPointF{r.right(), line.back().y()};
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(channelColor(row), 1.2));
        p->drawPolyline(line);
      }
      auto times = linearTicks(0., span, area.width(), 50., " s");
      for(auto& t : times)
        t.label = t.value == 0 ? QStringLiteral("now") : "-" + t.label;
      drawXAxisLabels(*p, area, times, [&](double v) { return area.right() - v / span * area.width(); });
      p->restore();
    }

    void paintXY(QPainter* p) const
    {
      using namespace Ui::Monitor;
      if(m_recent.empty() || m_components < 2)
      {
        drawLabel(*p, boundingRect(), Qt::AlignCenter, QStringLiteral("XY needs two components"));
        return;
      }
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      const QRectF b = boundingRect().adjusted(4, 20, -4, -4);
      const double side = std::min(b.width(), b.height());
      const QRectF r{b.center().x() - side / 2, b.center().y() - side / 2, side, side};
      const bool three = m_components >= 3;
      const auto rx = rangeOf(0), ry = rangeOf(1), rz = three ? rangeOf(2) : std::pair{0.f, 1.f};
      auto norm = [](float v, std::pair<float, float> rr) {
        return std::clamp((v - rr.first) / (rr.second - rr.first), 0.f, 1.f) * 2.f - 1.f;
      };
      // Three components: an isometric view of the unit cube
      auto project = [&](float x, float y, float z) {
        if(!three)
          return QPointF{r.center().x() + x * side / 2, r.center().y() - y * side / 2};
        const double s = side * 0.32;
        return QPointF{
            r.center().x() + (x - z) * 0.866 * s, r.center().y() - (y + (x + z) * 0.5) * s * 0.9};
      };
      p->setPen(gridColor());
      if(three)
      {
        for(float a : {-1.f, 1.f})
          for(float c : {-1.f, 1.f})
          {
            p->drawLine(project(-1, a, c), project(1, a, c));
            p->drawLine(project(a, -1, c), project(a, 1, c));
            p->drawLine(project(a, c, -1), project(a, c, 1));
          }
      }
      else
      {
        p->drawRect(r);
        p->drawLine(QPointF{r.center().x(), r.top()}, QPointF{r.center().x(), r.bottom()});
        p->drawLine(QPointF{r.left(), r.center().y()}, QPointF{r.right(), r.center().y()});
      }
      // Older segments fade, by steps: one pen and one call per step
      const double now = m_recent.back().time, span = history();
      for(auto& b : m_fade)
        b.resize(0);
      QPointF prev;
      bool has_prev = false;
      for(const auto& s : m_recent)
      {
        if(s.count < 2)
          continue;
        const auto pt = project(norm(s.v[0], rx), norm(s.v[1], ry), three ? norm(s.v[2], rz) : 0.f);
        if(has_prev)
        {
          const double alpha = std::clamp(1. - (now - s.time) / span, 0.05, 1.);
          m_fade[std::min(int(alpha * fade_steps), fade_steps - 1)].append(QLineF{prev, pt});
        }
        prev = pt;
        has_prev = true;
      }
      for(int i = 0; i < fade_steps; i++)
      {
        if(m_fade[i].isEmpty())
          continue;
        QColor c = traceColor();
        c.setAlphaF((i + 1.) / fade_steps);
        p->setPen(QPen(c, 1.5));
        p->drawLines(m_fade[i]);
      }
      p->setPen(Qt::NoPen);
      p->setBrush(warnColor());
      p->drawEllipse(prev, 4, 4);
      const auto& last = m_recent.back();
      QStringList info{QStringLiteral("x %1").arg(last.v[0], 0, 'g', 4), QStringLiteral("y %1").arg(last.v[1], 0, 'g', 4)};
      if(three)
        info << QStringLiteral("z %1").arg(last.v[2], 0, 'g', 4);
      if(isUnderMouse())
        drawReadout(*p, b.topLeft(), info);
      p->restore();
    }

    void paintBars(QPainter* p) const
    {
      using namespace Ui::Monitor;
      if(m_recent.empty())
        return;
      const auto& s = m_recent.back();
      if(s.count == 0)
        return;
      p->save();
      const QRectF r = boundingRect().adjusted(4, 20, -4, -14);
      const double bw = r.width() / s.count;
      for(int i = 0; i < s.count; i++)
      {
        const auto [lo, hi] = rangeOf(i);
        const double t = std::clamp((s.v[i] - lo) / (hi - lo), 0.f, 1.f);
        // From zero when the range holds it, else from the bottom
        const double zero = (lo < 0 && hi > 0) ? (-lo) / (hi - lo) : 0.;
        const double y0 = r.bottom() - zero * r.height(), y1 = r.bottom() - t * r.height();
        const QRectF bar{r.left() + i * bw + 1, std::min(y0, y1), std::max(1., bw - 2), std::abs(y1 - y0)};
        p->fillRect(QRectF(r.left() + i * bw + 1, r.top(), std::max(1., bw - 2), r.height()), QColor(30, 30, 32));
        p->fillRect(bar, channelColor(i));
        if(bw >= 28)
          drawLabel(*p, QRectF(r.left() + i * bw, r.bottom() + 1, bw, 12), Qt::AlignHCenter | Qt::AlignTop,
                    QString::number(s.v[i], 'g', 3));
      }
      p->restore();
    }

    struct Grid
    {
      int cols{}, rows{};
      double cell{};
      QRectF area;
    };
    Grid arrayGrid() const
    {
      Grid g;
      g.area = boundingRect().adjusted(2, 20, -2, -2);
      const int n = std::max(1, m_array.count);
      int cols = m_columns ? ossia::convert<int>(m_columns->value()) : 0;
      if(cols <= 0)
        cols = n == 512 ? 32 : std::max(1, int(std::ceil(std::sqrt(n * g.area.width() / std::max(1., g.area.height())))));
      g.cols = cols;
      g.rows = (n + cols - 1) / cols;
      g.cell = std::max(1., std::min(g.area.width() / g.cols, g.area.height() / g.rows));
      return g;
    }

    void paintArray(QPainter* p) const
    {
      using namespace Ui::Monitor;
      if(m_array.count == 0)
        return;
      p->save();
      const auto g = arrayGrid();
      const auto [lo, hi] = arrayRange();
      const auto& lut = palette();
      const double gap = g.cell >= 8 ? 1 : 0;
      int hovered = -1;
      QFont f = smallFont();
      f.setPixelSize(int(std::clamp(g.cell * 0.4, 6., 10.)));
      p->setFont(f);
      for(int i = 0; i < m_array.count; i++)
      {
        const QRectF cell{g.area.left() + (i % g.cols) * g.cell, g.area.top() + (i / g.cols) * g.cell, g.cell - gap, g.cell - gap};
        const int idx = std::clamp(int((m_array.v[i] - lo) / (hi - lo) * 255.f), 0, 255);
        p->fillRect(cell, QColor::fromRgb(lut[idx]));
        if(g.cell >= 22)
        {
          p->setPen(qGray(lut[idx]) > 140 ? Qt::black : Qt::white);
          p->drawText(cell, Qt::AlignCenter, QString::number(m_array.v[i], 'g', 3));
        }
        if(m_hover && cell.contains(*m_hover))
          hovered = i;
      }
      QString head = QStringLiteral("%1 values, %2 to %3").arg(m_array.count).arg(lo).arg(hi);
      if(hovered >= 0)
        head = QStringLiteral("[%1] = %2").arg(hovered).arg(m_array.v[hovered], 0, 'g', 6);
      if(isUnderMouse())
        drawLabel(*p, QRectF(2, 0, boundingRect().width() - 6, 13), Qt::AlignRight | Qt::AlignVCenter, head);
      p->restore();
    }

    void paintWaterfall(QPainter* p) const
    {
      using namespace Ui::Monitor;
      if(m_waterfall.isNull())
        return;
      const QRectF r = boundingRect().adjusted(28, 20, -2, -2);
      // Newest row at the top: from it to the end of the image, then the
      // rows from the start of the image, which are the oldest
      const int h = m_waterfall.height(), w = m_waterfall.width();
      const int newest = m_waterfall_row, upper = h - newest;
      p->drawImage(QRectF(r.left(), r.top(), w, upper), m_waterfall, QRectF(0, newest, w, upper));
      if(newest > 0)
        p->drawImage(
            QRectF(r.left(), r.top() + upper, w, newest), m_waterfall, QRectF(0, 0, w, newest));
    }

    void paintHistogram(QPainter* p) const
    {
      using namespace Ui::Monitor;
      if(m_recent.empty())
        return;
      p->save();
      const QRectF r = boundingRect().adjusted(4, 20, -4, -14);
      const auto [lo, hi] = rangeOf(0);
      std::array<int, 64> bins{};
      double sum = 0, sum2 = 0;
      int n = 0;
      for(const auto& s : m_recent)
      {
        if(s.count == 0)
          continue;
        const float v = s.v[0];
        bins[std::clamp(int((v - lo) / (hi - lo) * bins.size()), 0, int(bins.size()) - 1)]++;
        sum += v;
        sum2 += v * v;
        n++;
      }
      const int peak = std::max(1, *std::max_element(bins.begin(), bins.end()));
      const double bw = r.width() / bins.size();
      for(std::size_t i = 0; i < bins.size(); i++)
      {
        const double h = double(bins[i]) / peak * r.height();
        p->fillRect(QRectF(r.left() + i * bw, r.bottom() - h, std::max(1., bw - 1), h), traceColor());
      }
      drawLabel(*p, QRectF(r.left(), r.bottom() + 1, 80, 12), Qt::AlignLeft | Qt::AlignTop, QString::number(lo, 'g', 3));
      drawLabel(*p, QRectF(r.right() - 80, r.bottom() + 1, 80, 12), Qt::AlignRight | Qt::AlignTop, QString::number(hi, 'g', 3));
      const double mean = n ? sum / n : 0, sd = n ? std::sqrt(std::max(0., sum2 / n - mean * mean)) : 0;
      if(isUnderMouse())
        drawReadout(*p, r.topLeft(),
                  {QStringLiteral("n %1").arg(n), QStringLiteral("mean %1").arg(mean, 0, 'g', 4),
                   QStringLiteral("sd %1").arg(sd, 0, 'g', 4),
                   QStringLiteral("min %1  max %2").arg(m_seen_min[0], 0, 'g', 4).arg(m_seen_max[0], 0, 'g', 4)});
      p->restore();
    }
  };
};
}
