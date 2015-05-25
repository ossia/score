#pragma once
#include <ProcessInterface/ProcessModel.hpp>
#include <ProcessInterface/ProcessViewModel.hpp>
#include <ProcessInterface/ProcessPresenter.hpp>
#include <QPointer>

// Keeps the focused elements in memory for use by the scenario control.
// TODO rename file

// Note : focus should not be lost when switching documents. Hence, this
// should more be part of the per-document part.
class ProcessFocusManager : public QObject
{
        Q_OBJECT

    public:
        const ProcessModel* focusedModel();
        const ProcessViewModel* focusedViewModel();
        const ProcessPresenter* focusedPresenter();

        void setFocusedModel(ProcessModel*);
        void setFocusedViewModel(ProcessViewModel*);
        void setFocusedPresenter(ProcessPresenter*);

        void focusNothing();

    signals:
        void sig_defocusedViewModel(const ProcessViewModel*);
        void sig_focusedViewModel(const ProcessViewModel*);

    private:
        QPointer<const ProcessModel> m_currentModel{};
        QPointer<const ProcessViewModel> m_currentViewModel{};
        QPointer<const ProcessPresenter> m_currentPresenter{};
};


template<typename T, typename Container, typename Property, typename Value>
T get(const Container& c, Property&& prop, Value&& val)
{
    return static_cast<T>(
                *std::find_if(
                    c.begin(),
                    c.end(),
                    [&] (const auto& ctrl){ return (ctrl->*prop)() == val; }));
}

//ProcessFocusManager& getFocusManager();
