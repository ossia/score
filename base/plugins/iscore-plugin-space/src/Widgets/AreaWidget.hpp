#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace iscore{
class CommandStack;
}
class SpaceProcess;
class AreaWidget : public QWidget
{
    public:
        AreaWidget(iscore::CommandStack& stack, const SpaceProcess &space, QWidget* parent);

        void updateArea();

        void on_formulaChanged();

        void on_dimensionMapped(int);

        void validate();

        void cleanup();

    private:
        CommandDispatcher<> m_dispatcher;
        const SpaceProcess& m_space;

        QLineEdit* m_lineEdit{};
        QFormLayout* m_spaceMappingLayout{};
        QFormLayout* m_paramMappingLayout{};
};
