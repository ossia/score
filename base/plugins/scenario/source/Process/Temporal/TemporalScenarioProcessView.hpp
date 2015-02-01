#pragma once
#include "ProcessInterface/ProcessViewInterface.hpp"

#include <QAction>

class TemporalScenarioProcessView : public ProcessViewInterface
{
	Q_OBJECT

	public:
		TemporalScenarioProcessView(QGraphicsObject* parent);

		virtual ~TemporalScenarioProcessView() = default;

		virtual QRectF boundingRect() const;
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);


	signals:
		void scenarioPressed();
        void scenarioPressedWithControl(QPointF, QPointF);
        void scenarioReleased(QPointF, QPointF);

		void deletePressed();
        void clearPressed();

	public slots:
		void lock()
		{ m_lock = true; update(); }
		void unlock()
		{ m_lock = false; update(); }

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

		virtual void keyPressEvent(QKeyEvent* event) override;

    private:
        QPointF m_clickedPoint{};
        QAction* m_clearAction{};

        bool m_lock{};
};





