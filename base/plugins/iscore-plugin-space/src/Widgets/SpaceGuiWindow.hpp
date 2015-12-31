#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
namespace iscore {
    struct DocumentContext;
}
namespace Space
{
class ProcessModel;
// The widget that opens when a space process is clicked
class SpaceGuiWindow : public QWidget
{
        Q_OBJECT
    public:
        SpaceGuiWindow(
                const iscore::DocumentContext& ctx,
                const Space::ProcessModel &space,
                QWidget* parent);


    private:
};
}
