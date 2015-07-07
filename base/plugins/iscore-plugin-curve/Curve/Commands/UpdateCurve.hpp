#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class CurveSegmentModel;
// TODO due to AutomationControl this can't be put in a lib...
class UpdateCurve : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("UpdateCurve", "UpdateCurve")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateCurve, "AutomationControl")
        UpdateCurve(ObjectPath&& model, QVector<QByteArray>&& segments);

        void undo() override;
        void redo() override;

        void update(ObjectPath&& model, QVector<QByteArray>&&  segments);

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_model;
        QVector<QByteArray> m_oldCurveData;
        QVector<QByteArray> m_newCurveData;
};
