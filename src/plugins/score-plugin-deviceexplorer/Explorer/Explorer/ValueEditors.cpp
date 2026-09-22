#include "ValueEditors.hpp"

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/ExpandableTextEdit.hpp>
#include <State/Widgets/Values/TypeComboBox.hpp>

#include <score/model/Skin.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/IntSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/common/extended_types.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <score/graphics/BangPainting.hpp>

#include <QAbstractButton>
#include <QAbstractTableModel>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QtWidgets/qtwidgetsglobal.h>
#if defined(QT_FEATURE_fontcombobox) && QT_CONFIG(fontcombobox)
#include <QFontComboBox>
#endif
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
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
#include <QScreen>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
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

/**
 * @brief Room for the text of an editor living in an 18px row.
 *
 * The stock widgets are laid out for a dialog: a spin box spends ~16px on
 * arrows and another ~6 on frame and padding, leaving almost nothing for the
 * digits once the row height is honoured. The arrows go entirely -- the wheel
 * and the keyboard still step the value.
 *
 * Widget API and not a style sheet: the deployment builds of Qt are configured
 * with QT_NO_STYLE_STYLESHEET, where setStyleSheet is not declared at all.
 */
void compactField(QWidget& w)
{
  w.setMinimumSize(0, 0);
  w.setContentsMargins(0, 0, 0, 0);

  // The cell is the frame: a sunken border costs ~4px of the row's 18.
  if(auto* le = qobject_cast<QLineEdit*>(&w))
  {
    le->setFrame(false);
    le->setTextMargins(2, 0, 2, 0);
  }
  else if(auto* sb = qobject_cast<QAbstractSpinBox*>(&w))
  {
    sb->setFrame(false);
    sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
  }
  else if(auto* cb = qobject_cast<QComboBox*>(&w))
  {
    cb->setFrame(false);
  }
}

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

  compactField(w);
  for(auto* child : w.findChildren<QWidget*>())
    compactField(*child);

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
      return State::convert::parseMap(text);
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

/**
 * @brief A string the device says is a path, with the file dialog on it.
 *
 * The parameter carries the same std::string either way; EXTENDED_TYPE is the
 * device saying what the string means, and a path typed by hand into a line
 * edit is the one thing a file dialog exists to spare the user.
 */
class FilePathValueWidget final : public AddressValueWidget
{
public:
  explicit FilePathValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    m_edit.setPlaceholderText(tr("Path to a file"));
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    m_browse.setIcon(QIcon(":/icons/search.png"));
    m_browse.setToolTip(tr("Browse..."));
    m_edit.addAction(&m_browse, QLineEdit::TrailingPosition);

    connect(&m_edit, &QLineEdit::textEdited, this, [this] { markEdited(); });
    connect(&m_browse, &QAction::triggered, this, [this] {
      // Parented to this, so the delegate does not close the editor on the
      // focus-out the dialog causes; guarded, because it can still go away.
      QPointer self{this};
      const QString picked = QFileDialog::getOpenFileName(
          this, tr("Choose a file"), QFileInfo{m_edit.text()}.absolutePath());
      if(!self || picked.isEmpty())
        return;

      m_edit.setText(picked);
      markEdited();
      changed(get());
    });
  }

  ossia::value getImpl() const override { return m_edit.text().toStdString(); }
  void setImpl(ossia::value t) override
  {
    m_edit.setText(State::convert::value<QString>(t));
  }

  bool isTextual() const noexcept override { return true; }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  QLineEdit m_edit;
  QAction m_browse{this};
};

//! A string the device says is an URL: the same field, and a way to follow it.
class UrlValueWidget final : public AddressValueWidget
{
public:
  explicit UrlValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    m_edit.setPlaceholderText(tr("http://..."));
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    m_open.setIcon(QIcon(":/icons/load_on.png"));
    m_open.setToolTip(tr("Open in the browser"));
    m_edit.addAction(&m_open, QLineEdit::TrailingPosition);

