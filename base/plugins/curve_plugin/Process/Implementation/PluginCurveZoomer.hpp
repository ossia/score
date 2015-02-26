#ifndef PLUGINCURVEZOOMER_HPP
#define PLUGINCURVEZOOMER_HPP

#include <QGraphicsObject>

/*!
 *  This class is the parent class of graphics item. Transformations will be applied on it.
 *  @n
 *
 *  @brief Parent of graphics Item. Zoom methods.
 *  @author Simon Touchard, Myriam Desainte-Catherine
 *  @date 2014
 */


class PluginCurveZoomer : public QGraphicsObject
{
        Q_OBJECT
    public:
        //! Zoomer constructor.
        /*!
           \param parent The parent item.
        */
        explicit PluginCurveZoomer (QGraphicsObject* parent = 0);
        void paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);
        QRectF boundingRect() const;
        /*! Change the zoomer scale (and move all others graphics item consequently). Origin is the origin of the transformation in zoomer coordinates. The real delta if the zoom factor value. */
        void zoom (QPointF origin, qreal delta);
        /*! Translate the zoomer position (+ dx) (and all others graphics items) horizontally. */
        void translateX (qreal dx);
        /*! Translate the zoomer position (+ dy) (and all others graphics items) vertically. */
        void translateY (qreal dy);
    signals:

    public slots:

};

#endif // PLUGINCURVEZOOMER_HPP
