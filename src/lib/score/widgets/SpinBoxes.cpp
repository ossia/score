#include "SpinBoxes.hpp"
#include <ossia/detail/flicks.hpp>
#include <wobjectimpl.h>
#include <QStyleOptionComplex>
#include <QPainter>
#include <score/tools/Cursor.hpp>
W_OBJECT_IMPL(score::TimeSpinBox)
namespace score
{
static const constexpr int centStart = 20;
static const constexpr int semiquaverStart = 40;
static const constexpr int quarterStart = 60;
static const constexpr int millisecondStart = 20;
static const constexpr int secondStart = 50;
static const constexpr int minuteStart = 70;


struct BarSpinBox {
  TimeSpinBox& self;
  void paint(QPainter& p, QRect text_rect)
  {
    auto& m_barTime = self.m_barTime;
    const auto w = text_rect.width();
    const auto cent_start = w - centStart;
    const auto sq_start = w - semiquaverStart;
    const auto q_start = w - quarterStart;
    const auto bar_w = q_start - text_rect.x();

    p.drawText(QRect{text_rect.x(), text_rect.y(), bar_w, text_rect.height()}, QString{"%1 ."}.arg(m_barTime.bars), QTextOption(Qt::AlignRight));
    p.drawText(QRect{q_start, text_rect.y(), 20, text_rect.height()}, QString{"%1 ."}.arg(m_barTime.quarters), QTextOption(Qt::AlignRight));
    p.drawText(QRect{sq_start, text_rect.y(), 20, text_rect.height()}, QString{"%1 ."}.arg(m_barTime.semiquavers), QTextOption(Qt::AlignRight));
    p.drawText(QRect{cent_start, text_rect.y(), 30, text_rect.height()}, QString{"%1"}.arg(m_barTime.cents, 2, 10, QChar('0')), QTextOption(Qt::AlignRight));
  }

  void mousePress(QRect text_rect, QMouseEvent* event)
  {
    const auto w = text_rect.width();
    const auto cent_start = w - centStart;
    const auto sq_start = w - semiquaverStart;
    const auto q_start = w - quarterStart;
    const auto bar_w = q_start - text_rect.x();

    self.m_origFlicks = self.m_flicks;
    self.m_travelledY = 0;
    self.m_prevY = event->globalPos().y();
    if(event->x() < bar_w)
    {
      self.m_grab = TimeSpinBox::Bar;
      event->accept();
    }
    else if(event->x() > q_start && event->x() < sq_start)
    {
      self.m_grab = TimeSpinBox::Quarter;
      event->accept();
    }
    else if(event->x() > sq_start && event->x() < cent_start)
    {
      self.m_grab = TimeSpinBox::Semiquaver;
      event->accept();
    }
    else
    {
      self.m_grab = TimeSpinBox::Cent;
      event->accept();
    }
  }

  void mouseMove(QMouseEvent* event)
  {
    int pixelsTraveled = self.m_prevY - event->globalPos().y();
    self.m_travelledY += pixelsTraveled;
    self.m_prevY = event->globalPos().y();

    double subdivDelta = std::floor(self.m_travelledY / 6.);

    switch(self.m_grab)
    {
      case TimeSpinBox::Bar:
        self.m_flicks = self.m_origFlicks + subdivDelta * 4 * ossia::quarter_duration<int64_t>;
        break;
      case TimeSpinBox::Quarter:
        self.m_flicks = self.m_origFlicks + subdivDelta * ossia::quarter_duration<int64_t>;
        break;
      case TimeSpinBox::Semiquaver:
        self.m_flicks = self.m_origFlicks + subdivDelta * ossia::quarter_duration<int64_t> / 4;
        break;
      case TimeSpinBox::Cent:
        self.m_flicks = self.m_origFlicks + subdivDelta * ossia::quarter_duration<int64_t> / 400;
        break;
      default:
        break;
    }

    if (self.m_flicks < 0)
      self.m_flicks = 0;

    event->accept();
  }

