#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include "DimensionModel.hpp"
#include <QMap>
#include <QString>
#include <QPointF>

class ViewportModel : public IdentifiedObject<ViewportModel>
{
        Q_OBJECT
    public:
        ViewportModel(const id_type<ViewportModel>& id, QObject* parent):
            IdentifiedObject{id, staticMetaObject.className(), parent}
        {

        }

        // Should be part of the presenter?
        double zoomLevel{};
        double rotation{}; // Degrees ? Radians ?
        QPointF pos{}; // Top left point

        // Map from a dimension in space to a dimension in the GUI
        id_type<DimensionModel> x_dim;
        id_type<DimensionModel> y_dim;

        // Map from a dimension in space to a default value.
        QMap<id_type<DimensionModel>, double> defaultValuesMap;
};
