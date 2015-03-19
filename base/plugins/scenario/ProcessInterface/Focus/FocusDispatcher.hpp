#pragma once
#include <QObject>
class ProcessViewModelInterface;
namespace iscore
{
    class Document;
}
class BaseElementModel;

// Sets the focus on a scenario document.
class FocusDispatcher : public QObject
{
        Q_OBJECT
    public:
        FocusDispatcher(iscore::Document& doc);

    signals:
        void focus(ProcessViewModelInterface* obj);

    private:
        BaseElementModel& m_baseElementModel;
};
