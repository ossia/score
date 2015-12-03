#pragma once
#include <QColor>
#include <QDateTime>
#include <QtGlobal>
#include <QGraphicsItem>
#include <QMap>
#include <QPainterPath>
#include <QPoint>
#include <QString>

class AbstractTimeRuler;
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class AbstractTimeRulerView : public QGraphicsObject
{
        Q_OBJECT
        AbstractTimeRuler* m_pres{};
    public:
        AbstractTimeRulerView();
        void setPresenter(AbstractTimeRuler* pres) { m_pres = pres; }
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setHeight(qreal newHeight);
        void setWidth(qreal newWidth);

        qreal graduationSpacing() const {return m_intervalsBeetwenMark * m_graduationsSpacing;}

        qreal width() const
        {
            return m_width;
        }

    signals:
        void drag(QPointF, QPointF);

    public slots:
        void setGraduationsStyle(double size, int delta, QString format, int mark);
        void setFormat(QString);


    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent*) final override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent*) final override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent*) final override;
        void createRulerPath();

        qreal m_height {};
        qreal m_width {};

        qreal m_graduationsSpacing {};
        int m_graduationDelta {};
        QString m_timeFormat{};
        int m_intervalsBeetwenMark {};

        qreal m_textPosition{};
        int m_graduationHeight {};

        QColor m_color;
        QPainterPath m_path;

        QMap<double, QTime> m_marks;
};
