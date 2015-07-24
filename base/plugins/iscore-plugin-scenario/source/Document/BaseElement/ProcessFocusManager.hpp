#pragma once
#include <ProcessInterface/ProcessModel.hpp>
#include <ProcessInterface/LayerModel.hpp>
#include <ProcessInterface/LayerPresenter.hpp>
#include <QPointer>

// Keeps the focused elements in memory for use by the scenario control.
// Note : focus should not be lost when switching documents. Hence, this
// should more be part of the per-document part.
class ProcessFocusManager : public QObject
{
        Q_OBJECT

    public:
        const Process* focusedModel();
        const LayerModel* focusedViewModel();
        LayerPresenter* focusedPresenter();

    public slots:
        void setFocusedPresenter(LayerPresenter*);

        void focusNothing();

    signals:
        void sig_focusedPresenter(LayerPresenter*);
        void sig_defocusedPresenter(LayerPresenter*);

        void sig_defocusedViewModel(const LayerModel*);
        void sig_focusedViewModel(const LayerModel*);

    private:
        void focusPresenter(LayerPresenter*);
        void defocusPresenter(LayerPresenter*);
        QPointer<const Process> m_currentModel{};
        QPointer<const LayerModel> m_currentViewModel{};
        QPointer<LayerPresenter> m_currentPresenter{};
};
