#include "ValueEditors.hpp"

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/ExpandableTextEdit.hpp>

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

#include <score/graphics/BangPainting.hpp>

#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFrame>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVBoxLayout>

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
  bool escaped = false;
  QString cur;
  QStringList entries;
  QList<int> seps; // the separating ':' of each entry, found while scanning
  int sep = -1;
  for(QChar c : body)
  {
    if(escaped)
    {
      // A \" inside a string is not the end of it.
      escaped = false;
      cur += c;
      continue;
    }

    if(quoted && c == '\\')
      escaped = true;
    else if(c == '"')
      quoted = !quoted;
    else if(!quoted && (c == '[' || c == '{'))
      depth++;
    else if(!quoted && (c == ']' || c == '}'))
      depth--;
    else if(!quoted && c == ':' && depth == 0 && sep < 0)
      sep = cur.size();

    if(c == ',' && depth == 0 && !quoted)
    {
      entries.push_back(cur);
      seps.push_back(sep);
      cur.clear();
      sep = -1;
    }
    else
    {
      cur += c;
    }
  }
  entries.push_back(cur);
  seps.push_back(sep);

  for(int i = 0; i < entries.size(); i++)
  {
    const auto& entry = entries[i];
    if(seps[i] < 0)
      return std::nullopt;

    // The key goes through the value parser too, so that its escapes read the
    // same way as any other string's.
    const auto keyText = entry.left(seps[i]).trimmed();
    std::string key;
    if(keyText.startsWith('"'))
    {
      auto parsed = State::parseValue(keyText.toStdString());
      auto* str = parsed ? parsed->target<std::string>() : nullptr;
      if(!str)
        return std::nullopt;
      key = *str;
    }
    else
    {
      key = keyText.toStdString();
    }

    auto val = State::parseValue(entry.mid(seps[i] + 1).trimmed().toStdString());
    if(!val)
      return std::nullopt;

    map.emplace_back(std::move(key), *val);
  }
  return ossia::value{map};
}

//! Marks a field whose text names no value; the editors refuse to commit one.
//! `hint` is what it says when happy, so a field's own help is not wiped.
void setFieldValid(
    QLineEdit& e, const QPalette& ok, bool valid, const QString& hint = {})
{
  if(valid)
  {
    e.setPalette(ok);
    e.setToolTip(hint);
    return;
  }

  QPalette bad = ok;
  const QColor red{0xC0, 0x39, 0x2B};
  bad.setColor(QPalette::Text, red);
  bad.setColor(QPalette::WindowText, red);
  e.setPalette(bad);
  e.setToolTip(QObject::tr("This is not a value of the parameter's type; "
                           "it will not be applied."));
}

//! What "Edit as text" opens: one field in a frameless Qt::Popup. Clicking
//! away commits, Escape cancels, Return commits; text naming no value is
//! refused rather than silently dropped.
class TextFormPopup final : public QFrame
{
public:
  using Done = std::function<void(std::optional<ossia::value>)>;

  TextFormPopup(const AddressValueWidget& w, QWidget* anchor, Done done)
      : QFrame{anchor, Qt::Popup | Qt::FramelessWindowHint}
      , m_widget{w}
      , m_done{std::move(done)}
  {
    setFrameShape(QFrame::StyledPanel);

    auto* lay = new QHBoxLayout{this};
    lay->setContentsMargins(4, 3, 4, 3);
    lay->setSpacing(6);

    m_edit = new QLineEdit{this};
    m_edit->setText(w.toText());
    m_edit->selectAll();
    m_ok = m_edit->palette();
    lay->addWidget(m_edit, 1);

    m_status = new QLabel{this};
    m_status->setEnabled(false);
    lay->addWidget(m_status);

    connect(m_edit, &QLineEdit::textChanged, this, [this] { revalidate(); });
    connect(m_edit, &QLineEdit::returnPressed, this, [this] { close(); });

    revalidate();
    resize(360, sizeHint().height());
  }

  //! Empty when cancelled or when the text names no value.
  std::optional<ossia::value> result() const
  {
    if(m_cancelled)
      return std::nullopt;
    return m_widget.fromText(m_edit->text());
  }