    connect(&m_edit, &QLineEdit::textEdited, this, [this] {
      markEdited();
      refreshOpenAction();
    });
    connect(&m_open, &QAction::triggered, this, [this] {
      const QUrl u{m_edit.text(), QUrl::StrictMode};
      if(u.isValid() && !u.scheme().isEmpty())
        QDesktopServices::openUrl(u);
    });
    refreshOpenAction();
  }

  ossia::value getImpl() const override { return m_edit.text().toStdString(); }
  void setImpl(ossia::value t) override
  {
    m_edit.setText(State::convert::value<QString>(t));
    refreshOpenAction();
  }

  bool isTextual() const noexcept override { return true; }

private:
  //! An address with no scheme is not something to hand the desktop.
  void refreshOpenAction()
  {
    const QUrl u{m_edit.text(), QUrl::StrictMode};
    m_open.setEnabled(u.isValid() && !u.scheme().isEmpty());
  }

  score::MarginLess<QHBoxLayout> m_lay{this};
  QLineEdit m_edit;
  QAction m_open{this};
};

// QFontComboBox draws each entry in its own face, and is behind a Qt feature
// that some builds turn off -- the same family of switch as the
// QT_NO_STYLE_STYLESHEET this file already works around. Where it is missing,
// a plain combo box off QFontDatabase is the same list without the preview.
#if defined(QT_FEATURE_fontcombobox) && QT_CONFIG(fontcombobox)
using FontChooserBox = QFontComboBox;
void fillFontNames(FontChooserBox&) { }
#else
using FontChooserBox = QComboBox;
void fillFontNames(FontChooserBox& box)
{
  box.addItems(QFontDatabase::families());
}
#endif

/**
 * @brief A string the device says names a font: the fonts this machine has.
 *
 * Still editable, since the name is the device's and need not be a font
 * installed here.
 */
class FontValueWidget final : public AddressValueWidget
{
public:
  explicit FontValueWidget(QWidget* parent)
      : AddressValueWidget{parent}
  {
    m_edit.setContentsMargins(0, 0, 0, 0);
    m_edit.setEditable(true);
    m_edit.setInsertPolicy(QComboBox::NoInsert);
    fillFontNames(m_edit);
    this->setFocusProxy(&m_edit);
    m_lay.addWidget(&m_edit);

    connect(&m_edit, &QComboBox::currentTextChanged, this, [this](const QString& t) {
      markEdited();
      changed(ossia::value{t.toStdString()});
    });
  }

  ossia::value getImpl() const override { return m_edit.currentText().toStdString(); }
  void setImpl(ossia::value t) override
  {
    m_edit.setCurrentText(State::convert::value<QString>(t));
  }

  bool isTextual() const noexcept override { return true; }

private:
  score::MarginLess<QHBoxLayout> m_lay{this};
  FontChooserBox m_edit;
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

// ---------------------------------------------------------------------------
// Lists and maps.
// ---------------------------------------------------------------------------

//! How many elements a summary spells out before it says how many are left.
constexpr std::size_t summary_elements = 8;

//! How much of one element a summary or a table cell carries.
constexpr qsizetype element_text_budget = 160;

QString clipText(QString s)
{
  if(s.size() > element_text_budget)
    s = s.left(element_text_budget - 1) + QChar{0x2026};
  return s;
}

QString elementText(const ossia::value& v);

//! The head of a collection and its size, never the whole of it: a ten
//! thousand element list must not be formatted to be looked at.
template <typename Seq, typename Fmt>
QString sequenceText(const Seq& seq, QChar open, QChar close, Fmt&& fmt)
{
  QString out{open};
  const std::size_t shown = std::min<std::size_t>(seq.size(), summary_elements);
  for(std::size_t i = 0; i < shown; i++)
  {
    if(i > 0)
      out += QStringLiteral(", ");
    out += fmt(seq.at(i));
  }
  if(shown < seq.size())
    out += QObject::tr(", … %1 more").arg(qulonglong(seq.size() - shown));
  return out + close;
}

QString mapElementText(const ossia::value_map_element& e)
{
  return QString::fromStdString(e.first) + QStringLiteral(": ") + elementText(e.second);
}

QString elementText(const ossia::value& v)
{
  if(auto* l = v.target<std::vector<ossia::value>>())
    return sequenceText(*l, '[', ']', elementText);
  if(auto* m = v.target<ossia::value_map_type>())
    return sequenceText(*m, '{', '}', mapElementText);
  return clipText(State::convert::toSingleLine(State::convert::toPrettyString(v)));
}

/**
 * @brief A table over a list or a map: one row per element.
 *
 * It holds the collection being edited, and is the only copy of it while the
 * dialog is open. Nothing here is per element: what a row shows comes from
 * data(), and what edits it comes from the delegate below, one cell at a time.
 */
template <typename Container>
class CollectionTableModel final : public QAbstractTableModel
{
  static constexpr bool keyed = std::is_same_v<Container, ossia::value_map_type>;
  using Row = typename Container::value_type;

public:
  CollectionTableModel(Container rows, ossia::value proto, QObject* parent)
      : QAbstractTableModel{parent}
      // Parentheses: braces would pick the initializer_list overload, and a
      // list of values is itself a value.
      , m_rows(std::move(rows))
      , m_proto{std::move(proto)}
  {
  }

