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


#include <Curve/Segment/CurveSegmentData.hpp>
class UpdateCurveFast : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AutomationControl", "UpdateCurveFast", "UpdateCurveFast")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateCurveFast)
        UpdateCurveFast(
          Path<CurveModel>&& model,
          QVector<CurveSegmentData>&& segments);

        void undo() override;
        void redo() override;

        void update(const Path<CurveModel>&,
                    QVector<CurveSegmentData>&&);

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        Path<CurveModel> m_model;
        QVector<CurveSegmentData> m_oldCurveData;
        QVector<CurveSegmentData> m_newCurveData;
};
