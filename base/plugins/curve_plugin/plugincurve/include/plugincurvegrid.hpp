#ifndef PLUGINCURVEGRID_HPP
#define PLUGINCURVEGRID_HPP


#include <QGraphicsObject>
class PluginCurveView;
class PluginCurveMap;


class PluginCurveGrid : public QGraphicsObject
{

Q_OBJECT
/*!
*  This class represent the grid.
*  Permits to paint the grid. @n
*
*  @brief Grid
*  @author Simon Touchard, Myriam Desainte-Catherine
*  @date 2014
*/

// Attributes
private:
    PluginCurveMap *_pMap; /*!< Permit to transform devices from paint coordinate to scale coordinate and vice versa. */
    PluginCurveMap *_pDefaultMap; /*!< The default map if no map is giving. */
    QVector<qreal> magnetPointX; /*!< Magnetic points' abscissas. */
    QVector<qreal> magnetPointY; /*!< Magnetic points' ordonates. */
    qreal stepX; /*!< Horizontal distance (in scale coordinate) between two magnetic points. */
    qreal stepY; /*!< Vertical distance (in scale coordinate) between two magnetic points. */
public:
    //! Constructor.
    /*!
        \param parent The parent item.
        \param map The map used for transform coordinates.
    */
    PluginCurveGrid(QGraphicsObject *parent,PluginCurveMap *map);
    ~PluginCurveGrid();
    /*! Return the nearest magnetic point (in paint coordinate) of the point p (in paint coordinate)*/
    QPointF nearestMagnetPoint (QPointF p);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    QRectF boundingRect() const;

private:
    /*! Update position of magnetic points*/
    void updateMagnetPoints();
signals:

public slots:
    void mapChanged(); //! The map changed, redraw the grid.

};

#endif // PLUGINCURVEGRID_HPP
