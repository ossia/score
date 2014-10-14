#include "HelloWorldProcess.h"


struct TestModel : public iscore::ProcessModel
{
    TestModel() :
        iscore::ProcessModel()
    {
        qDebug("The TestModel begins.");
    }

    virtual ~TestModel()
    {
        qDebug("The TestModel ends.");
    }
};

HelloWorldProcess::HelloWorldProcess():
    iscore::Process()
{
    qDebug("Successfully instantiated HelloWorldProcess");
}

QStringList HelloWorldProcess::availableViews()
{
    return {};
}

std::unique_ptr<iscore::ProcessView> HelloWorldProcess::makeView(QString view)
{
    return std::make_unique<iscore::ProcessView>();
}

std::unique_ptr<iscore::ProcessPresenter> HelloWorldProcess::makePresenter()
{
    return std::make_unique<iscore::ProcessPresenter>();
}

std::unique_ptr<iscore::ProcessModel> HelloWorldProcess::makeModel()
{
    return std::make_unique<TestModel>();
}
