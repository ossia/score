#include "ClickableLabel.hpp"

ClickableLabel::ClickableLabel(QString text, QWidget* parent):
	QLabel{text, parent}
{
	setStyleSheet("QLabel { color: blue }");
}

void ClickableLabel::enterEvent(QEvent*)
{
	setStyleSheet("QLabel { color: blue; text-decoration: underline; }");
}

void ClickableLabel::leaveEvent(QEvent*)
{
	setStyleSheet("QLabel { color: blue; }");
}

void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
	emit clicked(this);
}