  void focusEditor() { m_edit->setFocus(Qt::PopupFocusReason); }

private:
  void revalidate()
  {
    const auto v = m_widget.fromText(m_edit->text());
    setFieldValid(*m_edit, m_ok, v.has_value());
    m_status->setText(v ? State::convert::prettyType(*v) : tr("?"));
  }

  void keyPressEvent(QKeyEvent* ev) override
  {
    if(ev->key() == Qt::Key_Escape)
    {
      m_cancelled = true;
      close();
      return;
    }
    QFrame::keyPressEvent(ev);
  }

  // Clicking away hides a Qt::Popup rather than closing it, so both paths
  // report, once. A nested loop here would wedge a caller with no user present.
  void hideEvent(QHideEvent* ev) override
  {
    QFrame::hideEvent(ev);
    finish();
  }
  void closeEvent(QCloseEvent* ev) override
  {
    QFrame::closeEvent(ev);
    finish();
  }

  void finish()
  {
    if(std::exchange(m_finished, true))
      return;
    if(m_done)
      m_done(result());
    deleteLater();
  }

  const AddressValueWidget& m_widget;
  Done m_done;
  QLineEdit* m_edit{};
  QLabel* m_status{};
  QPalette m_ok;
  bool m_cancelled{};
  bool m_finished{};
};

//! Lets an editor be squeezed into a tree row.
//!
//! Two things stop it otherwise: the editor's own layout publishes a minimum
//! size taken from the tallest field in it (a spin box asks for ~26px against
//! an 18px row) and QWidget::setGeometry honours that, and a widget with no
//! background of its own leaves the painted cell showing through behind it.
/**
 * @brief Room for the text of an editor living in an 18px row.
 *
 * The stock widgets are laid out for a dialog: a spin box spends ~16px on
 * arrows and another ~6 on frame and padding, leaving almost nothing for the
 * digits once the row height is honoured. The arrows go entirely -- the wheel
 * and the keyboard still step the value.
 *
 * Geometry only, no colours or borders, so the skin still styles the fields.
 */
constexpr auto compactEditorStyle = R"_(
QAbstractSpinBox { padding: 0px 1px; margin: 0px; min-height: 0px; border: none; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
  width: 0px; height: 0px; border: none; margin: 0px;
}
QLineEdit { padding: 0px 2px; margin: 0px; min-height: 0px; border: none; }
QComboBox { padding: 0px 2px; margin: 0px; min-height: 0px; border: none; }
QComboBox::drop-down { width: 10px; border: none; }
QPushButton {
  padding: 0px 4px; margin: 0px; min-height: 0px; min-width: 0px;
  border: 1px solid palette(mid); border-radius: 2px;
}
QCheckBox { padding: 0px; margin: 0px; min-height: 0px; }
)_";

//! Lets an editor be squeezed into a tree row.
//!
//! Two things stop it otherwise: the editor's own layout publishes a minimum
//! size taken from the tallest field in it (a spin box asks for ~26px against
//! an 18px row) and QWidget::setGeometry honours that, and a widget with no
//! background of its own leaves the painted cell showing through behind it.
void makeRowSized(QWidget& w)
{
  if(auto* l = w.layout())
  {
    l->setSizeConstraint(QLayout::SetNoConstraint);
    l->setContentsMargins(0, 0, 0, 0);
    // Frameless fields sit side by side in a vec editor; this is the gap.
    l->setSpacing(3);
  }

  w.setStyleSheet(QString::fromUtf8(compactEditorStyle));

  w.setMinimumSize(0, 0);
  for(auto* child : w.findChildren<QWidget*>())
  {
    child->setMinimumSize(0, 0);
    child->setContentsMargins(0, 0, 0, 0);

    // The cell is the frame: a sunken border costs ~4px of the row's 18.
    if(auto* le = qobject_cast<QLineEdit*>(child))
      le->setFrame(false);
    else if(auto* sb = qobject_cast<QAbstractSpinBox*>(child))
      sb->setFrame(false);
  }

  // The delegate hands the editor the cell rect; it has to cover it.
  w.setAutoFillBackground(true);
  w.setBackgroundRole(QPalette::Base);
}

