#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
class CurveModel;
class CurveSegmentModel;

class UpdateCurve : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AutomationControl", "UpdateCurve", "UpdateCurve")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateCurve)
        UpdateCurve(
          Path<CurveModel>&& model,
          QVector<QByteArray>&& segments);

        void undo() override;
        void redo() override;

        void update(Path<CurveModel>&& model, QVector<QByteArray>&& segments);

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        Path<CurveModel> m_model;
        QVector<QByteArray> m_oldCurveData;
        QVector<QByteArray> m_newCurveData;
};

// TODO move me
#include <Curve/Segment/CurveSegmentData.hpp>
class UpdateCurveFast : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AutomationControl", "UpdateCurveFast", "UpdateCurveFast")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateCurveFast)
        UpdateCurveFast(
          Path<CurveModel>&& model,
          std::vector<CurveSegmentData>&& segments);

        void undo() override;
        void redo() override;

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
