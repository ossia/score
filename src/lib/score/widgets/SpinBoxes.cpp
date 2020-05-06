#include "SpinBoxes.hpp"
#include <ossia/detail/flicks.hpp>
#include <wobjectimpl.h>
#include <QStyleOptionComplex>
#include <QPainter>
W_OBJECT_IMPL(score::TimeSpinBox)
namespace score
{
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
  m_flicks = t.impl;
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
}

void TimeSpinBox::mouseReleaseEvent(QMouseEvent* event)
{
}

void TimeSpinBox::mouseDoubleClickEvent(QMouseEvent* event)
{
}

void TimeSpinBox::mouseMoveEvent(QMouseEvent* event)
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

    constexpr const auto bar_duration = 4 * ossia::quarter_duration<int64_t>;
    constexpr const auto qrt_duration = ossia::quarter_duration<int64_t>;
    constexpr const auto cnt_duration = ossia::quarter_duration<int64_t> / 100;
    int bars = m_flicks / (bar_duration);
    int quarters = (m_flicks - (bars * bar_duration)) / qrt_duration;
    int rem = (m_flicks - (bars * bar_duration) - (quarters * qrt_duration)) / cnt_duration;

    p.drawText(rect().adjusted(2, 2, -4, -2), QString("%1 . %2 . %3").arg(bars).arg(quarters).arg(rem), QTextOption(Qt::AlignRight));

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
