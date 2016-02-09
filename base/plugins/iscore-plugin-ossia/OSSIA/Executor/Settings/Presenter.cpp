#include "Presenter.hpp"
#include "Model.hpp"
#include "View.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <QApplication>
#include <QStyle>
#include <iscore/command/PropertyCommand.hpp>

class SettingsCommandDispatcher
{
        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            auto it = commands.find(TheCommand::static_key());
            if(it != commands.end())
            {
                static_cast<TheCommand&>(*it->second).update(std::forward<Args>(args)...);
            }
            else
            {
                it = commands.emplace(TheCommand::static_key(), args...);
            }
            it.second->redo();
        }

        void commit()
        {
            commands.clear();
        }

        void rollback()
        {
            for(auto& it : commands)
            {
                it.second->undo();
            }
            commands.clear();
        }

    private:
        std::map<CommandFactoryKey, std::unique_ptr<iscore::SerializableCommand>> commands;
};


template<
        typename SettingsModel_T,
        typename Param_T,
        Param_T Property_M>
class ISCORE_LIB_BASE_EXPORT SettingsCommand : public iscore::Command
{
    public:
        using Command::Command;
        SettingsCommand() = default;

        SettingsCommand(SettingsModel_T& obj,
                        const QVariant& newval):
            m_model{obj},
            m_property{*Property_M},
            m_new{newval}
        {
            m_old = m_model.property(m_property);
        }

        virtual ~SettingsCommand();

        void undo() const final override
        {
            m_model.setProperty(m_property, m_old);
        }

        void redo() const final override
        {
            m_model.setProperty(m_property, m_new);
        }

        void update(
                const SettingsModel_T&,
                const QVariant& newval)
        {
            m_new = newval;
        }

    private:
        SettingsModel_T m_model;
        QString m_property;
        QVariant m_old, m_new;
};


class SetRate : public SettingsCommand<
        RecreateOnPlay::Settings::Model,
        decltype(&RecreateOnPlay::Settings::Keys::rate_property),
        &RecreateOnPlay::Settings::Keys::rate_property>
{

};


namespace RecreateOnPlay
{
namespace Settings
{

Presenter::Presenter(
        Model& model,
        View& view,
        QObject *parent):
    iscore::SettingsDelegatePresenterInterface{model, view, parent}
{
    con(view, &View::rateChanged,
        this, &Presenter::on_rateChanged);

}

void Presenter::on_rateChanged(int rate)
{
    model(this).setRate(rate);
}

void Presenter::on_accept()
{
}

void Presenter::on_reject()
{
}

QString Presenter::settingsName()
{
    return tr("Execution");
}

QIcon Presenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
}


}
}
