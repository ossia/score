#pragma once
#include "ProcessInterface/ProcessFactoryInterface.hpp"

class ScenarioFactory : public ProcessFactoryInterface
{
    public:
        virtual QString name() const override;

        virtual ProcessSharedModelInterface* makeModel(
                TimeValue duration,
                id_type<ProcessSharedModelInterface> id,
                QObject* parent) override;

        virtual ProcessSharedModelInterface* makeModel(
                const VisitorVariant&,
                QObject* parent) override;

        virtual ProcessPresenterInterface* makePresenter(
                ProcessViewModelInterface*,
                ProcessViewInterface*,
                QObject* parent) override;

        virtual ProcessViewInterface* makeView(
                ProcessViewModelInterface* viewmodel,
                QObject* parent) override;

};
