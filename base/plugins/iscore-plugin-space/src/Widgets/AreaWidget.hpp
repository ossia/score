#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
namespace iscore
{ struct DocumentContext; }
class AreaModel;
namespace Space { class ProcessModel; }
class AreaSelectionWidget;
class AreaWidget : public QWidget
{
    public:
        AreaWidget(
                const iscore::DocumentContext& ctx,
                const Space::ProcessModel &space,
                QWidget* parent);

        // If null, will add a new area instead.
        void setActiveArea(const AreaModel *);

        void on_formulaChanged();
        void on_dimensionMapped(int);

        void validate();
        void cleanup();

    private:
        CommandDispatcher<> m_dispatcher;
        const Space::ProcessModel& m_space;
        const AreaModel* m_area{};

        AreaSelectionWidget* m_selectionWidget{};
        QFormLayout* m_spaceMappingLayout{};
        QFormLayout* m_paramMappingLayout{};
};
