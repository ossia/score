#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/SearchLineEdit.hpp>

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

SearchLineEdit::~SearchLineEdit() {}

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

SpeedSlider::SpeedSlider(QWidget* parent) : Slider{parent}
{
  setTickInterval(100);
  setMinimum(-100);
  setMaximum(500);
}

void SpeedSlider::paintEvent(QPaintEvent*)
{
  QString text;
  text.reserve(16);
  text += (showText) ? "speed: × " : "× ";

  text += QString::number(double(value()) * 0.01, 'f', 2);
  paintWithText(text);
}

void SpeedSlider::mousePressEvent(QMouseEvent* ev)
{
  if (ev->button() == Qt::LeftButton)
    return Slider::mousePressEvent(ev);

  if (qApp->keyboardModifiers() & Qt::CTRL)
  {
    setValue(100);
  }
  else
  {
    QTimer::singleShot(0, [&, &self = *this, pos = ev->screenPos()] {
      auto w = new score::DoubleSpinboxWithEnter;
      w->setWindowFlag(Qt::Tool);
      w->setWindowFlag(Qt::FramelessWindowHint);
      w->setRange(-1., 5.);
      w->setDecimals(2);
      w->setValue(self.value() / 100.);
      w->show();
      w->move(pos.x(), pos.y());
      QTimer::singleShot(5, w, [w] { w->setFocus(); });
      QObject::connect(
          w,
          SignalUtils::QDoubleSpinBox_valueChanged_double(),
          &self,
          [=, &self](double v) { self.setValue(v * 100.); });

      QObject::connect(
          w,
          &DoubleSpinboxWithEnter::editingFinished,
          w,
          &QObject::deleteLater);
    });
  }
  ev->ignore();
}

void VolumeSlider::paintEvent(QPaintEvent*)
{
  paintWithText(
      "vol: "
      + QString::number(ossia::detail::LinearGainToDecibels(value()), 'f', 1)
      + " dB");
}

void ValueDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(min + value() * (max - min), 'f', 3));
}

void ValueLogDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(
      ossia::normalized_to_log(min, max - min, value()), 'f', 3));
}

ComboSlider::ComboSlider(const QStringList& arr, QWidget* parent)
    : score::Slider{parent}, array{arr}
{
}

void ComboSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(array[value()]);
}
}
