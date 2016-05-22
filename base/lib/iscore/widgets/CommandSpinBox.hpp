#pragma once
#include <QObject>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/widgets/SignalUtils.hpp>
namespace iscore
{
/**
 * @brief CommandSpinbox Will update a value of the model according to the spinbox.
 */
template<typename Property, typename Command, typename SpinBox>
struct CommandSpinbox : public QObject
{
        using model_type = typename Property::model_type;
        CommandSpinbox(
                const model_type& model,
                const iscore::CommandStackFacade& stck,
                QWidget* parent):
            m_sb{new SpinBox{parent}},
            m_slotDisp{stck}
        {
            m_sb->setMinimum(20);
            m_sb->setMaximum(20000);

            connect(m_sb, SignalUtils::SpinBox_valueChanged<SpinBox>(),
                    this, [&] (int h) {
                if(h != (model.*Property::get())())
                {
                    m_slotDisp.submitCommand(model, h);
                }
            });
            connect(m_sb, &SpinBox::editingFinished,
                    this, [=] { m_slotDisp.commit(); });

            con(model, Property::notify(),
                this, [=] (auto val) {
                if(val != m_sb->value())
                {
                    m_sb->setValue(val);
                }
            });

            m_sb->setValue((model.*Property::get())());
        }

        auto widget() const
        { return m_sb; }

    private:
        SpinBox* m_sb{};
        SingleOngoingCommandDispatcher<Command> m_slotDisp;
};

}
