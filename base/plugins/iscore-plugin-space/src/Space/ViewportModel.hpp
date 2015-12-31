#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include "DimensionModel.hpp"
#include <QMap>
#include <QString>
#include <QPointF>

#include <QGraphicsView>

namespace Space
{
class ViewportModel : public IdentifiedObject<ViewportModel>
{
        Q_OBJECT
    public:
        ViewportModel(const Id<ViewportModel>& id, QObject* parent):
            IdentifiedObject{id, staticMetaObject.className(), parent}
        {

        }

        const Id<DimensionModel>& xDim() const;
        void setXDim(const Id<DimensionModel>& xDim);

        const Id<DimensionModel>& yDim() const;
        void setYDim(const Id<DimensionModel>& yDim);

        const QMap<Id<DimensionModel>, double>& defaultValuesMap() const;
        void setDefaultValuesMap(const QMap<Id<DimensionModel>, double>& defaultValuesMap);

        double zoomLevel() const;
        void setZoomLevel(double zoomLevel);

        double rotation() const;
        void setRotation(double rotation);

        const QPointF& pos() const;
        void setPos(const QPointF& pos);

        const QString& name() const;
        void setName(const QString& name);

    private:
        QString m_name;

        // Should be part of the presenter?
        double m_zoomLevel{}; // what is the reference ? 1.0 = 1 pixel ? think of retina, too.
        // Maybe use the zooming features of QGraphicsScene, view, etc.
        double m_rotation{}; // Degrees ? Radians ?
        QPointF m_pos; // Top left point

        // Map from a dimension in space to a dimension in the GUI
        Id<DimensionModel> m_xDim;
        Id<DimensionModel> m_yDim;

        // Map from a dimension in space to a default value.
        // e.g. : for (x, y, z), we set z = 0.
        QMap<Id<DimensionModel>, double> m_defaultValuesMap;
};
}
