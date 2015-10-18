#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
class CurveModel;
class CurveSegmentModel;

class UpdateCurve : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL("AutomationControl", UpdateCurve, "UpdateCurve")
    public:
        UpdateCurve(
          Path<CurveModel>&& model,
          std::vector<CurveSegmentData>&& segments);

        void undo() const override;
        void redo() const override;

        void update(const Path<CurveModel>&,
                    std::vector<CurveSegmentData>&&);

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        Path<CurveModel> m_model;
        std::vector<CurveSegmentData> m_oldCurveData;
        std::vector<CurveSegmentData> m_newCurveData;
};
