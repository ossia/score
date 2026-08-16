#include "ValueEditors.hpp"

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/IntSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Explorer::AddressValueWidget)

namespace Explorer
{
namespace
{
ossia::value defaultValueOf(const Device::AddressSettingsCommon& addr)
{
  if(auto v = ossia::net::get_default_value(addr.extendedAttributes))
    return *v;
  return {};
}

double asDouble(const ossia::value& v, double fallback) noexcept
{
  return v.valid() ? ossia::convert<float>(v) : fallback;
}

int asInt(const ossia::value& v, int fallback) noexcept
{
  return v.valid() ? ossia::convert<int>(v) : fallback;
}

constexpr double unbounded_min = std::numeric_limits<double>::lowest();

constexpr double unbounded_max = std::numeric_limits<double>::max();

using range = std::pair<double, double>;

//! The map form, which the value parser has no rule for.
std::optional<ossia::value> readMap(const QString& text)
{
  const auto t = text.trimmed();
  if(!t.startsWith('{') || !t.endsWith('}'))
    return std::nullopt;

  ossia::value_map_type map;
  const auto body = t.mid(1, t.size() - 2).trimmed();
  if(body.isEmpty())
    return ossia::value{map};

  int depth = 0;
  bool quoted = false;
  QString cur;
  QStringList entries;
  for(QChar c : body)
  {
    if(c == '"')
      quoted = !quoted;
    else if(!quoted && (c == '[' || c == '{'))
      depth++;
    else if(!quoted && (c == ']' || c == '}'))
      depth--;

    if(c == ',' && depth == 0 && !quoted)
    {
      entries.push_back(cur);
      cur.clear();
    }
    else
    {
      cur += c;
    }
  }
  entries.push_back(cur);

  for(const auto& entry : entries)
  {
    const int sep = entry.indexOf(':');
    if(sep < 0)
      return std::nullopt;

    auto key = entry.left(sep).trimmed();
    if(key.startsWith('"') && key.endsWith('"') && key.size() >= 2)
      key = key.mid(1, key.size() - 2);

    auto val = State::parseValue(entry.mid(sep + 1).trimmed().toStdString());
    if(!val)
      return std::nullopt;

    map.emplace_back(key.toStdString(), *val);
  }
  return ossia::value{map};
}

//! Text read as a value of that type, or nothing when it cannot be.
std::optional<ossia::value> readAs(const QString& text, ossia::val_type type)
{
  switch(type)
  {
    case ossia::val_type::STRING:
      return ossia::value{text.toStdString()};
    case ossia::val_type::INT: {
      bool ok = false;
      const int v = text.toInt(&ok);
      return ok ? std::optional<ossia::value>{v} : std::nullopt;
    }
    case ossia::val_type::FLOAT: {
      bool ok = false;
      const float v = text.toFloat(&ok);
      return ok ? std::optional<ossia::value>{v} : std::nullopt;
    }
    case ossia::val_type::MAP:
      return readMap(text);
    default:
      break;
  }

  auto parsed = State::parseValue(text.toStdString());
  if(!parsed || parsed->get_type() != type)
    return std::nullopt;
  return parsed;
}

template <typename Box, typename T>
void widen(Box& box, T v)
{
  if(v < box.minimum())
    box.setMinimum(v);
  if(v > box.maximum())
    box.setMaximum(v);
}

// A vecf_domain bounds each component on its own; a scalar one bounds them all.
template <std::size_t N>
std::array<range, N>
vecRanges(const ossia::domain& dom, double lo, double hi, bool bounded)
{
  std::array<range, N> r;
  r.fill(bounded ? range{lo, hi} : range{unbounded_min, unbounded_max});

  if(auto* vd = dom.v.target<ossia::vecf_domain<N>>())
  {
    for(std::size_t i = 0; i < N; i++)
    {
      r[i] = {vd->min[i] ? *vd->min[i] : unbounded_min,
              vd->max[i] ? *vd->max[i] : unbounded_max};
    }
  }
  return r;
}

// The values the domain lists, when they are values the parameter can take: a
// vecf_domain lists bounds per component, not whole values.
std::vector<ossia::value> listedValues(const Device::AddressSettingsCommon& addr)
{
  if(addr.value.get_type() == ossia::val_type::BOOL)
    return {};

  auto vals = ossia::get_values(addr.domain.get());
  if(vals.empty())
    return {};
  if(addr.value.valid() && !vals.front().valid())
    return {};
  if(addr.value.valid() && vals.front().get_type() != addr.value.get_type())
    return {};
  return vals;
}


class IntSpinValueWidget final : public AddressValueWidget
{
public:
  IntSpinValueWidget(int min, int max, QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setRange(min, max);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    connect(&m_edit, SignalUtils::QSpinBox_valueChanged_int(), this, [this] {
      markEdited();
    });
  }