  void mouseRelease(QMouseEvent* event)
  {
    mouseMove(event);
    event->accept();
  }
};

struct SecondSpinBox {
  TimeSpinBox& self;
  void paint(QPainter& p, QRect text_rect)
  {
    QTime t = QTime(0, 0, 0, 0).addMSecs(self.time().impl / ossia::flicks_per_millisecond<double>);
    const auto w = text_rect.width();
    const auto cent_start = w - millisecondStart;
    const auto sq_start = w - secondStart;
    const auto q_start = w - minuteStart;
    const auto bar_w = q_start - text_rect.x();

    p.drawText(QRect{text_rect.x(), text_rect.y(), bar_w, text_rect.height()}, QString{"%1 :"}.arg(t.hour()), QTextOption(Qt::AlignRight));
    p.drawText(QRect{q_start, text_rect.y(), 20, text_rect.height()}, QString{"%1 :"}.arg(t.minute()), QTextOption(Qt::AlignRight));
    p.drawText(QRect{sq_start, text_rect.y(), 20, text_rect.height()}, QString{"%1 ."}.arg(t.second()), QTextOption(Qt::AlignRight));
    p.drawText(QRect{cent_start, text_rect.y(), 30, text_rect.height()}, QString{"%1"}.arg(t.msec(), 2, 10, QChar('0')), QTextOption(Qt::AlignRight));
  }

  void mousePress(QRect text_rect, QMouseEvent* event)
  {
    const auto w = text_rect.width();
    const auto cent_start = w - millisecondStart;
    const auto sq_start = w - secondStart;
    const auto q_start = w - minuteStart;
    const auto bar_w = q_start - text_rect.x();

    self.m_origFlicks = self.m_flicks;
    self.m_travelledY = 0;
    self.m_prevY = event->globalPos().y();
    if(event->x() < bar_w)
    {
      self.m_grab = TimeSpinBox::Bar;
      event->accept();
    }
    else if(event->x() > q_start && event->x() < sq_start)
    {
      self.m_grab = TimeSpinBox::Quarter;
      event->accept();
    }
    else if(event->x() > sq_start && event->x() < cent_start)
    {
      self.m_grab = TimeSpinBox::Semiquaver;
      event->accept();
    }
    else
    {
      self.m_grab = TimeSpinBox::Cent;
      event->accept();
    }
  }

  void mouseMove(QMouseEvent* event)
  {
    int pixelsTraveled = self.m_prevY - event->globalPos().y();
    self.m_travelledY += pixelsTraveled;
    self.m_prevY = event->globalPos().y();

    double subdivDelta = std::floor(self.m_travelledY / 6.);

    switch(self.m_grab)
    {
      case TimeSpinBox::Bar: // Hour
        self.m_flicks = self.m_origFlicks + subdivDelta * 3600 * 1000 * ossia::flicks_per_millisecond<int64_t>;
        break;
      case TimeSpinBox::Quarter: // Minute
        self.m_flicks = self.m_origFlicks + subdivDelta * 60 * 1000 * ossia::flicks_per_millisecond<int64_t>;
        break;
      case TimeSpinBox::Semiquaver: // Second
        self.m_flicks = self.m_origFlicks + subdivDelta * 1000 * ossia::flicks_per_millisecond<int64_t>;
        break;
      case TimeSpinBox::Cent: // ms
        self.m_flicks = self.m_origFlicks + subdivDelta * ossia::flicks_per_millisecond<int64_t>;
        break;
      default:
        break;
    }

    if (self.m_flicks < 0)
      self.m_flicks = 0;

    event->accept();
  }

  void mouseRelease(QMouseEvent* event)
  {
    mouseMove(event);
    event->accept();
  }
};

struct FlicksSpinBox {
  TimeSpinBox& self;
  void paint(QPainter& p, QRect text_rect)
  {
  }

  void mousePress(QRect text_rect, QMouseEvent* event)
  {
  }

  void mouseMove(QMouseEvent* event)
  {
  }

