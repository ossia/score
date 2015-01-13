#pragma once
#include <tools/NamedObject.hpp>

#include <QObject>

class TimeNodeView;
class TimeNodeModel;

class TimeNodePresenter :  public NamedObject
{
    Q_OBJECT
    public:
        explicit TimeNodePresenter(TimeNodeModel* model, TimeNodeView* view, QObject *parent);
        ~TimeNodePresenter();

    int id();
    TimeNodeModel* model();
    TimeNodeView* view();

    signals:

    public slots:

    private:
        TimeNodeModel* m_model{};
        TimeNodeView* m_view{};
};
