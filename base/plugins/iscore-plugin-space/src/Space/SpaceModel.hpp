#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Space/space.hpp>

// A space has a number of dimensions
class SpaceModel : public IdentifiedObject<SpaceModel>
{
    public:
        SpaceModel(
                std::unique_ptr<spacelib::space>&& sp,
                const id_type<SpaceModel>& id,
                QObject* parent);

        const auto& space() const
        { return *m_space; }

    private:
        std::unique_ptr<spacelib::space> m_space;
};