//! The value actions, tagged by QAction::data so the caller acts on them after
//! the menu's event loop has gone rather than inside it.
enum ValueAction
{
  NoValueAction = 0,
  EditAsText,
  CopyValue,
  PasteValue,
  ResetValue
};

//! A vec prints as a bracketed list and the parser has no rule for one, so the
//! arity and the element types are what identify it.
template <std::size_t N>
std::optional<ossia::value> readVec(const QString& text)
{
  auto parsed = State::parseValue(text.toStdString());
  if(!parsed)
    return std::nullopt;

  auto* lst = parsed->target<std::vector<ossia::value>>();
  if(!lst || lst->size() != N)
    return std::nullopt;

  std::array<float, N> v{};
  for(std::size_t i = 0; i < N; i++)
  {
    switch((*lst)[i].get_type())
    {
      case ossia::val_type::INT:
      case ossia::val_type::FLOAT:
        v[i] = ossia::convert<float>((*lst)[i]);
        break;
      default:
        return std::nullopt;
    }
  }
  return ossia::value{v};
}

//! Text read as a value of that type, or nothing when it cannot be.
std::optional<ossia::value> readAs(const QString& text, ossia::val_type type)
{
  switch(type)
  {
    case ossia::val_type::VEC2F:
      return readVec<2>(text);
    case ossia::val_type::VEC3F:
      return readVec<3>(text);
    case ossia::val_type::VEC4F:
      return readVec<4>(text);
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
      // Blocked: the slider clamps to the declared domain, and letting that
      // come back would overwrite a value the device holds outside it.
      const QSignalBlocker b{m_slider};
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
      // Blocked: see the integer editor above.
      const QSignalBlocker b{m_slider};
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

//! A box to tick, rather than a two-item combo.
class BoolValueWidget final : public AddressValueWidget
{
public:
  explicit BoolValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);
    m_lay.addStretch(1);

    connect(&m_edit, &QCheckBox::clicked, this, [this](bool v) {
      markEdited();
      changed(ossia::value{v});
    });
  }

  ossia::value getImpl() const override { return m_edit.isChecked(); }
  void setImpl(ossia::value t) override
  {
    m_edit.setChecked(ossia::convert<bool>(t));
  }

  // Toggling is the whole edit.
  bool commitsImmediately() const noexcept override { return true; }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QCheckBox m_edit;
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
    connect(
        &m_edit, &State::ExpandableTextEdit::fullTextEdited, this,
        [this](const QString& t) {
      markEdited();
      changed(ossia::value{t.toStdString()});
        });
  }

  // Bytes, not text: ossia's STRING is a std::string and need not be UTF-8.
  // Decoding it to display it would commit replacement characters.
  ossia::value getImpl() const override { return m_edit.fullBytes().toStdString(); }
  void setImpl(ossia::value t) override
  {
    if(auto* s = t.target<std::string>())
      m_edit.setFullBytes(QByteArray::fromStdString(*s));
    else
      m_edit.setFullText(State::convert::value<QString>(t));
  }

  bool isTextual() const noexcept override { return true; }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  State::ExpandableTextEdit m_edit;
};


//! Anything whose textual form round-trips through the value parser.
class ParsedValueWidget final : public AddressValueWidget
{
public:
  ParsedValueWidget(ossia::value fallback, QWidget* parent)
      : AddressValueWidget{parent}
      , m_type{fallback.get_type()}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);
    m_okPalette = m_edit.palette();

    connect(&m_edit, &QLineEdit::textEdited, this, [this] { markEdited(); });

    // textChanged, not textEdited: the mark follows the text however it got there.
    connect(&m_edit, &QLineEdit::textChanged, this, [this] { revalidate(); });

    connect(&m_edit, &State::ExpandableTextEdit::fullTextEdited, this, [this] {
      markEdited();
      if(auto v = readAs(m_edit.fullText(), m_type))
        changed(*v);
    });
  }

  // Invalid on purpose when the text names no value of this type; the callers
  // read that as "write nothing back".
  ossia::value getImpl() const override
  {
    if(auto val = readAs(m_edit.fullText(), m_type))
      return *val;
    return {};
  }

  void setImpl(ossia::value t) override
  {
    if(t.valid())
      m_type = t.get_type();
    m_edit.setFullText(State::convert::toPrettyString(t));
    revalidate();
  }

  bool isTextual() const noexcept override { return true; }

