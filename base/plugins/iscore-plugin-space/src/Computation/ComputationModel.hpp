#pragma once
#include <memory>
#include <Space/computation.hpp>

#include <iscore/tools/IdentifiedObject.hpp>

class SpaceModel;
// Maps addresses / values to the parameter of an Computation
class ComputationModel : public IdentifiedObject<ComputationModel>
{
        Q_OBJECT
    public:
        ComputationModel(
                std::unique_ptr<spacelib::computation>&& computation,
                const SpaceModel& space,
                const id_type<ComputationModel>&,
                QObject* parent);

        void setComputation(std::unique_ptr<spacelib::computation> &&ar);

        const auto& computation() const
        { return *m_computation; }
        const auto& space() const
        { return m_space; }

    private:
        const SpaceModel& m_space;
        std::unique_ptr<spacelib::computation> m_computation;

};
