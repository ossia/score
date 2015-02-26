#pragma once

#include <core/presenter/command/Command.hpp>
#include <QApplication>

class MasterPortChangedCommand : public iscore::Command
{
        // QUndoCommand interface
    public:
        MasterPortChangedCommand(int oldval, int newval) :
            Command {QString(""),
                    QString(""),
                    QString("")
        },
        m_oldval {oldval},
        m_newval {newval}
        {

        }

        virtual bool mergeWith(const QUndoCommand* other) override
        {
            auto cmd = static_cast<const MasterPortChangedCommand*>(other);
            m_newval = cmd->m_newval;
            return true;
        }

        virtual void undo() override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setMasterPort(m_oldval);
        }

        virtual void redo() override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setMasterPort(m_newval);
        }

    private:
        int m_oldval;
        int m_newval;
};
