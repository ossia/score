#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Space/space.hpp>
#include <Space/bounded_symbol.hpp>
#include "DimensionModel.hpp"
#include "ViewportModel.hpp"
// A space has a number of dimensions
class SpaceModel : public IdentifiedObject<SpaceModel>
{
        Q_OBJECT
    public:
        SpaceModel(
                const id_type<SpaceModel>& id,
                QObject* parent);

        const auto& space() const
        { return *m_space; }

        void addDimension(DimensionModel* dim);
        void removeDimension(const QString& name);
        const DimensionModel& dimension(const id_type<DimensionModel>& id) const;
        const DimensionModel& dimension(const QString& name) const;

        const auto& dimensions() const
        { return m_dimensions; }

        void addViewport(ViewportModel*);
        void removeViewport(const id_type<ViewportModel>&);
        const auto& viewports() const
        { return m_viewports; }

    signals:
        void dimensionAdded(const DimensionModel&);
        void viewportAdded(const ViewportModel&);
        void spaceChanged();

    private:
        void rebuildSpace();

        std::unique_ptr<spacelib::euclidean_space> m_space;
        IdContainer<DimensionModel> m_dimensions;
        IdContainer<ViewportModel> m_viewports;
};
