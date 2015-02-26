#include "PresenterInterface.hpp"
#include <QApplication>
#include <core/presenter/Presenter.hpp>

iscore::SerializableCommand*
iscore::IPresenter::instantiateUndoCommand(const QString& parent_name,
        const QString& name,
        const QByteArray& data)
{
    auto presenter = qApp->findChild<iscore::Presenter*> ("Presenter");
    return presenter->instantiateUndoCommand(parent_name,
            name,
            data);
}
