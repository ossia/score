#pragma once
#include <iscore/tools/ModelPath.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <State/Address.hpp>

/** Note : this command is for internal use only, in recording **/

class AutomationModel;
class InitAutomation : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AutomationControl", "InitAutomation", "InitAutomation")
    public:
             ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(InitAutomation)
        InitAutomation(
                Path<AutomationModel>&& path,
                const iscore::Address& newaddr,
                double newmin,
                double newmax,
                const QVector<QByteArray>& segments);

    public:
        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream &) const override;
        void deserializeImpl(QDataStream &) override;

    private:
        Path<AutomationModel> m_path;
        iscore::Address m_addr;
        double m_newMin;
        double m_newMax;
        QVector<QByteArray> m_segments;
};
