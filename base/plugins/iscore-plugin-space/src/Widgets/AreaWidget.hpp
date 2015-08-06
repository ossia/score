#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class AreaModel;
class SpaceProcess;
class AreaWidget : public QWidget
{
    public:
        AreaWidget(iscore::CommandStack& stack, const SpaceProcess &space, QWidget* parent);

        // If null, will add a new area instead.
        void setActiveArea(const AreaModel *);

        void on_formulaChanged();
        void on_dimensionMapped(int);

        void validate();
        void cleanup();

    private:
        CommandDispatcher<> m_dispatcher;
        const SpaceProcess& m_space;
        const AreaModel* m_area{};

        QLineEdit* m_lineEdit{};
        QFormLayout* m_spaceMappingLayout{};
        QFormLayout* m_paramMappingLayout{};
};