  //! What the dialog reports back; the model is done with it.
  Container take() { return std::move(m_rows); }

  int rowCount(const QModelIndex& p) const override
  {
    return p.isValid() ? 0 : (int)m_rows.size();
  }

  int columnCount(const QModelIndex& p) const override
  {
    return p.isValid() ? 0 : columns;
  }

  QVariant data(const QModelIndex& idx, int role) const override
  {
    if(!idx.isValid() || idx.row() >= (int)m_rows.size())
      return {};

    const auto& row = m_rows.at(idx.row());
    if constexpr(keyed)
    {
      if(idx.column() == key_column)
      {
        if(role == Qt::DisplayRole || role == Qt::EditRole)
          return QString::fromStdString(row.first);
        return {};
      }
    }

    const ossia::value& v = valueOf(row);
    if(idx.column() == type_column)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return State::convert::prettyType(v);
        case Qt::EditRole:
          return QVariant::fromValue(v.get_type());
        default:
          return {};
      }
    }

    switch(role)
    {
      case Qt::DisplayRole:
        return elementText(v);
      case Qt::EditRole:
        return QVariant::fromValue(v);
      case Qt::ToolTipRole:
        return State::convert::prettyType(v);
      default:
        return {};
    }
  }

  bool setData(const QModelIndex& idx, const QVariant& v, int role) override
  {
    if(role != Qt::EditRole || !idx.isValid() || idx.row() >= (int)m_rows.size())
      return false;

    auto& row = m_rows.at(idx.row());
    if constexpr(keyed)
    {
      if(idx.column() == key_column)
      {
        row.first = v.toString().toStdString();
        dataChanged(idx, idx);
        return true;
      }
    }

    if(idx.column() == type_column)
    {
      const auto type = v.value<ossia::val_type>();
      auto& cur = valueOf(row);
      if(type == ossia::val_type::NONE || type == cur.get_type())
        return false;

      // What the old value can still say in the new type is kept; the rest is
      // the type's own zero.
      auto next = ossia::convert(cur, type);
      cur = next.valid() ? std::move(next) : ossia::init_value(type);
      dataChanged(index(idx.row(), type_column), index(idx.row(), value_column));
      return true;
    }

    auto val = v.value<ossia::value>();
    if(!val.valid())
      return false;

    valueOf(row) = std::move(val);
    dataChanged(idx, idx);
    return true;
  }

  Qt::ItemFlags flags(const QModelIndex& idx) const override
  {
    if(!idx.isValid())
      return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
  }

  QVariant headerData(int section, Qt::Orientation o, int role) const override
  {
    if(o == Qt::Horizontal && role == Qt::DisplayRole)
    {
      if(section == type_column)
        return tr("Type");
      if(section == value_column)
        return tr("Value");
      return tr("Key");
    }
    return QAbstractTableModel::headerData(section, o, role);
  }

  bool insertRows(int at, int count, const QModelIndex& p) override
  {
    if(p.isValid() || count <= 0 || at < 0 || at > (int)m_rows.size())
      return false;

    beginInsertRows({}, at, at + count - 1);
    m_rows.insert(m_rows.begin() + at, count, newRow());
    endInsertRows();
    return true;
  }

  bool removeRows(int at, int count, const QModelIndex& p) override
  {
    if(p.isValid() || count <= 0 || at < 0 || at + count > (int)m_rows.size())
      return false;

    beginRemoveRows({}, at, at + count - 1);
    m_rows.erase(m_rows.begin() + at, m_rows.begin() + at + count);
    endRemoveRows();
    return true;
  }

