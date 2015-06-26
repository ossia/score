#pragma once
#include <ProcessInterface/ProcessModel.hpp>
#include <ProcessInterface/LayerModel.hpp>
#include <ProcessInterface/ProcessPresenter.hpp>
#include <QPointer>

// Keeps the focused elements in memory for use by the scenario control.
// Note : focus should not be lost when switching documents. Hence, this
// should more be part of the per-document part.
class ProcessFocusManager : public QObject
{
        Q_OBJECT

    public:
        const ProcessModel* focusedModel();
        const LayerModel* focusedViewModel();
        ProcessPresenter* focusedPresenter();

    public slots:
        void setFocusedPresenter(ProcessPresenter*);

        void focusNothing();

    signals:
        void sig_focusedPresenter(ProcessPresenter*);
        void sig_defocusedPresenter(ProcessPresenter*);

        void sig_defocusedViewModel(const LayerModel*);
        void sig_focusedViewModel(const LayerModel*);

    private:
        void focusPresenter(ProcessPresenter*);
        void defocusPresenter(ProcessPresenter*);
        QPointer<const ProcessModel> m_currentModel{};
        QPointer<const LayerModel> m_currentViewModel{};
        QPointer<ProcessPresenter> m_currentPresenter{};
};
