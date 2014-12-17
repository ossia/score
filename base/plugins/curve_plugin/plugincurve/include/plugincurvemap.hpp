#ifndef PLUGINCURVEMAP_HPP
#define PLUGINCURVEMAP_HPP

#include <QObject>
#include <QRectF>
class PluginCurveMap : public QObject
{
    Q_OBJECT

    /*!
    *  This class enables to convert items in View coordinates to scale coordinates and vice versa.
    *  @n
    *
    *  @brief Convert coordinates
    *  @author Simon Touchard, Myriam Desainte-Catherine
    *  @date 2014
    */

// Attributes
private:
    QRectF *_pScaleRect; /*!< Define points' area in scale coordinate. */
    QRectF *_pPaintRect; /*!< Define points' area in paint coordinate. */
// Methods
public:
    //! Construct a PluginCurveMap.
    /*!
    \param scaleRect The rectangle delimitting the points in scale coordinate.
    \param paintRect The rectangle delimitting the points in paint coordinate.
    */
    PluginCurveMap(QRectF scaleRect, QRectF paintRect);
    /*! Transform a point from scale to paint coordinates. */
    QPointF scaleToPaint(QPointF val);
    /*! Transform a rectangle from scale to paint coordinates. */
    QRectF scaleToPaint(QRectF rect);
    /*! Transform a point from paint to scale coordinates. */
    QPointF paintToScale(QPointF pos);
    /*! Transform a rectangle from paint to scale coordinates. */
    QRectF paintToScale(QRectF rect);
    /*! Returns the point's area in paint coordinate */
    QRectF paintRect();
    /*! Returns the point's area in scale coordinate */
    QRectF scaleRect();
    /*! Set the point's area in paint coordinate. */
    void setScaleRect(QRectF scaleRect);
    /*! Set the point's area in scale coordinate. If changeScaleRect is true, the scale rect will be modifie consequently. */
    void setPaintRect(QRectF paintRect, bool changeScaleRect = true);
signals:
    void mapChanged(); //! Indicates if a a cange occured.
public slots:

};

#endif // PLUGINCURVEMAP_HPP
