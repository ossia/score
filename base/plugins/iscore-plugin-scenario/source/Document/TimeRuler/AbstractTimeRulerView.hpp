#pragma once
#include <QGraphicsObject>
#include <ProcessInterface/TimeValue.hpp>

class QPainterPath;
class AbstractTimeRuler;
class AbstractTimeRulerView : public QGraphicsObject
{
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

    public slots:
        void setGraduationsStyle(double size, int delta, QString format, int mark);
        void setFormat(QString);

    protected:
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
