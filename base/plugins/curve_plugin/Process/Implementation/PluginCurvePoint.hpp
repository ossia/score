#ifndef PLUGINCURVEPOINT_HPP
#define PLUGINCURVEPOINT_HPP

#include <QGraphicsWidget>
#include <QPointF>
#include <QList>
class PluginCurveSection;
class PluginCurveView;
class PluginCurvePresenter;

//! Mobility mode enum.
/*! Enum the differents point's behavior while moved*/
/// @todo Add Mobility mode : horizontal, Fixed
enum MobilityMode { Normal, Vertical }; // Point movement restriction enumeration

/*!
 *  This class is a representation of a point of a PluginCurve.
 *  Permits to paint a PluginCurvePoint. Contain pointers of adjacents sections @n
 *
 *  @brief Point of a PluginCurve
 *  @author Simon Touchard, Myriam Desainte-Catherine
 *  @date 2014
 */
class PluginCurvePoint : public QGraphicsObject
{
		Q_OBJECT

		//Attributs
	public:
		static const int RADIUS = 2; /*!< Point radius. */
		static const int SHAPERADIUS = 8; /*!< Point's shape radius. */
	protected:
		QColor _color; /*!< Point's color. */
		QColor _selectColor; /*!< Point's color when selected. */
	private:
		PluginCurvePresenter* _pPresenter; /*!< A pointer to the presenter. */
		QPointF _value; /*!< Point's value. */
		MobilityMode _mobility; /*!< Point movement restriction. */
		qreal _fixedCoordinate; /*!< Fixed coordinate if point's mobility is vertical. */
		bool _removable; /*!< Indicate if the point is removable. */
		PluginCurveSection* _pLeftSection = nullptr; /*!< Section at the left of the point. */
		PluginCurveSection* _pRightSection = nullptr;/*!< Seciton at the right of the point. */
		bool _highlight = false; /*!< Indicates if the points is hilighted. */

		// Signals
	signals:
		void pointPositionHasChanged(); /*!< Notifies that the points selected has been released. */
		void pointPositionIsChanging (PluginCurvePoint*); /*!< Notifies that the point is moving. */
		void rightClicked (PluginCurvePoint* point); /*!< Notifies that the user right clicked the point. */
		// Slots
	public slots :
		void setAllFlags (bool b);

		// Methods
	public:
		//! Constructs a PluginCurvePoint. The left section and right section are initially null.
		/*!
		  \param parent Point's parent.
		  \param presenter Pointer to the presenter.
		  \param point Point's initial position.
		  \param value Point's value.
		  \param mobility Point's mobility.
		  \param removable Indicates if the point can be removed.
		*/
		PluginCurvePoint (QGraphicsObject* parent, PluginCurvePresenter* presenter, QPointF point, QPointF value, MobilityMode mobility = Normal, bool removable = true);

		/*! Sets the point's value to value.
		*/
		void setValue (QPointF value);
		/*! Returns the point's value.
		*/
		QPointF getValue();
		/*! Returns point's mobility. A Normal point can be moved horizontaly and verticaly, a Vertical point can only be moved vertically.
		 * The fixed coordinate of the point is indicated by fixedCoordinate().
		\sa setMobility() , fixedCoordinate()
		*/
		MobilityMode mobility();
		/*! Sets the point's mobility to mode. If mode value is Vertical, the fixed coordinate is fixed to point's x actual value.
		\sa mobility() , fixedCoordinate()
		*/
		void setMobility (MobilityMode mode);
		/*! Returns de fixed coordinate. Used for restrict the mobility of a Vertical point.
		 \sa mobility() , setMobility()
		*/
		qreal fixedCoordinate();
		/*! Highlights the point. If b value is true, the point is hilihgted. In the other case, the point is print normaly. */
		void highlight (bool b);
		/*! Indicates if the point can be cancelled. A point can not be removed  if this function returns false.
		\sa setRemovable();
		*/
		bool removable();
		/*! Returns the point's color. */
		QColor color();
		/*! Returns the point's color when selected. */
		QColor selectColor();
		/*! Returns the point position in gloabl coordinates. */
		QPointF globalPos();
		/*! Set point's removability to.
		\sa removable()
		*/
		void setRemovable (bool b);
		/*! Returns the point's bounding rectangle. */
		QRectF boundingRect() const;
		/*! Returns the point's shape. */
		QPainterPath shape() const;
		/*! Paint the point. */
		void paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);
		/*! Modifies point's right section. The section must be modified consequently.
		\sa setLeftSection(), rightSection() , leftSection()
		*/
		void setRightSection (PluginCurveSection* section);
		/*! Modifies point's left section. The section must be modified consequently.
		\sa setRightSection(), leftSection() , rightSection()
		*/
		void setLeftSection (PluginCurveSection* section);
		/*! Returns point's right section.
		\sa setRightSection(), setLeftSection(), leftSection()
		*/
		PluginCurveSection* rightSection();
		/*! Return point's left section.
		\sa setLeftSection(), setRightSection(), rightSection()
		*/
		PluginCurveSection* leftSection();
		/*! Adjust the left and right section position. */
		void adjust();
		/*! Returns true if the point's x value is superior or egal to the other's one. */
		bool compareXSup (const QPointF& other) const;
		/*! Returns true if the point's x value is inferior or egal to the other's one. */
		bool compareXInf (const QPointF& other) const;
		/*! Returns true if the point's y value is superior or egal to the other's one. */
		bool compareYSup (const QPointF& other) const;
		/*! Returns true if the point's y value is inferior or egal to the other's one. */
		bool compareYInf (const QPointF& other) const;
		/*! Returns true if the point's x value is superior or egal to the other's one. Equivalent to compareXSup */
		bool operator>= (const QPointF& other) const;
		/*! Returns true if the point's x value is superior or egal to the other's one. Equivalent to compareXInf */
		bool operator<= (const QPointF& other) const;

	protected:
		void mousePressEvent (QGraphicsSceneMouseEvent* event);
		void mouseReleaseEvent (QGraphicsSceneMouseEvent* event);
		void mouseMoveEvent (QGraphicsSceneMouseEvent* event);
		void keyPressEvent (QKeyEvent* event);
		void hoverEnterEvent (QGraphicsSceneHoverEvent* event);
		void hoverLeaveEvent (QGraphicsSceneHoverEvent* event);
		QVariant itemChange (GraphicsItemChange change, const QVariant& value);


};

#endif // PLUGINCURVEPOINT_HPP