private:
  void revalidate()
  {
    setFieldValid(m_edit, m_okPalette, readAs(m_edit.fullText(), m_type).has_value());
  }

  score::MarginLess<QHBoxLayout> m_lay{this};
  State::ExpandableTextEdit m_edit;
  QPalette m_okPalette;
  ossia::val_type m_type{};
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


//! The node view's impulse port as a widget: score::QGraphicsButton's
//! proportions and skin colours.
class BangButton final : public QAbstractButton
{
public:
  explicit BangButton(QWidget* parent = nullptr)
      : QAbstractButton{parent}
  {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
  }

  QSize sizeHint() const override { return {18, 18}; }
  QSize minimumSizeHint() const override { return {10, 10}; }

private:
  void paintEvent(QPaintEvent*) override
  {
    const qreal side = std::max(8, std::min(height() - 4, 14));
    const QRectF circle{2., (height() - side) / 2., side, side};

    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(Qt::NoPen);
    p.setBrush(score::bangFill(palette(), isDown()));
    p.drawEllipse(circle);

    if(isDown())
    {
      const qreal inset = circle.width() * 0.125;
      p.setPen(QPen{score::bangFill(palette(), false).color(), 1.5});
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(circle.adjusted(inset, inset, -inset, -inset));
    }
  }
};

class ImpulseValueWidget final : public AddressValueWidget
{
public:
  explicit ImpulseValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_button.setToolTip(QObject::tr("Send an impulse"));
    m_button.setContentsMargins(0, 0, 0, 0);
    this->setFocusProxy(&m_button);
    m_lay.addWidget(&m_button);
    m_lay.addStretch(1);

    connect(&m_button, &QAbstractButton::clicked, this, [this] {
      markEdited();
      changed(ossia::value{ossia::impulse{}});
    });
  }

  ossia::value getImpl() const override { return ossia::impulse{}; }
  void setImpl(ossia::value) override { }
  bool commitsImmediately() const noexcept override { return true; }

  // An impulse carries no value: nothing to copy or paste.
  bool hasTextForm() const noexcept override { return false; }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  BangButton m_button;
};


//! A swatch opening the platform picker, plus a hex field. The value keeps the
//! parameter's component count, always in [0; 1].
class ColorValueWidget final : public AddressValueWidget
{
public:
  ColorValueWidget(int components, QWidget* parent)
      : AddressValueWidget{parent}
      , m_components{components}
  {
    m_button.setContentsMargins(0, 0, 0, 0);
    m_button.setFlat(true);
    m_button.setToolTip(tr("Pick a colour"));
    m_edit.setContentsMargins(0, 0, 0, 0);
    m_hint = tr("#rrggbb, #aarrggbb, a colour name, or [r, g, b, a] in 0-1");
    m_edit.setToolTip(m_hint);
    m_okPalette = m_edit.palette();

    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_button);
    m_lay.addWidget(&m_edit);

    connect(&m_button, &QPushButton::clicked, this, [this] {
      // Parented to this so the delegate does not close the editor on the
      // focus-out; guarded because the editor can still go away under it.
      QPointer<QColorDialog> dial = new QColorDialog{m_color, this};
      if(m_components == 4)
        dial->setOption(QColorDialog::ShowAlphaChannel);

      QPointer self{this};
      const int res = dial->exec();
      if(!self || !dial)
        return;

      const QColor picked = dial->currentColor();
      delete dial;
      if(res != QDialog::Accepted)
        return;

      m_color = picked;
      refresh();
      markEdited();
      changed(get());
    });

    connect(&m_edit, &QLineEdit::textChanged, this, [this](const QString& t) {
      const auto c = parseColorText(t);
      m_valid = c.has_value();
      setFieldValid(m_edit, m_okPalette, m_valid, m_hint);
      if(!c)
        return;

      m_color = *c;
      updateSwatch();
    });
    connect(&m_edit, &QLineEdit::textEdited, this, [this] { markEdited(); });