  void mouseRelease(QMouseEvent* event)
  {
  }
};

static std::vector<TimeSpinBox*> spinBoxes;
TimeSpinBox::TimeMode globalTimeMode = TimeSpinBox::TimeMode::Bars;

TimeSpinBox::TimeSpinBox(QWidget* parent)
  : QWidget(parent)
{
  spinBoxes.push_back(this);
  m_mode = globalTimeMode;
}

TimeSpinBox::~TimeSpinBox()
{
  ossia::remove_one(spinBoxes, this);
}

void TimeSpinBox::setGlobalTimeMode(TimeSpinBox::TimeMode mode)
{
  globalTimeMode = mode;
  for(auto sb : spinBoxes)
  {
    sb->m_mode = mode;
    sb->update();
  }
}

void TimeSpinBox::setMinimumTime(ossia::time_value t)
{

}

void TimeSpinBox::setMaximumTime(ossia::time_value t)
{

}

void TimeSpinBox::setTime(ossia::time_value t)
{
  if(m_flicks != t.impl)
  {
    m_flicks = t.impl;
    updateTime();
  }
}

void TimeSpinBox::updateTime()
{
  constexpr const auto bar_duration = 4 * ossia::quarter_duration<int64_t>;
  constexpr const auto qrt_duration = ossia::quarter_duration<int64_t>;
  constexpr const auto sem_duration = ossia::quarter_duration<int64_t> / 4;
  constexpr const auto cnt_duration = ossia::quarter_duration<int64_t> / 400;
  const int64_t bars = m_flicks / (bar_duration);
  const int64_t quarters = (m_flicks - (bars * bar_duration)) / qrt_duration;
  const int64_t semiquavers = (m_flicks - (bars * bar_duration) - (quarters * qrt_duration)) / sem_duration;
  const int64_t cents = (m_flicks - (bars * bar_duration) - (quarters * qrt_duration)- (semiquavers * sem_duration)) / cnt_duration;
  m_barTime.bars = bars;
  m_barTime.quarters = quarters;
  m_barTime.semiquavers = semiquavers;
  m_barTime.cents = cents;
  update();
}

ossia::time_value TimeSpinBox::time() const noexcept
{
  return {m_flicks};
}

void TimeSpinBox::wheelEvent(QWheelEvent* event)
{
  event->ignore();
}

void TimeSpinBox::mousePressEvent(QMouseEvent* event)
{
  const auto text_rect = rect().adjusted(2, 2, -4, -2);

#if defined(__APPLE__)
  CGPoint loc;
  {
    CGEventRef event = CGEventCreate(nullptr);
    loc = CGEventGetLocation(event);
    CFRelease(event);
  }
  m_startPos = QPoint(loc.x, loc.y);
#else
  m_startPos = event->globalPos();
#endif

  switch(m_mode)
  {
    case Bars: BarSpinBox{*this}.mousePress(text_rect, event); break;
    case Seconds: SecondSpinBox{*this}.mousePress(text_rect, event); break;
    case Flicks: FlicksSpinBox{*this}.mousePress(text_rect, event); break;
  }

// #if defined(__APPLE__)
//     score::hideCursor(true);
// #else
//     QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
// #endif
}

void TimeSpinBox::mouseMoveEvent(QMouseEvent* event)
{
  switch(m_mode)
  {
    case Bars: BarSpinBox{*this}.mouseMove(event); break;
    case Seconds: SecondSpinBox{*this}.mouseMove(event); break;
    case Flicks: FlicksSpinBox{*this}.mouseMove(event); break;
  }

  //score::moveCursorPos(m_startPos);
// score::hideCursor(true);
  updateTime();
  timeChanged({this->m_flicks});
}

void TimeSpinBox::mouseReleaseEvent(QMouseEvent* event)
{
  switch(m_mode)
  {
    case Bars: BarSpinBox{*this}.mouseRelease(event); break;
    case Seconds: SecondSpinBox{*this}.mouseRelease(event); break;
    case Flicks: FlicksSpinBox{*this}.mouseRelease(event); break;
  }

//  score::showCursor();
//  score::setCursorPos(m_startPos);
  updateTime();
  editingFinished();
}

void TimeSpinBox::mouseDoubleClickEvent(QMouseEvent* event)
{
}

void TimeSpinBox::initStyleOption(QStyleOptionFrame *option) const noexcept
{
    option->initFrom(this);
    option->rect = contentsRect();
    constexpr bool frame = true;
    option->lineWidth = frame ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, this) : 0;
    option->midLineWidth = 0;
    option->state |= QStyle::State_Sunken;
#ifdef QT_KEYPAD_NAVIGATION
    if (hasEditFocus())
        option->state |= QStyle::State_HasEditFocus;
#endif
    option->features = QStyleOptionFrame::None;
}

QSize TimeSpinBox::sizeHint() const
{
  return {150, 20};
}

QSize TimeSpinBox::minimumSizeHint() const
{
  return {50, 20};
}


