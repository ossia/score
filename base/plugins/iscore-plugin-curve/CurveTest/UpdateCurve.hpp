#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class CurveSegmentModel;
class UpdateCurve : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("UpdateCurve", "UpdateCurve")
    public:
        UpdateCurve(ObjectPath&& model, QVector<CurveSegmentModel*> segments);

        void undo() override;
        void redo() override;

        void update(ObjectPath&& model, QVector<CurveSegmentModel*> segments);

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_model;
        QVector<QByteArray> m_oldCurveData;
        QVector<QByteArray> m_newCurveData;
};