    refresh();
  }

  // Invalid while the field says so, rather than the last colour that parsed.
  ossia::value getImpl() const override
  {
    if(!m_valid)
      return {};

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
    refresh();
  }

  //! Hex rather than the vector form: pickers and stylesheets read it back.
  QString toText() const override { return hexText(); }

  std::optional<ossia::value> fromText(const QString& text) const override
  {
    auto c = parseColorText(text);
    if(!c)
      return std::nullopt;

    if(m_components == 3)
      return ossia::value{ossia::vec3f{
          {(float)c->redF(), (float)c->greenF(), (float)c->blueF()}}};
    return ossia::value{ossia::vec4f{
        {(float)c->redF(), (float)c->greenF(), (float)c->blueF(),
         (float)c->alphaF()}}};
  }

private:
  QString hexText() const
  {
    return m_color.name(m_components == 4 ? QColor::HexArgb : QColor::HexRgb);
  }

  //! Both forms the field takes: a picker's hex, and what the parameter holds.
  std::optional<QColor> parseColorText(const QString& text) const
  {
    const auto t = text.trimmed();
    if(t.isEmpty())
      return std::nullopt;

    if(t.startsWith('['))
    {
      auto parsed = State::parseValue(t.toStdString());
      if(!parsed)
        return std::nullopt;

      const auto v = State::convert::value<std::array<float, 4>>(*parsed);
      const auto n = State::convert::value<std::vector<ossia::value>>(*parsed).size();
      if(n < 3)
        return std::nullopt;

      return QColor::fromRgbF(
          std::clamp(v[0], 0.f, 1.f), std::clamp(v[1], 0.f, 1.f),
          std::clamp(v[2], 0.f, 1.f),
          (m_components == 4 && n >= 4) ? std::clamp(v[3], 0.f, 1.f) : 1.f);
    }

    const QColor c = QColor::fromString(t);
    if(!c.isValid())
      return std::nullopt;
    return c;
  }

  void refresh()
  {
    m_edit.setText(hexText());
    m_valid = true;
    setFieldValid(m_edit, m_okPalette, true, m_hint);
    updateSwatch();
  }

  void updateSwatch()
  {
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
    m_button.setFixedWidth(side + 12);
  }

  score::MarginLess<QHBoxLayout> m_lay{this};
  QPushButton m_button;
  QLineEdit m_edit;
  QPalette m_okPalette;
  QString m_hint;
  QColor m_color{Qt::black};
  bool m_valid{true};
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
  // Left button only: the right one opens the value menu, and must not move
  // the point first.
  void mousePressEvent(QMouseEvent* e) override
  {
    if(e->button() == Qt::LeftButton)
      moveTo(e->position());
    else
      e->ignore();
  }
  void mouseMoveEvent(QMouseEvent* e) override
  {
    if(e->buttons() & Qt::LeftButton)
      moveTo(e->position());
  }
  void mouseReleaseEvent(QMouseEvent* e) override
  {
    if(e->button() == Qt::LeftButton)
      moveTo(e->position());
  }

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

//! The pad and the two numbers it stands for, on one value: a pad alone cannot
//! be typed into or read off precisely.
class PositionValueWidget final : public AddressValueWidget
{
public:
  PositionValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
      , m_pad{new XYPad{this}}
  {
    m_lay.addWidget(m_pad);

    auto* fields = new score::MarginLess<QVBoxLayout>;
    for(int i = 0; i < 2; i++)
    {
      auto* box = new QDoubleSpinBox{this};
      box->setRange(-1., 1.);
      box->setSingleStep(0.01);
      box->setDecimals(6);
      box->setPrefix(i == 0 ? tr("x ") : tr("y "));
      box->setContentsMargins(0, 0, 0, 0);
      box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
      connect(
          box, SignalUtils::QDoubleSpinBox_valueChanged_double(), this,
          [this] { fromBoxes(); });
      fields->addWidget(box);
      m_boxes[i] = box;
    }
    fields->addStretch(1);
    m_lay.addLayout(fields);

    this->setFocusProxy(m_boxes[0]);
    m_pad->onMoved = [this] { fromPad(); };
  }

