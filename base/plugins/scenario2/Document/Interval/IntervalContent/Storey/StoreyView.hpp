#pragma once
#include <QNamedObject>

class StoreyView : public QNamedGraphicsObject
{
	Q_OBJECT

	public:
		StoreyView(QGraphicsObject* parent);
		virtual ~StoreyView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

		int m_height;

	private:

};