private:
  static const ossia::value& valueOf(const Row& r) noexcept
  {
    if constexpr(keyed)
      return r.second;
    else
      return r;
  }

  static ossia::value& valueOf(Row& r) noexcept
  {
    if constexpr(keyed)
      return r.second;
    else
      return r;
  }

  //! A new element is of the type the others are, so that a list of floats
  //! keeps getting spin boxes as it grows.
  Row newRow() const
  {
    auto type = ossia::val_type::FLOAT;
    if(!m_rows.empty())
      type = valueOf(m_rows.back()).get_type();
    else if(m_proto.valid())
      type = m_proto.get_type();

    auto v = type == ossia::val_type::NONE ? ossia::value{0.f} : ossia::init_value(type);
    if constexpr(keyed)
      return Row{std::string{}, std::move(v)};
    else
      return v;
  }

  static constexpr int key_column = 0;
  static constexpr int type_column = keyed ? 1 : 0;
  static constexpr int value_column = keyed ? 2 : 1;
  static constexpr int columns = value_column + 1;

  Container m_rows;
  ossia::value m_proto;
};

/**
 * @brief The editor for one element, built for the cell being edited and no
 * other.
 *
 * It goes through the same factory the parameter itself went through, so a
 * list of floats gets spin boxes, a list of booleans gets boxes to tick and a
 * nested collection gets this table again -- and because it is a delegate,
 * only the one cell under the cursor ever has a widget.
 */
class ElementDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