  // The boxes are authoritative; the pad only has pixel resolution.
  ossia::value getImpl() const override
  {
    return ossia::vec2f{{(float)m_boxes[0]->value(), (float)m_boxes[1]->value()}};
  }

  void setImpl(ossia::value t) override
  {
    const auto v = State::convert::value<std::array<float, 2>>(t);
    const QSignalBlocker b0{m_boxes[0]}, b1{m_boxes[1]};

    // The pad's square is [-1; 1] but the value need not be: widen rather than
    // clamp, so showing a value never changes it.
    for(std::size_t i = 0; i < 2; i++)
    {
      widen(*m_boxes[i], v[i]);
      m_boxes[i]->setValue(v[i]);
    }
    m_pad->setValue(v);
  }

private:
  void fromPad()
  {
    const auto v = m_pad->value();
    const QSignalBlocker b0{m_boxes[0]}, b1{m_boxes[1]};
    m_boxes[0]->setValue(v[0]);
    m_boxes[1]->setValue(v[1]);
    markEdited();
    changed(get());
  }

  void fromBoxes()
  {
    m_pad->setValue({(float)m_boxes[0]->value(), (float)m_boxes[1]->value()});
    markEdited();
  }

  score::MarginLess<QHBoxLayout> m_lay{this};
  XYPad* m_pad{};
  std::array<QDoubleSpinBox*, 2> m_boxes{};
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

QString AddressValueWidget::toText() const
{
  const auto v = get();

  // A string is written bare: readAs takes a string's text verbatim, so the
  // quoted literal form would round-trip the quotes into the value.
  if(v.get_type() == ossia::val_type::STRING)
    return State::convert::value<QString>(v);

  return State::convert::toPrettyString(v);
}

std::optional<ossia::value> AddressValueWidget::fromText(const QString& text) const
{
  const auto cur = get();
  if(!cur.valid())
    return std::nullopt;
  return readAs(text, cur.get_type());
}

void AddressValueWidget::applyExternal(const ossia::value& v)
{
  if(!v.valid())
    return;

  set(v);
  markEdited();
  changed(v);
}

void AddressValueWidget::resetToDefault()
{
  applyExternal(m_default);
}

void AddressValueWidget::installValueMenu()
{
  // The fields, not the editor, are what the pointer is over on a right-click
  // and what holds the keyboard focus.
  for(auto* w : findChildren<QWidget*>())
    w->installEventFilter(this);
  installEventFilter(this);
}

bool AddressValueWidget::eventFilter(QObject* obj, QEvent* ev)
{
  // Return means "send this", not "send this if it changed": a toggle has to
  // be repeatable without a trip through the other value.
  if(ev->type() == QEvent::KeyPress)
  {
    const auto key = static_cast<QKeyEvent*>(ev)->key();
    if(key == Qt::Key_Return || key == Qt::Key_Enter)
      markEdited();
  }

  // Qt commits and closes a cell editor on focus-out, from a filter on the
  // editor widget -- but the focus lives on a field *inside* this one, so that
  // filter never sees it go. Children only: a FocusOut on the editor itself is
  // Qt's own, and the one synthesized below.
  if(ev->type() == QEvent::FocusOut && obj != this)
  {
    // Deferred: focus may simply be moving to another field of this same
    // editor, and the new focus widget is not known yet.
    QPointer self{this};
    QTimer::singleShot(0, this, [self] {
      if(!self)
        return;

      // A menu or a dialog of ours took the focus; it will give it back.
      if(QApplication::activePopupWidget() || QApplication::activeModalWidget())
        return;

      auto* f = QApplication::focusWidget();
      for(auto* w = f; w; w = w->parentWidget())
        if(w == self)
          return;

      // Hand Qt the event rather than committing here: its filter is on this
      // widget for exactly as long as the view owns it, so it cannot commit an
      // editor that is already gone. A commit of our own raced Qt's queued one
      // and lost the value ("an editor that does not belong to this view").
      QFocusEvent out{QEvent::FocusOut, Qt::OtherFocusReason};
      QCoreApplication::sendEvent(self, &out);

      self->editingFinished();
    });
  }

  if(ev->type() != QEvent::ContextMenu)
    return QWidget::eventFilter(obj, ev);

  auto* w = qobject_cast<QWidget*>(obj);
  if(!w)
    return QWidget::eventFilter(obj, ev);

  // A text field keeps its own cut / copy / paste; the value actions go under
  // them. Everything else -- a spin box's arrows, a swatch, a pad -- gets ours
  // alone. Note that a spin box's editable area *is* a QLineEdit child, so
  // this is the branch a right-click over the digits lands in.
  QMenu* menu = nullptr;
  if(auto* le = qobject_cast<QLineEdit*>(w); le && !le->isReadOnly())
    menu = le->createStandardContextMenu();
  else
    menu = new QMenu{this};

  ev->accept();
  popValueMenu(menu, static_cast<QContextMenuEvent*>(ev)->globalPos());
  return true;
}

void AddressValueWidget::contextMenuEvent(QContextMenuEvent* ev)
{
  ev->accept();
  popValueMenu(new QMenu{this}, ev->globalPos());
}

void AddressValueWidget::popValueMenu(QMenu* menu, QPoint globalPos)
{
  QPointer<QMenu> guard{menu};
  const bool text = hasTextForm();

  if(!text && !m_default.valid())
  {
    if(menu->isEmpty())
    {
      delete menu;
      return;
    }
  }
  else if(!menu->isEmpty())
  {
    menu->addSeparator();
  }

  auto add = [menu](const QString& label, ValueAction id) {
    auto* a = menu->addAction(label);
    a->setData(int(id));
    return a;
  };

  std::optional<ossia::value> pasted;
  if(text)
  {
    if(!isTextual())
      add(tr("Edit as text…"), EditAsText);

    add(tr("Copy value"), CopyValue);

    auto* paste = add(tr("Paste value"), PasteValue);
    if(const auto clip = QGuiApplication::clipboard()->text(); !clip.isEmpty())
      pasted = fromText(clip);
    paste->setEnabled(pasted.has_value());
    if(!pasted)
    {
      // A QMenu shows action tooltips only when asked to.
      menu->setToolTipsVisible(true);
      paste->setToolTip(tr("The clipboard does not hold a value of this type"));
    }
  }

  if(m_default.valid())
  {
    menu->addSeparator();
    add(tr("Reset to default"), ResetValue);
  }

  // The chosen action is acted on after the menu is gone: "Edit as text" opens
  // a modal dialog, and doing that from inside the menu's own loop is asking
  // for the menu to be destroyed under it.
  QPointer self{this};
  QAction* chosen = menu->exec(globalPos);

  // The editor can be taken down while the menu is up; the menu goes with it,
  // and so does the action it returned.
  if(!self || !guard)
    return;

  const int act = chosen ? chosen->data().toInt() : int(NoValueAction);
  delete guard.data();

  switch(act)
  {
    case EditAsText: {
      // Parented to this: QStyledItemDelegate closes an editor that loses
      // focus, unless the new focus widget has the editor above it.
      auto* pop = new TextFormPopup{
          *this, this, [self](std::optional<ossia::value> v) {
        if(self && v)
          self->applyExternal(*v);
          }};
      pop->move(mapToGlobal(QPoint{0, height()}));
      pop->show();
      pop->focusEditor();
      break;
    }
    case CopyValue:
      QGuiApplication::clipboard()->setText(toText());
      break;
    case PasteValue:
      if(pasted)
        applyExternal(*pasted);
      break;
    case ResetValue:
      resetToDefault();
      break;
    default:
      break;
  }
}

bool hasValueList(const Device::AddressSettingsCommon& addr) noexcept
{
  return !listedValues(addr).empty();
}

bool paintValueWithMarker(
    QPainter& painter, const QStyleOptionViewItem& option, const QString& text)
{
  const auto split = State::convert::splitSingleLine(text);
  if(split.marker.isEmpty())
    return false;

  auto* style = option.widget ? option.widget->style() : QApplication::style();

  QStyleOptionViewItem opt = option;
  opt.text.clear();
  style->drawControl(QStyle::CE_ItemViewItem, &opt, &painter, option.widget);

  const auto area = style->subElementRect(
      QStyle::SE_ItemViewItemText, &option, option.widget);

  painter.save();
  painter.setPen(option.palette.color(
      option.state & QStyle::State_Selected ? QPalette::HighlightedText
                                            : QPalette::Text));

  const QFontMetrics fm{option.font};
  const int headW = fm.horizontalAdvance(split.head);

  painter.setFont(option.font);
  painter.drawText(area, Qt::AlignVCenter, split.head);

  // The marker is not part of the value: say so with the pen, not with a
  // symbol in the text.
  QFont mf = option.font;
  mf.setItalic(true);
  painter.setFont(mf);

  auto dim = painter.pen().color();
  dim.setAlphaF(0.6);
  painter.setPen(dim);
  painter.drawText(
      area.adjusted(headW + fm.horizontalAdvance(QStringLiteral("  ")), 0, 0, 0),
      Qt::AlignVCenter, split.marker);

  painter.restore();
  return true;
}

void fitEditorToCell(QWidget& editor, const QRect& cell)
{
  // Qt's own editor for a plain string -- the Name column, an extended
  // attribute -- never passed through make_value_widget, so it still carries
  // the frame and padding of a dialog field and the type gets shrunk to pay
  // for them. Same treatment: the cell is the frame.
  if(auto* le = qobject_cast<QLineEdit*>(&editor); le && le->hasFrame())
  {
    le->setFrame(false);
    le->setContentsMargins(0, 0, 0, 0);
    le->setStyleSheet(
        QStringLiteral("QLineEdit { padding: 0px 2px; margin: 0px; "
                       "min-height: 0px; border: none; }"));
  }

  const int target = cell.height();
  if(target > 0)
  {
    QFont f = editor.font();

    // Six steps is enough to get from a default UI font to the floor; past it
    // the field is squeezed rather than made unreadable.
    for(int i = 0; i < 6; i++)
    {
      editor.updateGeometry();
      if(auto* l = editor.layout())
        l->invalidate();
      if(editor.sizeHint().height() <= target)
        break;

      if(f.pointSizeF() > 0.)
      {
        if(f.pointSizeF() <= 5.5)
          break;
        f.setPointSizeF(f.pointSizeF() - 0.5);
      }
      else
      {
        if(f.pixelSize() <= 7)
          break;
        f.setPixelSize(f.pixelSize() - 1);
      }

      editor.setFont(f);
      for(auto* child : editor.findChildren<QWidget*>())
        child->setFont(f);
    }
  }

  editor.setGeometry(cell);
}

AddressValueWidget* make_value_widget(
    const Device::AddressSettingsCommon& addr, QWidget* parent, ValueEditorSize size)
{
  auto* widg = make_unit_widget(addr, parent, size);

  if(!widg)
  {
    if(auto vals = listedValues(addr); !vals.empty())
    {
      widg = new ComboValueWidget{std::move(vals), parent};
    }
    else
    {
      const auto& dom = addr.domain.get();
      const auto min = ossia::get_min(dom), max = ossia::get_max(dom);
      const bool bounded = min.valid() && max.valid();

      widg = make_typed_widget(
          addr, parent, size, asDouble(min, 0.), asDouble(max, 1.), bounded);
    }
  }

  if(widg)
  {
    widg->setDefaultValue(defaultValueOf(addr));
    widg->installValueMenu();
    if(size == ValueEditorSize::Compact)
      makeRowSized(*widg);
  }
  return widg;
}

AddressValueWidget*
make_bound_widget(const Device::AddressSettingsCommon& addr, QWidget* parent)
{
  // A bound is of the parameter's type but is not itself bounded.
  Device::AddressSettingsCommon plain;
  plain.value = addr.value;
  auto* widg = make_typed_widget(
      plain, parent, ValueEditorSize::Compact, 0., 1., /* bounded */ false);
  if(widg)
    widg->installValueMenu();
  return widg;
}

AddressValueWidget*
make_values_widget(const Device::AddressSettingsCommon& addr, QWidget* parent)
{
  auto* widg = new ParsedValueWidget{
      ossia::value{ossia::get_values(addr.domain.get())}, parent};
  widg->installValueMenu();
  return widg;
}
}
