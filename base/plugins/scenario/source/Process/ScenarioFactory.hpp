#pragma once
#include "ProcessInterface/ProcessFactoryInterface.hpp"

class ScenarioFactory : public ProcessFactoryInterface
{
    public:
        virtual QString name() const override;
        virtual QStringList availableViews() override;
        virtual ProcessViewInterface* makeView(QString view, QObject* parent) override;
        virtual ProcessPresenterInterface* makePresenter(ProcessViewModelInterface*,
                ProcessViewInterface*,
                QObject* parent) override;

        virtual ProcessSharedModelInterface* makeModel(id_type<ProcessSharedModelInterface> id,
                QObject* parent) override;

        virtual ProcessSharedModelInterface* makeModel(SerializationIdentifier identifier,
                void* data,
                QObject* parent) override;
};