private:
  //! The value lives in the last column, what it is in the one before it; a
  //! map's key is a plain string ahead of both.
  static bool isValueColumn(const QModelIndex& index) noexcept
  {
    return index.model() && index.column() == index.model()->columnCount() - 1;
  }

  static bool isTypeColumn(const QModelIndex& index) noexcept
  {
    return index.model() && index.column() == index.model()->columnCount() - 2;
  }

  QWidget* createEditor(
      QWidget* parent, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override
  {
    if(isTypeColumn(index))
    {
      auto* box = new State::TypeComboBox{parent};
      box->set(index.data(Qt::EditRole).value<ossia::val_type>());
      return box;
    }

    if(!isValueColumn(index))
      return QStyledItemDelegate::createEditor(parent, option, index);

    Device::AddressSettingsCommon as;
    as.value = index.data(Qt::EditRole).value<ossia::value>();
    as.ioType = ossia::access_mode::BI;

    if(auto* w = make_value_widget(as, parent, ValueEditorSize::Compact))
      return w;

    // An element of a type nothing answers to is still editable as text.
    return new ParsedValueWidget{as.value, parent};
  }

  void setEditorData(QWidget* editor, const QModelIndex& index) const override
  {
    if(auto* box = qobject_cast<State::TypeComboBox*>(editor))
      box->set(index.data(Qt::EditRole).value<ossia::val_type>());
    else if(auto* w = qobject_cast<AddressValueWidget*>(editor))
      w->set(index.data(Qt::EditRole).value<ossia::value>());
    else
      QStyledItemDelegate::setEditorData(editor, index);
  }

  void setModelData(
      QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
  {
    if(auto* box = qobject_cast<State::TypeComboBox*>(editor))
    {
      model->setData(index, QVariant::fromValue(box->get()), Qt::EditRole);
      return;
    }

    auto* w = qobject_cast<AddressValueWidget*>(editor);
    if(!w)
    {
      QStyledItemDelegate::setModelData(editor, model, index);
      return;
    }

    // Untouched, or text naming no value of the element's type: the element
    // stays as it was rather than being replaced by whatever the field held.
    if(!w->edited())
      return;
    if(auto v = w->get(); v.valid())
      model->setData(index, QVariant::fromValue(v), Qt::EditRole);
  }

  void updateEditorGeometry(
      QWidget* editor, const QStyleOptionViewItem& option,
      const QModelIndex&) const override
  {
    fitEditorToCell(*editor, option.rect);
  }
};

//! The mark on the button that opens a collection: three dots, painted from
//! the palette rather than taken from the icon theme, which has none.
QIcon ellipsisIcon(const QPalette& pal)
{
  constexpr int side = 16;
  constexpr qreal dot = 2.6;

  QPixmap pm{side, side};
  pm.fill(Qt::transparent);

  QPainter p{&pm};
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(pal.text());
  for(int i = 0; i < 3; i++)
    p.drawEllipse(QRectF{2.4 + i * 4.6, (side - dot) / 2., dot, dot});

  return QIcon{pm};
}

//! What the dialog opens at when nothing has sized it yet, and what it keeps
//! from then on: a collection is usually looked at more than once a session.
QSize& collectionDialogSize()
{
  static QSize size{620, 460};
  return size;
}

/**
 * @brief What the "…" button opens: the elements in a table, in a dialog.
 *
 * A window and not a popup, because a collection is not a one-field edit: it
 * holds thousands of elements, the user has to be able to make the window big
 * enough to work in, and every cell editor here takes the focus -- which is
 * what folds a popup up under the hand using it.
 */
class CollectionDialog final : public QDialog
{
public:
  CollectionDialog(QAbstractItemModel* model, bool keyed, QWidget* anchor)
      : QDialog{anchor}
      , m_keyed{keyed}
  {
    model->setParent(this);
    setWindowTitle(keyed ? tr("Edit map") : tr("Edit list"));
    setSizeGripEnabled(true);

    auto* lay = new QVBoxLayout{this};

    m_view = new QTableView{this};
    m_view->setModel(model);
    m_view->setItemDelegate(new ElementDelegate{m_view});
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setWordWrap(false);
    // No SelectedClicked: a click on the selected row is how the row Remove
    // and Duplicate act on is picked, and an editor opened by it would take
    // the keys meant for the table.
    m_view->setEditTriggers(
        QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::AnyKeyPressed);

    auto* cols = m_view->horizontalHeader();
    cols->setStretchLastSection(true);
    cols->setSectionResizeMode(QHeaderView::Interactive);
    const int columns = model->columnCount({});
    m_view->setColumnWidth(columns - 2, 110);
    if(columns > 2)
      m_view->setColumnWidth(0, 140);

    // Fixed rows, and no resizeRowsToContents anywhere: a header that sizes
    // itself to its contents walks every row, which is the one thing a ten
    // thousand element list cannot afford.
    auto* rows = m_view->verticalHeader();
    rows->setSectionResizeMode(QHeaderView::Fixed);
    rows->setDefaultSectionSize(m_view->fontMetrics().height() + 8);
    lay->addWidget(m_view, 1);

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);

    auto* add = new QPushButton{tr("Add"), this};
    auto* ins = new QPushButton{tr("Insert"), this};
    auto* dup = new QPushButton{tr("Duplicate"), this};
    auto* rem = new QPushButton{tr("Remove"), this};
    add->setToolTip(keyed ? tr("Append an entry") : tr("Append an element"));
    ins->setToolTip(tr("Insert before the selected row"));
    rem->setToolTip(tr("Remove the selected rows"));
    for(auto* b : {add, ins, dup, rem})
    {
      b->setAutoDefault(false);
      bar->addWidget(b);
    }

    bar->addStretch(1);
    m_count = new QLabel{this};
    bar->addWidget(m_count);
    lay->addLayout(bar);

    auto* box = new QDialogButtonBox{
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
    lay->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // No default button: Return belongs to the cell being typed into, and a
    // dialog that closes on it takes the half-finished element with it.
    for(auto* b : box->buttons())
      qobject_cast<QPushButton*>(b)->setAutoDefault(false);

    connect(add, &QAbstractButton::clicked, this, [this] { insertAt(rowCount()); });
    connect(ins, &QAbstractButton::clicked, this, [this] {
      insertAt(std::max(0, currentRow()));
    });
    connect(dup, &QAbstractButton::clicked, this, [this] { duplicate(); });
    connect(rem, &QAbstractButton::clicked, this, [this] { removeSelection(); });

    // On the view alone: Delete in a cell editor is the editor's own.
    addViewShortcut(QKeySequence::Delete, [this] { removeSelection(); });
    addViewShortcut(QKeySequence{Qt::Key_Insert}, [this] {
      insertAt(std::max(0, currentRow()));
    });

    auto touch = [this] {
      m_touched = true;
      refreshCount();
    };
    connect(model, &QAbstractItemModel::dataChanged, this, touch);
    connect(model, &QAbstractItemModel::rowsInserted, this, touch);
    connect(model, &QAbstractItemModel::rowsRemoved, this, touch);

    refreshCount();
    resize(collectionDialogSize());
  }

  //! Whether anything was changed; a dialog the user backed out of says no.
  bool touched() const noexcept { return m_touched; }

private:
  int rowCount() const { return m_view->model()->rowCount({}); }

  int currentRow() const
  {
    const auto idx = m_view->currentIndex();
    return idx.isValid() ? idx.row() : -1;
  }

  template <typename F>
  void addViewShortcut(const QKeySequence& keys, F&& fun)
  {
    auto* act = new QAction{m_view};
    act->setShortcut(keys);
    act->setShortcutContext(Qt::WidgetShortcut);
    connect(act, &QAction::triggered, this, std::forward<F>(fun));
    m_view->addAction(act);
  }

  //! A new row, with the cursor in it and its value open for typing.
  void insertAt(int at)
  {
    auto* model = m_view->model();
    if(!model->insertRow(at))
      return;
    editRow(at);
  }

  void duplicate()
  {
    const int at = currentRow();
    auto* model = m_view->model();
    if(at < 0 || !model->insertRow(at + 1))
      return;

    // Left to right, so that the element's type is in place before the
    // element that is read as one.
    const int columns = model->columnCount({});
    for(int c = 0; c < columns; c++)
      model->setData(
          model->index(at + 1, c), model->index(at, c).data(Qt::EditRole),
          Qt::EditRole);

    editRow(at + 1);
  }

  void editRow(int at)
  {
    auto* model = m_view->model();
    const auto cell = model->index(at, model->columnCount({}) - 1);
    m_view->setCurrentIndex(cell);
    m_view->scrollTo(cell);
    m_view->edit(cell);
  }

  void removeSelection()
  {
    auto* model = m_view->model();
    auto rows = m_view->selectionModel()->selectedRows();
    if(rows.isEmpty())
    {
      if(const int at = currentRow(); at >= 0)
        model->removeRow(at);
      return;
    }

    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
      return a.row() > b.row();
    });
    for(const auto& r : rows)
      model->removeRow(r.row());
  }

  void refreshCount()
  {
    const int n = rowCount();
    m_count->setText(
        m_keyed ? tr("%n entries", nullptr, n) : tr("%n elements", nullptr, n));
  }

  void done(int result) override
  {
    // A cell left open is part of the edit: leaving the row is what makes the
    // view commit its editor.
    if(result == QDialog::Accepted && m_view->currentIndex().isValid())
      m_view->setCurrentIndex(QModelIndex{});

    collectionDialogSize() = size();
    QDialog::done(result);
  }

  QTableView* m_view{};
  QLabel* m_count{};
  bool m_keyed{};
  bool m_touched{};
};

