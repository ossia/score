#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class SpaceProcess;
// The widget that opens when a space process is clicked
class SpaceGuiWindow : public QWidget
{
        Q_OBJECT
    public:
        SpaceGuiWindow(iscore::CommandStackFacade &stack, const SpaceProcess &space, QWidget* parent);


    private:
};
