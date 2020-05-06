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
TimeSpinBox::TimeSpinBox(QWidget* parent)
  : QWidget(parent)
{
  m_flicks = 12345678910;
  //setDisplayFormat(QStringLiteral("h.mm.ss.zzz"));
  //setAlignment(Qt::AlignRight);
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
  const auto w = text_rect.width();
  const auto cent_start = w - centStart;
  const auto sq_start = w - semiquaverStart;
  const auto q_start = w - quarterStart;
  const auto bar_w = q_start - text_rect.x();

#if defined(__APPLE__)
  CGEventRef event = CGEventCreate(nullptr);
  CGPoint loc = CGEventGetLocation(event);
  CFRelease(event);

  m_startPos = {loc.x, loc.y};
#else
  m_startPos = event->globalPos();
#endif

  m_origFlicks = m_flicks;
  m_travelledY = 0;
  m_prevY = event->y();
  if(event->x() < bar_w)
  {
    m_grab = Bar;
    event->accept();
  }
  else if(event->x() > q_start && event->x() < sq_start)
  {
    m_grab = Quarter;
    event->accept();
  }
  else if(event->x() > sq_start && event->x() < cent_start)
  {
    m_grab = Semiquaver;
    event->accept();
  }
  else
  {
    m_grab = Cent;
    event->accept();
  }

#if defined(__APPLE__)
    score::hideCursor(true);
#else
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
#endif
}

void TimeSpinBox::mouseMoveEvent(QMouseEvent* event)
{
  int pixelsTraveled = m_prevY - event->globalPos().y();
  m_travelledY += pixelsTraveled;
  m_prevY = event->globalPos().y();

  double subdivDelta = m_travelledY / 5.;

  switch(m_grab)
  {
    case Bar:
      m_flicks = m_origFlicks + subdivDelta * 4 * ossia::quarter_duration<int64_t>;
      break;
    case Quarter:
      m_flicks = m_origFlicks + subdivDelta * ossia::quarter_duration<int64_t>;
      break;
    case Semiquaver:
      m_flicks = m_origFlicks + subdivDelta * ossia::quarter_duration<int64_t> / 4;
      break;
    case Cent:
      m_flicks = m_origFlicks + subdivDelta * ossia::quarter_duration<int64_t> / 400;
      break;
    default:
      break;
  }

  if (m_flicks < 0)
    m_flicks = 0;

  score::moveCursorPos(m_startPos);
  score::hideCursor(true);
  updateTime();
}

void TimeSpinBox::mouseReleaseEvent(QMouseEvent* event)
{
  score::showCursor();

  score::setCursorPos(m_startPos);
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
    const auto w = text_rect.width();
    const auto cent_start = w - centStart;
    const auto sq_start = w - semiquaverStart;
    const auto q_start = w - quarterStart;
    const auto bar_w = q_start - text_rect.x();

    p.drawText(QRect{text_rect.x(), text_rect.y(), bar_w, text_rect.height()}, QString::number(m_barTime.bars), QTextOption(Qt::AlignRight));
    p.drawText(QRect{q_start, text_rect.y(), 20, text_rect.height()}, QString::number(m_barTime.quarters), QTextOption(Qt::AlignRight));
    p.drawText(QRect{sq_start, text_rect.y(), 20, text_rect.height()}, QString::number(m_barTime.semiquavers), QTextOption(Qt::AlignRight));
    p.drawText(QRect{cent_start, text_rect.y(), 20, text_rect.height()}, QString{"%1"}.arg(m_barTime.cents, 2, 10, QChar('0')), QTextOption(Qt::AlignRight));

    QString txt = QString("%1 . %2 . %3 . %4")
        .arg(m_barTime.bars)
        .arg(m_barTime.quarters)
        .arg(m_barTime.semiquavers)
        .arg(m_barTime.cents);

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