  ossia::value getImpl() const override { return m_edit.value(); }
  void setImpl(ossia::value t) override
  {
    widen(m_edit, ossia::convert<int>(t));
    m_edit.setValue(ossia::convert<int>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QSpinBox m_edit;
};

class FloatSpinValueWidget final : public AddressValueWidget
{
public:
  FloatSpinValueWidget(double min, double max, QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setRange(min, max);
    m_edit.setDecimals(6);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    connect(
        &m_edit, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
        [this] { markEdited(); });
  }

  ossia::value getImpl() const override { return (float)m_edit.value(); }
  void setImpl(ossia::value t) override
  {
    widen(m_edit, ossia::convert<float>(t));
    m_edit.setValue(ossia::convert<float>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QDoubleSpinBox m_edit;
};

class SliderValueWidget final : public AddressValueWidget
{
public:
  SliderValueWidget(int min, int max, int init, QWidget* parent)
      : AddressValueWidget{parent}
      , m_slider{this}
  {
    m_slider.setOrientation(Qt::Horizontal);
    m_slider.setRange(min, max, init);
    m_edit.setRange(min, max);

    m_slider.setContentsMargins(0, 0, 0, 0);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);

    connect(&m_slider, &score::IntSlider::valueChanged, this, [this](int v) {
      m_edit.setValue(v);
    });

    connect(&m_edit, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int v) {
      m_slider.setValue(v);
      markEdited();
    });

    m_lay.addWidget(&m_slider);
    m_lay.addWidget(&m_edit);
  }

  ossia::value getImpl() const override { return m_edit.value(); }

  void setImpl(ossia::value t) override
  {
    widen(m_edit, ossia::convert<int>(t));
    m_edit.setValue(ossia::convert<int>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  score::IntSlider m_slider;
  QSpinBox m_edit;
};

class DoubleSliderValueWidget final : public AddressValueWidget
{
public:
  DoubleSliderValueWidget(double min, double max, double init, QWidget* parent)
      : AddressValueWidget{parent}
      , m_slider{this}
  {
    m_slider.setOrientation(Qt::Horizontal);
    m_slider.setRange(min, max, init);
    m_edit.setRange(min, max);
    m_edit.setDecimals(6);

    m_slider.setContentsMargins(0, 0, 0, 0);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);

    connect(
        &m_slider, &score::DoubleSlider::valueChanged, this,
        [this, min, max](double v) { m_edit.setValue(min + v * (max - min)); });

    connect(
        &m_edit, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
        [this, min, max](double v) {
      m_slider.setValue(max > min ? (v - min) / (max - min) : 0.);
      markEdited();
        });

    m_lay.addWidget(&m_slider);
    m_lay.addWidget(&m_edit);
  }

  ossia::value getImpl() const override { return (float)m_edit.value(); }

  void setImpl(ossia::value t) override
  {
    widen(m_edit, ossia::convert<float>(t));
    m_edit.setValue(ossia::convert<float>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  score::DoubleSlider m_slider;
  QDoubleSpinBox m_edit;
};


class ComboValueWidget final : public AddressValueWidget
{
public:
  ComboValueWidget(std::vector<ossia::value> values, QWidget* parent)
      : AddressValueWidget{parent}
      // Parentheses: braces would pick the initializer_list overload.
      , m_values(std::move(values))
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    m_edit.setEditable(true);
    m_edit.setInsertPolicy(QComboBox::NoInsert);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    for(auto& v : m_values)
      m_edit.addItem(State::convert::toDisplayString(v));

    connect(&m_edit, &QComboBox::currentTextChanged, this, [this] { markEdited(); });
  }

  ossia::value getImpl() const override
  {
    const int idx = m_edit.currentIndex();
    if(idx >= 0 && idx < std::ssize(m_values)
       && m_edit.currentText() == m_edit.itemText(idx))
      return m_values[idx];

    const auto type
        = m_values.empty() ? ossia::val_type::STRING : m_values.front().get_type();
    return readAs(m_edit.currentText(), type).value_or(ossia::value{});
  }

  void setImpl(ossia::value t) override
  {
    const int idx = ossia::index_in_container(m_values, t);
    if(idx != -1)
      m_edit.setCurrentIndex(idx);
    else
      m_edit.setCurrentText(State::convert::toDisplayString(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QComboBox m_edit;
  std::vector<ossia::value> m_values;
};

class BoolValueWidget final : public AddressValueWidget
{
public:
  explicit BoolValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.addItem(QObject::tr("false"), false);
    m_edit.addItem(QObject::tr("true"), true);
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    connect(
        &m_edit, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this] { markEdited(); });
  }

  ossia::value getImpl() const override { return m_edit.currentIndex() == 1; }
  void setImpl(ossia::value t) override
  {
    m_edit.setCurrentIndex(ossia::convert<bool>(t) ? 1 : 0);
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QComboBox m_edit;
};

class StringValueWidget final : public AddressValueWidget
{
public:
  explicit StringValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    connect(&m_edit, &QLineEdit::textEdited, this, [this] { markEdited(); });
  }

  // The text is the value: a string that looks like a list stays a string.
  ossia::value getImpl() const override { return m_edit.text().toStdString(); }
  void setImpl(ossia::value t) override
  {
    m_edit.setText(State::convert::value<QString>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QLineEdit m_edit;
};


//! Anything whose textual form round-trips through the value parser.
class ParsedValueWidget final : public AddressValueWidget
{
public:
  ParsedValueWidget(ossia::value fallback, QWidget* parent)
      : AddressValueWidget{parent}
      , m_fallback{std::move(fallback)}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    connect(&m_edit, &QLineEdit::textEdited, this, [this] { markEdited(); });
  }

  ossia::value getImpl() const override
  {
    if(auto val = readAs(m_edit.text(), m_fallback.get_type()))
      return *val;
    return m_fallback;
  }

  void setImpl(ossia::value t) override
  {
    m_fallback = t;
    m_edit.setText(State::convert::toPrettyString(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QLineEdit m_edit;
  ossia::value m_fallback;
};

template <std::size_t N>
class VecValueWidget final : public AddressValueWidget
{
public:
  VecValueWidget(const std::array<range, N>& ranges, QWidget* parent)
      : AddressValueWidget{parent}
  {
    for(std::size_t i = 0; i < N; i++)
    {
      auto* box = new QDoubleSpinBox{this};
      box->setRange(ranges[i].first, ranges[i].second);
      box->setDecimals(6);
      box->setContentsMargins(0, 0, 0, 0);
      connect(
          box, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
          [this] { markEdited(); });
      m_lay.addWidget(box);
      m_boxes[i] = box;
    }
    this->setFocusProxy(m_boxes[0]);
  }

  ossia::value getImpl() const override
  {
    std::array<float, N> v{};
    for(std::size_t i = 0; i < N; i++)
      v[i] = m_boxes[i]->value();
    return v;
  }

  void setImpl(ossia::value t) override
  {
    const auto v = State::convert::value<std::array<float, N>>(t);
    for(std::size_t i = 0; i < N; i++)
    {
      widen(*m_boxes[i], v[i]);
      m_boxes[i]->setValue(v[i]);
    }
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  std::array<QDoubleSpinBox*, N> m_boxes{};
};


class ImpulseValueWidget final : public AddressValueWidget
{
public:
  explicit ImpulseValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_button.setText(QObject::tr("Bang"));
    m_button.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_button);
    m_lay.addWidget(&m_button);

    connect(&m_button, &QPushButton::clicked, this, [this] {
      markEdited();
      changed(ossia::value{ossia::impulse{}});
    });
  }

  ossia::value getImpl() const override { return ossia::impulse{}; }
  void setImpl(ossia::value) override { }
  bool commitsImmediately() const noexcept override { return true; }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QPushButton m_button;
};


//! A swatch opening the platform picker; the value keeps the parameter's own
//! number of components, always in [0; 1].
class ColorValueWidget final : public AddressValueWidget
{
public:
  ColorValueWidget(int components, QWidget* parent)
      : AddressValueWidget{parent}
      , m_components{components}
  {
    m_button.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_button);
    m_lay.addWidget(&m_button);

    connect(&m_button, &QPushButton::clicked, this, [this] {
      QColorDialog dial{m_color, this};
      if(m_components == 4)
        dial.setOption(QColorDialog::ShowAlphaChannel);
      if(dial.exec() == QDialog::Accepted)
      {
        m_color = dial.currentColor();
        updateSwatch();
        markEdited();
        changed(get());
      }
    });

    updateSwatch();
  }

  ossia::value getImpl() const override
  {
    if(m_components == 3)
      return ossia::vec3f{
          {(float)m_color.redF(), (float)m_color.greenF(), (float)m_color.blueF()}};
    return ossia::vec4f{
        {(float)m_color.redF(), (float)m_color.greenF(), (float)m_color.blueF(),
         (float)m_color.alphaF()}};
  }

  void setImpl(ossia::value t) override
  {
    const auto v = State::convert::value<std::array<float, 4>>(t);
    m_color = QColor::fromRgbF(
        std::clamp(v[0], 0.f, 1.f), std::clamp(v[1], 0.f, 1.f),
        std::clamp(v[2], 0.f, 1.f),
        m_components == 4 ? std::clamp(v[3], 0.f, 1.f) : 1.f);
    updateSwatch();
  }

private:
  void updateSwatch()
  {
    m_button.setText(m_color.name(
        m_components == 4 ? QColor::HexArgb : QColor::HexRgb));

    const qreal dpr = m_button.devicePixelRatioF();
    const int side = m_button.fontMetrics().height();
    const QRect area{0, 0, side, side};

    QPixmap px{QSize{side, side} * dpr};
    px.setDevicePixelRatio(dpr);
    px.fill(Qt::transparent);
    {
      QPainter p{&px};
      p.fillRect(area, Qt::white);
      p.fillRect(area, m_color);
      p.setPen(m_button.palette().color(QPalette::WindowText));
      p.drawRect(area.adjusted(0, 0, -1, -1));
    }

    m_button.setIcon(QIcon{px});
    m_button.setIconSize(QSize{side, side});
  }

  score::MarginLess<QHBoxLayout> m_lay{this};
  QPushButton m_button;
  QColor m_color{Qt::black};
  int m_components{4};
};

//! A square to drag a point in.
class XYPad final : public QWidget
{
public:
  explicit XYPad(QWidget* parent)
      : QWidget{parent}
  {
    setMinimumSize(48, 48);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }

  std::array<float, 2> value() const noexcept { return m_value; }
  void setValue(std::array<float, 2> v)
  {
    m_value = {std::clamp(v[0], -1.f, 1.f), std::clamp(v[1], -1.f, 1.f)};
    update();
  }

  std::function<void()> onMoved;

private:
  void mousePressEvent(QMouseEvent* e) override { moveTo(e->position()); }
  void mouseMoveEvent(QMouseEvent* e) override { moveTo(e->position()); }
  void mouseReleaseEvent(QMouseEvent* e) override { moveTo(e->position()); }

  void moveTo(QPointF p)
  {
    const auto r = rect();
    if(r.width() <= 1 || r.height() <= 1)
      return;

    // Y grows upwards; a cartesian 2-D position runs over [-1; 1].
    setValue(
        {float(2. * std::clamp(p.x() / r.width(), 0., 1.) - 1.),
         float(1. - 2. * std::clamp(p.y() / r.height(), 0., 1.))});
    if(onMoved)
      onMoved();
  }

  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    const auto r = rect().adjusted(0, 0, -1, -1);
    p.setPen(palette().mid().color());
    p.setBrush(palette().base());
    p.drawRect(r);

    const QPointF pos{
        r.x() + (m_value[0] + 1.f) / 2.f * r.width(),
        r.y() + (1.f - m_value[1]) / 2.f * r.height()};
    const qreal x0 = r.x(), y0 = r.y(), w = r.width(), h = r.height();
    p.setPen(palette().highlight().color());
    p.drawLine(QPointF{x0, pos.y()}, QPointF{x0 + w, pos.y()});
    p.drawLine(QPointF{pos.x(), y0}, QPointF{pos.x(), y0 + h});
    p.setBrush(palette().highlight());
    p.drawEllipse(pos, 3., 3.);
  }

  std::array<float, 2> m_value{{0.f, 0.f}};
};

class PositionValueWidget final : public AddressValueWidget
{
public:
  PositionValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
      , m_pad{new XYPad{this}}
  {
    m_lay.addWidget(m_pad);
    this->setFocusProxy(m_pad);
    m_pad->onMoved = [this] {
      markEdited();
      changed(get());
    };
  }

  ossia::value getImpl() const override { return ossia::vec2f{m_pad->value()}; }

  void setImpl(ossia::value t) override
  {
    m_pad->setValue(State::convert::value<std::array<float, 2>>(t));
  }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  XYPad* m_pad{};
};


int colorComponents(const ossia::value& v) noexcept
{
  return v.get_type() == ossia::val_type::VEC3F ? 3 : 4;
}

AddressValueWidget* make_unit_widget(
    const Device::AddressSettingsCommon& addr, QWidget* parent, ValueEditorSize size)
{
  const auto& unit = addr.unit.get().v;
  if(auto* col = unit.target<ossia::color_u>())
  {
    if(col->target<ossia::rgb_u>() || col->target<ossia::rgba_u>())
      return new ColorValueWidget{colorComponents(addr.value), parent};
  }

  if(auto* pos = unit.target<ossia::position_u>())
  {
    if(pos->target<ossia::cartesian_2d_u>() && size == ValueEditorSize::Full
       && addr.value.get_type() == ossia::val_type::VEC2F)
      return new PositionValueWidget{parent};
  }

  return nullptr;
}

AddressValueWidget* make_typed_widget(
    const Device::AddressSettingsCommon& addr, QWidget* parent, ValueEditorSize size,
    double min, double max, bool bounded)
{
  const auto& dom = addr.domain.get();
  const int imin = bounded ? asInt(ossia::get_min(dom), 0)
                           : std::numeric_limits<int>::lowest();
  const int imax = bounded ? asInt(ossia::get_max(dom), 0)
                           : std::numeric_limits<int>::max();

  switch(addr.value.get_type())
  {
    case ossia::val_type::INT:
      if(bounded && size == ValueEditorSize::Full)
        return new SliderValueWidget{
            imin, imax, asInt(defaultValueOf(addr), imin), parent};
      return new IntSpinValueWidget{imin, imax, parent};


    case ossia::val_type::FLOAT:
      if(bounded && size == ValueEditorSize::Full)
        return new DoubleSliderValueWidget{
            min, max, asDouble(defaultValueOf(addr), min), parent};
      return new FloatSpinValueWidget{
          bounded ? min : unbounded_min, bounded ? max : unbounded_max, parent};

    case ossia::val_type::VEC2F:
      return new VecValueWidget<2>{
          vecRanges<2>(addr.domain.get(), min, max, bounded), parent};
    case ossia::val_type::VEC3F:
      return new VecValueWidget<3>{
          vecRanges<3>(addr.domain.get(), min, max, bounded), parent};
    case ossia::val_type::VEC4F:
      return new VecValueWidget<4>{
          vecRanges<4>(addr.domain.get(), min, max, bounded), parent};

    case ossia::val_type::IMPULSE:
      return new ImpulseValueWidget{parent};

    case ossia::val_type::BOOL:
      return new BoolValueWidget{parent};

    case ossia::val_type::STRING:
      return new StringValueWidget{parent};

    case ossia::val_type::LIST:
    case ossia::val_type::MAP:
      return new ParsedValueWidget{addr.value, parent};

    case ossia::val_type::NONE:
      return nullptr;
  }
  return nullptr;
}
}

bool hasValueList(const Device::AddressSettingsCommon& addr) noexcept
{
  return !listedValues(addr).empty();
}

AddressValueWidget* make_value_widget(
    const Device::AddressSettingsCommon& addr, QWidget* parent, ValueEditorSize size)
{
  if(auto* widg = make_unit_widget(addr, parent, size))
    return widg;

  const auto& dom = addr.domain.get();
  if(auto vals = listedValues(addr); !vals.empty())
    return new ComboValueWidget{std::move(vals), parent};

  const auto min = ossia::get_min(dom), max = ossia::get_max(dom);
  const bool bounded = min.valid() && max.valid();

  return make_typed_widget(
      addr, parent, size, asDouble(min, 0.), asDouble(max, 1.), bounded);
}

AddressValueWidget*
make_bound_widget(const Device::AddressSettingsCommon& addr, QWidget* parent)
{
  // A bound is of the parameter's type but is not itself bounded.
  Device::AddressSettingsCommon plain;
  plain.value = addr.value;
  return make_typed_widget(
      plain, parent, ValueEditorSize::Compact, 0., 1., /* bounded */ false);
}

AddressValueWidget*
make_values_widget(const Device::AddressSettingsCommon& addr, QWidget* parent)
{
  return new ParsedValueWidget{
      ossia::value{ossia::get_values(addr.domain.get())}, parent};
}
}
