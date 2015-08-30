#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class CurveModel;
class CurveSegmentModel;

class UpdateCurve : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("UpdateCurve", "UpdateCurve")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(UpdateCurve, "AutomationControl")
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