/**
 * @brief A list or a map: a summary in the row, the elements in a table.
 *
 * These run to thousands of elements, so the row shows the head of the
 * collection and its size rather than the whole of it, and the table is a
 * model a view pulls from rather than a widget per element.
 */
template <typename Container>
class CollectionValueWidget final : public AddressValueWidget
{
  static constexpr bool keyed = std::is_same_v<Container, ossia::value_map_type>;
  using Model = CollectionTableModel<Container>;

public:
  //! `elementProto` says what type a first element takes when the collection
  //! starts out empty; the elements already there say it otherwise.
  CollectionValueWidget(ossia::value elementProto, QWidget* parent)
      : AddressValueWidget{parent}
      , m_proto{std::move(elementProto)}
  {
    m_summary.setContentsMargins(0, 0, 0, 0);
    m_summary.setReadOnly(true);
    m_summary.setPlaceholderText(keyed ? tr("Empty map") : tr("Empty list"));
    this->setFocusProxy(&m_summary);
    m_lay.addWidget(&m_summary, 1);

    // A button of its own and not a QLineEdit action: an action is drawn and
    // clicked by the field, which is a different widget in every table the
    // editor is put in.
    m_open.setObjectName(QStringLiteral("editCollection"));
    m_open.setIcon(ellipsisIcon(palette()));
    m_open.setToolTip(keyed ? tr("Edit the entries") : tr("Edit the elements"));
    m_button.setDefaultAction(&m_open);
    m_button.setIconSize(QSize{12, 12});
    m_button.setFocusPolicy(Qt::NoFocus);
    m_lay.addWidget(&m_button);

    connect(&m_open, &QAction::triggered, this, [this] { openTable(); });

    refresh();
  }

