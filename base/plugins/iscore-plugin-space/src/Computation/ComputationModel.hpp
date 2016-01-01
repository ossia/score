#pragma once
#include <memory>
#include <Space/computation.hpp>
#include <Process/ModelMetadata.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

namespace Space
{
class SpaceModel;
// Maps addresses / values to the parameter of an Computation
class ComputationModel : public IdentifiedObject<ComputationModel>
{
        Q_OBJECT
    public:
        ModelMetadata metadata;

        using Computation = std::function<double()>;
        ComputationModel(
                const Computation& comp,
                const SpaceModel& space,
                const Id<ComputationModel>&,
                QObject* parent);


        //void setComputation(std::unique_ptr<spacelib::computation> &&ar);

        const auto& computation() const
        { return m_fun; }


        const auto& space() const
        { return m_space; }

    private:
        Computation m_fun;
        const SpaceModel& m_space;
        //std::unique_ptr<spacelib::computation> m_computation;

};
}
