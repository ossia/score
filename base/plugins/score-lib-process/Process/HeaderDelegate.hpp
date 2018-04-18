#pragma once
#include <Process/LayerPresenter.hpp>
namespace Process
{
class HeaderDelegate
    : public QObject
    , public Process::GraphicsShapeItem
{
  public:
    HeaderDelegate(Process::LayerPresenter& p)
      : presenter{&p}
    {

    }
    ~HeaderDelegate() override;

    enum class Shape { MiniShape, MaxiShape };
    virtual Shape headerShape(double w) const = 0;

    QPointer<Process::LayerPresenter> presenter;
};

}