  ossia::value getImpl() const override { return m_rows; }

  void setImpl(ossia::value t) override
  {
    if(auto* c = t.target<Container>())
      m_rows = *c;
    else if(t.valid())
      m_rows = State::convert::value<Container>(t);
    else
      m_rows.clear();
    refresh();
  }

private:
  void refresh()
  {
    m_summary.setText(clipText(summary()));

    // The head of the collection is what identifies it; a field left scrolled
    // to the end shows the tail of the elision and nothing else.
    m_summary.setCursorPosition(0);
    m_summary.setToolTip(
        keyed ? tr("%n entries", nullptr, (int)m_rows.size())
              : tr("%n elements", nullptr, (int)m_rows.size()));
  }

  //! Empty, so that the placeholder says what the collection is.
  QString summary() const
  {
    if(m_rows.empty())
      return {};
    if constexpr(keyed)
      return sequenceText(m_rows, '{', '}', mapElementText);
    else
      return sequenceText(m_rows, '[', ']', elementText);
  }

  void openTable()
  {
    auto* model = new Model(m_rows, m_proto, nullptr);

    // Parented to this: QStyledItemDelegate closes an editor that loses focus,
    // unless the new focus widget has the editor above it.
    auto* dialog = new CollectionDialog{model, keyed, this};
    QPointer<Model> guard{model};
    connect(dialog, &QDialog::accepted, this, [this, guard, dialog] {
      if(!guard || !dialog->touched())
        return;

      m_rows = guard->take();
      refresh();
      markEdited();
      changed(get());
    });
    connect(dialog, &QDialog::finished, this, [this] {
      m_summary.setFocus(Qt::OtherFocusReason);
    });
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);

    // open() and not exec(): a nested event loop inside a cell editor outlives
    // the view that owns it too easily.
    dialog->open();
  }

  score::MarginLess<QHBoxLayout> m_lay{this};
  QLineEdit m_summary;
  QAction m_open{this};
  QToolButton m_button{this};
  Container m_rows;
  ossia::value m_proto;
};

using ListValueWidget = CollectionValueWidget<std::vector<ossia::value>>;
using MapValueWidget = CollectionValueWidget<ossia::value_map_type>;

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

