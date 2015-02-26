#include "ClickableLabel.hpp"

ClickableLabel::ClickableLabel (QString text, QWidget* parent) :
    QLabel {text, parent}
{
}

void ClickableLabel::enterEvent (QEvent*)
{
    setStyleSheet ("QLabel { text-decoration: underline; }");
}

void ClickableLabel::leaveEvent (QEvent*)
{
    setStyleSheet ("QLabel { }");
}

void ClickableLabel::mousePressEvent (QMouseEvent* event)
{
    emit clicked (this);
}
