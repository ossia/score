#pragma once

#include <iscore/command/Command.hpp>
#include <QApplication>

class MasterPortChangedCommand : public iscore::Command
{
        // QUndoCommand interface
    public:
        /*
        MasterPortChangedCommand(int oldval, int newval) :
            Command {QString(""),
                    QString(""),
                    QString("")
        },
        m_oldval {oldval},
        m_newval {newval}
        {

        }
        */

        /*
        bool mergeWith(const Command* other) override
        {
            auto cmd = static_cast<const MasterPortChangedCommand*>(other);
            m_newval = cmd->m_newval;
            return true;
        }
        */

        void undo() const override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setMasterPort(m_oldval);
        }

        void redo() const override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setMasterPort(m_newval);
        }

    private:
        int m_oldval;
        int m_newval;
};
