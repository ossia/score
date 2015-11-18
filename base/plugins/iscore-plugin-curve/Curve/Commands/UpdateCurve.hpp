#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
class CurveModel;
class CurveSegmentModel;

class UpdateCurve final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(
                CurveCommandFactoryName(),
                UpdateCurve,
                "UpdateCurve")
    public:
        UpdateCurve(
          Path<CurveModel>&& model,
          std::vector<CurveSegmentData>&& segments);

        void undo() const override;
        void redo() const override;

        void update(const Path<CurveModel>&,
                    std::vector<CurveSegmentData>&&);

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<CurveModel> m_model;
        std::vector<CurveSegmentData> m_oldCurveData;
        std::vector<CurveSegmentData> m_newCurveData;
};
