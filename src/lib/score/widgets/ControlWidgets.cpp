#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/SearchLineEdit.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/network/dataspace/gain.hpp>

#include <QAction>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>

#include <cmath>
#include <wobjectimpl.h>
W_OBJECT_IMPL(score::SearchLineEdit)
namespace score
{
SearchLineEdit::SearchLineEdit(QWidget* parent)
    : QLineEdit{parent}
{
  setObjectName("SearchLineEdit");
  setPlaceholderText("Search");
  auto act = new QAction{this};
  act->setIcon(QIcon(":/icons/search.png"));
  act->setStatusTip(tr("Filter with the given text"));
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
