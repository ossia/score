#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Space/space.hpp>
#include <Space/bounded_symbol.hpp>
#include "DimensionModel.hpp"
#include "ViewportModel.hpp"
#include <src/SpaceContext.hpp>
// A space has a number of dimensions
class SpaceModel : public IdentifiedObject<SpaceModel>
{
        Q_OBJECT
    public:
        SpaceModel(
                const Id<SpaceModel>& id,
                QObject* parent);

        const auto& space() const
        { return *m_space; }

        void addDimension(DimensionModel* dim);
        void removeDimension(const QString& name);
        const DimensionModel& dimension(const Id<DimensionModel>& id) const;
        const DimensionModel& dimension(const QString& name) const;

        const auto& dimensions() const
        { return m_dimensions; }

        void addViewport(ViewportModel*);
        void removeViewport(const Id<ViewportModel>&);
        const auto& viewports() const
        { return m_viewports; }

        // Might be false if there is no viewport.
        const Id<ViewportModel>& defaultViewport() const
        { return m_defaultViewport; }

    signals:
        void dimensionAdded(const DimensionModel&);
        void viewportAdded(const ViewportModel&);
        void spaceChanged();

    private:
        void rebuildSpace();

        std::unique_ptr<spacelib::euclidean_space> m_space;
        IdContainer<DimensionModel> m_dimensions;
        IdContainer<ViewportModel> m_viewports;

        Id<ViewportModel> m_defaultViewport;
};
