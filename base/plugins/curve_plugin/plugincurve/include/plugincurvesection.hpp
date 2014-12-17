#ifndef PLUGINCURVESECTION_HPP
#define PLUGINCURVESECTION_HPP

#include <QGraphicsItem>
#include "plugincurvepoint.hpp"
#include "plugincurvesection.hpp"
#include "plugincurveview.hpp"

class PluginCurveSection : public QGraphicsObject
{
 Q_OBJECT

   /*!
   *  This class is a representation of a curve section of a PluginCurve.
   *  Permits to paint a PluginCurveSection. Contains pointers to destination and source points. @n
   *
   *  @brief Section of a PluginCurve
   *  @author Simon Touchard, Myriam Desainte-Catherine
   *  @date 2014
   */

//Attributes
protected:
    static const int SHAPEHEIGHT = 5; /*!< Defines the height of the setion's shape. */
    PluginCurvePoint *_pSourcePoint; /*!< Pointer of the source point (left point). */
    PluginCurvePoint *_pDestPoint; /*!< Pointer of the destination point (right point). */
    bool _highlight{}; /*!< Indicates if the points is highlighted. True : highlighted. False : Not Highlighted */
    QColor _color; /*!< Section's color. */
    QColor _selectColor; /*!< Section's color when selected */
    qreal _coef; /*!< Bending coefficient */
// Signals and slots
signals:
    void doubleClicked(QGraphicsSceneMouseEvent *event); /*!< Notifies that the user double clicked the section. */
    void rightClicked(PluginCurveSection *section, QPointF scenePos); /*!< Notifies that the user right clicked the button. */
public slots:
    void setAllFlags(bool b); /*! Changes the flags.*/

//Methods

public:
    //! Constructs a PluginCurveSection. The source and destination points must be modified after the instatiation of the section consequently.
       /*!
         \param parent Section's parent.
         \param source The left point. It musn't be null.
         \param dest The right point. It musn't be null.
       */
    PluginCurveSection(QGraphicsObject *parent, PluginCurvePoint *source, PluginCurvePoint *dest); // Parent : PluginCurvePoint ? GraphicsCurveView ?
    ~PluginCurveSection();
    /*! Returns the source point. The function transform().map() should be used for obtain the point posistion in paint coordinates. */
    PluginCurvePoint *sourcePoint();
    /*! Returns the destination point. The function transform().map() should be used for obtain the point posistion in paint coordinates. */
    PluginCurvePoint *destPoint();
    /*! Modifies the source point and adjust the section. The point must be modified consequently. */
    void setSourcePoint(PluginCurvePoint *autoPoint);
    /*! Modifies the destination point and adjust the destination. The point must be modified consequently. */
    void setDestPoint(PluginCurvePoint *autoPoint);
    /*! Returns the bending coefficient. */
    qreal bendingCoef();
    /*! Sets the bending coefficient. */
    void setBendingCoef(qreal coef);
    /*! Highlights the section. */
    void highlight(bool b);
    /*! Returns the point's color. */
    QColor color();
    /*! Returns the point's color when selected. */
    QColor selectColor();
    /*! Adjusts the curve position */
    void adjust();
    /*! Returns the bounding rectangle. */
    virtual QRectF boundingRect() const = 0;
    /*! Returns the section's shape. */
    virtual QPainterPath shape() const = 0;
    /*! Returns the section's path. */
    virtual QPainterPath path() const = 0;
    /*! Paints the curve. */
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) = 0;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);



};

#endif // PLUGINCURVESECTION_HPP