void TimeSpinBox::paintEvent(QPaintEvent* event)
{
  QPainter p(this);
  QPalette pal = palette();

  QStyleOptionFrame panel;
  initStyleOption(&panel);
  style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);
  QRect r = style()->subElementRect(QStyle::SE_LineEditContents, &panel, this);
  // r = r.marginsRemoved(d->effectiveTextMargins());
  p.setClipRect(r);

  QFontMetrics fm = fontMetrics();

  const auto text_rect = rect().adjusted(2, 2, -4, -2);
  switch(m_mode)
  {
    case Bars: BarSpinBox{*this}.paint(p, text_rect); break;
    case Seconds: SecondSpinBox{*this}.paint(p, text_rect); break;
    case Flicks: FlicksSpinBox{*this}.paint(p, text_rect); break;
  }

    /*
    Qt::Alignment va = QStyle::visualAlignment(d->control->layoutDirection(), QFlag(d->alignment));
    switch (va & Qt::AlignVertical_Mask) {
     case Qt::AlignBottom:
         d->vscroll = r.y() + r.height() - fm.height() - QLineEditPrivate::verticalMargin;
         break;
     case Qt::AlignTop:
         d->vscroll = r.y() + QLineEditPrivate::verticalMargin;
         break;
     default:
         //center
         d->vscroll = r.y() + (r.height() - fm.height() + 1) / 2;
         break;
    }
    QRect lineRect(r.x() + QLineEditPrivate::horizontalMargin, d->vscroll,
                   r.width() - 2 * QLineEditPrivate::horizontalMargin, fm.height());



    int cix = qRound(d->control->cursorToX());

    // horizontal scrolling. d->hscroll is the left indent from the beginning
    // of the text line to the left edge of lineRect. we update this value
    // depending on the delta from the last paint event; in effect this means
    // the below code handles all scrolling based on the textline (widthUsed),
    // the line edit rect (lineRect) and the cursor position (cix).
    int widthUsed = qRound(d->control->naturalTextWidth()) + 1;
    if (widthUsed <= lineRect.width()) {
        // text fits in lineRect; use hscroll for alignment
        switch (va & ~(Qt::AlignAbsolute|Qt::AlignVertical_Mask)) {
        case Qt::AlignRight:
            d->hscroll = widthUsed - lineRect.width() + 1;
            break;
        case Qt::AlignHCenter:
            d->hscroll = (widthUsed - lineRect.width()) / 2;
            break;
        default:
            // Left
            d->hscroll = 0;
            break;
        }
    } else if (cix - d->hscroll >= lineRect.width()) {
        // text doesn't fit, cursor is to the right of lineRect (scroll right)
        d->hscroll = cix - lineRect.width() + 1;
    } else if (cix - d->hscroll < 0 && d->hscroll < widthUsed) {
        // text doesn't fit, cursor is to the left of lineRect (scroll left)
        d->hscroll = cix;
    } else if (widthUsed - d->hscroll < lineRect.width()) {
        // text doesn't fit, text document is to the left of lineRect; align
        // right
        d->hscroll = widthUsed - lineRect.width() + 1;
    } else {
        //in case the text is bigger than the lineedit, the hscroll can never be negative
        d->hscroll = qMax(0, d->hscroll);
    }


    // the y offset is there to keep the baseline constant in case we have script changes in the text.
    QPoint topLeft = lineRect.topLeft() - QPoint(d->hscroll, d->control->ascent() - fm.ascent());

    // draw text, selections and cursors
    p.setPen(pal.text().color());

    int flags = QWidgetLineControl::DrawText;

#ifdef QT_KEYPAD_NAVIGATION
    if (!QApplicationPrivate::keypadNavigationEnabled() || hasEditFocus())
#endif
    if (d->control->hasSelectedText() || (d->cursorVisible && !d->control->inputMask().isEmpty() && !d->control->isReadOnly())){
        flags |= QWidgetLineControl::DrawSelections;
        // Palette only used for selections/mask and may not be in sync
        if (d->control->palette() != pal
           || d->control->palette().currentColorGroup() != pal.currentColorGroup())
            d->control->setPalette(pal);
    }

    // Asian users see an IM selection text as cursor on candidate
    // selection phase of input method, so the ordinary cursor should be
    // invisible if we have a preedit string.
    if (d->cursorVisible && !d->control->isReadOnly())
        flags |= QWidgetLineControl::DrawCursor;

    d->control->setCursorWidth(style()->pixelMetric(QStyle::PM_TextCursorWidth, &panel));
    d->control->draw(&p, topLeft, r, flags);
    */

}

}
