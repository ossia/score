#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SearchLineEdit.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/network/dataspace/gain.hpp>

#include <QAction>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>

#include <cmath>
#include <wobjectimpl.h>
W_OBJECT_IMPL(score::SearchLineEdit)
W_OBJECT_IMPL(score::RangeWidget)
namespace score
{
RangeWidget::RangeWidget(bool integral, QWidget* parent)
    : QWidget{parent}
    , m_start{new QDoubleSpinBox{this}}
    , m_end{new QDoubleSpinBox{this}}
{
  auto lay = new QHBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);
  lay->addWidget(m_start);
  lay->addWidget(m_end);

  for(auto* sb : {m_start, m_end})
  {
    sb->setDecimals(integral ? 0 : 3);
    sb->setSingleStep(integral ? 1. : 0.01);
    sb->setKeyboardTracking(false);
    sb->setContentsMargins(0, 0, 0, 0);
  }
  m_start->setPrefix(tr("min "));
  m_end->setPrefix(tr("max "));

  connect(
      m_start, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
      &RangeWidget::onStartEdited);
  connect(
      m_end, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
      &RangeWidget::onEndEdited);
}

RangeWidget::~RangeWidget() = default;

void RangeWidget::setRange(double min, double max, ossia::vec2f init)
{
  if(max <= min)
    max = min + 1.;

  m_init = init;

  // Only the bounds change here: the current value must survive a domain
  // change, it is set separately through setValue.
  m_updating = true;
  m_start->setRange(min, max);
  m_end->setRange(min, max);
  m_updating = false;
}

void RangeWidget::setValue(ossia::vec2f value)
{
  const double lo = std::min(value[0], value[1]);
  const double hi = std::max(value[0], value[1]);

  m_updating = true;
  m_start->setValue(lo);
  m_end->setValue(hi);
  m_updating = false;
}

ossia::vec2f RangeWidget::value() const noexcept
{
  return ossia::vec2f{(float)m_start->value(), (float)m_end->value()};
}

void RangeWidget::onStartEdited()
{
  if(m_updating)
    return;

  // Keep the pair ordered by pushing the other bound out of the way
  if(m_start->value() > m_end->value())
  {
    m_updating = true;
    m_end->setValue(m_start->value());
    m_updating = false;
  }

  moving = true;
  sliderMoved();
  moving = false;
  sliderReleased();
}

void RangeWidget::onEndEdited()
{
  if(m_updating)
    return;

  if(m_end->value() < m_start->value())
  {
    m_updating = true;
    m_start->setValue(m_end->value());
    m_updating = false;
  }

  moving = true;
  sliderMoved();
  moving = false;
  sliderReleased();
}

SearchLineEdit::SearchLineEdit(QWidget* parent)
    : QLineEdit{parent}
{
  setObjectName("SearchLineEdit");
  setPlaceholderText("Search");
  auto act = new QAction{this};
  act->setIcon(QIcon(":/icons/search.png"));
  score::setHelp(act, tr("Filter with the given text"));
  addAction(act, QLineEdit::TrailingPosition);

  connect(this, &QLineEdit::returnPressed, this, [&]() { search(); });
  connect(act, &QAction::triggered, this, [&]() { search(); });
}

SearchLineEdit::~SearchLineEdit() { }

SCORE_LIB_BASE_EXPORT
const QPalette& transparentPalette()
{
  static QPalette p{[] {
    QPalette palette;
    palette.setBrush(QPalette::Window, Qt::transparent);
    return palette;
  }()};
  return p;
}

ToggleButton::ToggleButton(std::array<QString, 2> alts, QWidget* parent)
    : QPushButton{parent}
    , alternatives{alts}
{
  setCheckable(true);

  connect(this, &QPushButton::toggled, this, [&](bool b) {
    if(b)
    {
      setText(alternatives[1]);
    }
    else
    {
      setText(alternatives[0]);
    }
  });
  if(isChecked())
  {
    setText(alternatives[1]);
  }
  else
  {
    setText(alternatives[0]);
  }
}

ToggleButton::ToggleButton(std::array<const char*, 2> alt, QWidget* parent)
    : ToggleButton{std::array<QString, 2>{alt[0], alt[1]}, parent}
{
}

ToggleButton::ToggleButton(QStringList alt, QWidget* parent)
    : ToggleButton{std::array<QString, 2>{alt[0], alt[1]}, parent}
{
}

ToggleButton::~ToggleButton() { }

void ToggleButton::paintEvent(QPaintEvent* event)
{
  return QPushButton::paintEvent(event);
  /*
  QStyleOptionButton opt;
  opt.text = this->isChecked() ? alternatives[1] : alternatives[0];
  opt.state |= QStyle::State_On;
  opt.state |= QStyle::State_Enabled;
  opt.state |= this->isChecked() ? QStyle::State_Raised : QStyle::State_Sunken;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);*/
}

ValueSlider::~ValueSlider() { }

void ValueSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(value()));
}

double VolumeSlider::map(double v) const
{
  return ossia::detail::LinearGainToDecibels(v);
}

double VolumeSlider::unmap(double v) const
{
  return ossia::detail::DecibelsToLinearGain(v);
}

VolumeSlider::~VolumeSlider() { }

void VolumeSlider::paintEvent(QPaintEvent*)
{
  paintWithText("vol: " + QString::number(map(value()), 'f', 1) + " dB");
}

ValueDoubleSlider::~ValueDoubleSlider() { }

void ValueDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(map(value()), 'f', 3));
}

ValueLogDoubleSlider::~ValueLogDoubleSlider() { }

double ValueLogDoubleSlider::map(double v) const
{
  return ossia::normalized_to_log(min, max - min, v);
}

double ValueLogDoubleSlider::unmap(double v) const
{
  return ossia::log_to_normalized(min, max - min, v);
}

void ValueLogDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(map(value()), 'f', 3));
}

ComboSlider::ComboSlider(const QStringList& arr, QWidget* parent)
    : score::IntSlider{parent}
    , array{arr}
{
}

ComboSlider::~ComboSlider() { }

void ComboSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(array[value()]);
}
}
