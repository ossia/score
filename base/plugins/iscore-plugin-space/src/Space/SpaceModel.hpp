#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Space/space.hpp>
#include <Space/bounded_symbol.hpp>
#include "DimensionModel.hpp"
#include <QPointF>

class ViewportModel : public IdentifiedObject<ViewportModel>
{
        Q_OBJECT
    public:
        ViewportModel(const id_type<ViewportModel>& id, QObject* parent):
            IdentifiedObject{id, staticMetaObject.className(), parent}
        {

        }

        double zoomLevel{};
        // todo ? orientation;
        QPointF pos{};

        // Map from a dimension in space to a dimension in the GUI
        QMap<QString, QString> dimensionsMap;

        // Map form a dimension in space to a default value.
        QMap<QString, double> defaultValuesMap;
};

// A space has a number of dimensions
class SpaceModel : public IdentifiedObject<SpaceModel>
{
        Q_OBJECT
    public:
        SpaceModel(
                std::vector<DimensionModel>&& sp,
                const id_type<SpaceModel>& id,
                QObject* parent);

        const auto& space() const
        { return *m_space; }

        void addDimension(const DimensionModel& dim);
        void removeDimension(const QString& name);
        const DimensionModel& dimension(const QString& name) const;

        const auto& dimensions() const
        { return m_dimensions; }

        void addViewport(ViewportModel*);
        void removeViewport(const id_type<ViewportModel>&);
        const auto& viewports() const
        { return m_viewports; }

    signals:
        void spaceChanged();

    private:
        void rebuildSpace();

        std::unique_ptr<spacelib::euclidean_space> m_space;
        std::vector<DimensionModel> m_dimensions;
        IdContainer<ViewportModel> m_viewports;
};
