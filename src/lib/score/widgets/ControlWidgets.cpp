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
SearchLineEdit::SearchLineEdit(QWidget* parent) : QLineEdit{parent}
{
  setObjectName("SearchLineEdit");
  setPlaceholderText("Search");
  auto act = new QAction{this};
  act->setIcon(QIcon(":/icons/search.png"));
  act->setStatusTip(tr("Filter with the given text"));
  addAction(act, QLineEdit::TrailingPosition);

  connect(this, &QLineEdit::returnPressed, [&]() { search(); });
  connect(act, &QAction::triggered, [&]() { search(); });
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
    : QPushButton{parent}, alternatives{alts}
{
  setCheckable(true);

  connect(this, &QPushButton::toggled, this, [&](bool b) {
    if (b)
    {
      setText(alternatives[1]);
    }
    else
    {
      setText(alternatives[0]);
    }
  });
  if (isChecked())
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

void ValueSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(value()));
}

/* Speed goes from -1 to 5 */
constexpr double valueFromSpeed(double speed)
{
  return (speed + 1.) / 6.;
}
constexpr double speedFromValue(double value)
{
  return value * 6. - 1.;
}

SpeedSlider::SpeedSlider(QWidget* parent) : DoubleSlider{parent} { }

double SpeedSlider::speed() const noexcept
{
  return std::round(1000 * speedFromValue(value())) / 1000;
}

void SpeedSlider::setSpeed(double v)
{
  setValue(valueFromSpeed(v));
}

void SpeedSlider::setTempo(double t)
{
  setValue(valueFromSpeed(t / ossia::root_tempo));
}

void SpeedSlider::paintEvent(QPaintEvent*)
{
  QString text;
  text.reserve(16);
  text += (tempo) ? "" : (showText) ? "speed: × " : "× ";

  double v = speed();
  if (tempo)
  {
    v *= ossia::root_tempo;
    text += QString::number(v, 'f', 1);
  }
  else
  {
    text += QString::number(v, 'f', 2);
  }

  paintWithText(text);
}

void SpeedSlider::mousePressEvent(QMouseEvent* ev)
{
  if (ev->button() == Qt::LeftButton)
    return DoubleSlider::mousePressEvent(ev);

  if (qApp->keyboardModifiers() & Qt::CTRL)
  {
    setValue(valueFromSpeed(1.));
  }
  else
  {
    QTimer::singleShot(0, [&, &self = *this, pos = ev->screenPos()] {
      auto w = new score::DoubleSpinboxWithEnter;
      w->setWindowFlag(Qt::Tool);
      w->setWindowFlag(Qt::FramelessWindowHint);
      if (tempo)
      {
        w->setRange(20., 500.);
        w->setDecimals(1);
        w->setValue(speed() * ossia::root_tempo);

        QObject::connect(
            w, SignalUtils::QDoubleSpinBox_valueChanged_double(), &self, [=, &self](double v) {
              self.setValue(valueFromSpeed(v / ossia::root_tempo));
            });
      }
      else
      {
        w->setRange(-1., 5.);
        w->setDecimals(2);
        w->setValue(speed());

        QObject::connect(
            w, SignalUtils::QDoubleSpinBox_valueChanged_double(), &self, [=, &self](double v) {
              self.setValue(valueFromSpeed(v));
            });
      }

      w->show();
      w->move(pos.x(), pos.y());
      QTimer::singleShot(5, w, [w] { w->setFocus(); });
      QObject::connect(w, &DoubleSpinboxWithEnter::editingFinished, w, &QObject::deleteLater);
    });
  }
  ev->ignore();
}

void VolumeSlider::paintEvent(QPaintEvent*)
{
  paintWithText(
      "vol: " + QString::number(ossia::detail::LinearGainToDecibels(value()), 'f', 1) + " dB");
}

void ValueDoubleSlider::setRange(double min, double max) noexcept
{
  this->min = min;
  this->max = max;
  update();
}

void ValueDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(min + value() * (max - min), 'f', 3));
}

void ValueLogDoubleSlider::setRange(double min, double max) noexcept
{
  this->min = min;
  this->max = max;
  update();
}

void ValueLogDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(ossia::normalized_to_log(min, max - min, value()), 'f', 3));
}

ComboSlider::ComboSlider(const QStringList& arr, QWidget* parent)
    : score::IntSlider{parent}, array{arr}
{
}

void ComboSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(array[value()]);
}
}