//! A font name is not one of libossia's declared extended types; it is a name
//! score answers to, so that a device that knows its string is a font can say
//! so and get the picker.
constexpr std::string_view font_type{"font"};

/**
 * @brief The editor an EXTENDED_TYPE asks for, if any.
 *
 * A parameter's value type says how the bytes travel; its extended type says
 * what they mean. A path and an URL are both a STRING and both got a bare line
 * edit -- the declaration was carried around, serialized and shown in the
 * address panel without anything ever acting on it.
 */
AddressValueWidget*
make_extended_type_widget(const Device::AddressSettingsCommon& addr, QWidget* parent)
{
  // Only strings, so far: the array-shaped extended types say how to read an
  // array that the typed editors already cover.
  if(addr.value.get_type() != ossia::val_type::STRING)
    return nullptr;

  const auto ext = ossia::net::get_extended_type(addr.extendedAttributes);
  if(!ext)
    return nullptr;

  const std::string_view type{*ext};
  if(type == ossia::filesystem_path_type())
    return new FilePathValueWidget{parent};
  if(type == ossia::url_type())
    return new UrlValueWidget{parent};
  if(type == font_type)
    return new FontValueWidget{parent};

  return nullptr;
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
      return new ListValueWidget{{}, parent};

    case ossia::val_type::MAP:
      return new MapValueWidget{{}, parent};

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
    auto check = [self](auto&& again) -> void {
      if(!self)
        return;

      // A menu, our own value panel, a colour dialog: something took the focus
      // and will hand it back. Ask again when it has gone rather than dropping
      // the question, or the editor is left open for good.
      if(QApplication::activePopupWidget() || QApplication::activeModalWidget())
      {
        QTimer::singleShot(50, self, [self, again] {
          if(self)
            again(again);
        });
        return;
      }

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
    };

    QTimer::singleShot(0, this, [check] { check(check); });
  }

  // The editor's own right-click goes to contextMenuEvent below; only the
  // fields need intercepting, and letting both handle `this` would build the
  // menu twice.
  if(ev->type() != QEvent::ContextMenu || obj == this)
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
  QFont mf = option.font;
  mf.setItalic(true);
  const QFontMetrics mfm{mf};

  // The marker keeps its room and the value gives way, elided as the base
  // delegate would: a value cut off mid-glyph reads as a shorter value.
  const int gap = fm.horizontalAdvance(QStringLiteral("  "));
  const int markerW = mfm.horizontalAdvance(split.marker);
  const int headRoom = std::max(0, area.width() - gap - markerW);

  const auto head = fm.elidedText(split.head, option.textElideMode, headRoom);
  const int headW = std::min(fm.horizontalAdvance(head), headRoom);

  painter.setFont(option.font);
  painter.drawText(area.adjusted(0, 0, headRoom - area.width(), 0),
                   Qt::AlignVCenter, head);

  // The marker is not part of the value: say so with the pen, not with a
  // symbol in the text.
  painter.setFont(mf);

  auto dim = painter.pen().color();
  dim.setAlphaF(0.6);
  painter.setPen(dim);
  painter.drawText(area.adjusted(headW + gap, 0, 0, 0), Qt::AlignVCenter, split.marker);

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
    le->setTextMargins(2, 0, 2, 0);
    le->setMinimumSize(0, 0);
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
        const int smaller = score::snapToFontGrid(f, f.pixelSize() - 1);
        if(smaller >= f.pixelSize())
          break; // A pixel font with no smaller grid step: stop shrinking.
        f.setPixelSize(smaller);
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
  // Most specific first: what the device says the string means, then what its
  // unit says the numbers mean, then its type.
  auto* widg = make_extended_type_widget(addr, parent);

  if(!widg)
    widg = make_unit_widget(addr, parent, size);

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
  // A genuine list, and one whose elements are of the parameter's own type.
  auto* widg = new ListValueWidget{addr.value, parent};
  widg->set(ossia::value{ossia::get_values(addr.domain.get())});
  widg->installValueMenu();
  return widg;
}
}
