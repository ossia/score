#include "AbstractConstraintView.hpp"

AbstractConstraintView::AbstractConstraintView(QGraphicsObject *parent):
	QGraphicsObject{parent}
{

}

void AbstractConstraintView::setDefaultWidth(int width)
{
	prepareGeometryChange();
	m_defaultWidth = width;
}

void AbstractConstraintView::setMaxWidth(int max)
{
	prepareGeometryChange();
	m_maxWidth = max;
}

void AbstractConstraintView::setMinWidth(int min)
{
	prepareGeometryChange();
	m_minWidth = min;
}

void AbstractConstraintView::setHeight(int height)
{
	prepareGeometryChange();
	m_height = height;
}
