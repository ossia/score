#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Space/space.hpp>
#include <Space/bounded_symbol.hpp>
#include "DimensionModel.hpp"

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

    signals:
        void spaceChanged();

    private:
        void rebuildSpace();

        std::unique_ptr<spacelib::euclidean_space> m_space;
        std::vector<DimensionModel> m_dimensions;
};
