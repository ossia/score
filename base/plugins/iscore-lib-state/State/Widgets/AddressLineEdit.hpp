#pragma once
#include <QLineEdit>
#include <QDropEvent>
#include <QValidator>
#include <State/Widgets/AddressValidator.hpp>
#include <State/MessageListSerialization.hpp>

#include "AddressValidator.hpp"

namespace State
{
/**
 * @brief The AddressLineEdit class
 *
 * Used to input an address. Changes colors to red-ish if it is invalid.
 */
template<class Validator_T>
class ISCORE_LIB_STATE_EXPORT AddressLineEditBase :
        public QLineEdit
{
    public:
        explicit AddressLineEditBase(QWidget* parent):
            QLineEdit{parent}
        {
            setAcceptDrops(true);
            connect(this, &QLineEdit::textChanged,
                    this, [&] (const QString& str) {
                QString s = str;
                int i = 0;
                if(m_validator.validate(s, i) == QValidator::State::Acceptable)
                {
                    this->setStyleSheet("QLineEdit { background: black; }");
                }
                else
                {
                    this->setStyleSheet("QLineEdit { background: #660000; }");
                }
            });
        }

    private:
        void dragEnterEvent(QDragEnterEvent *event) override
        {
            if(event->mimeData()->formats().contains(iscore::mime::messagelist()))
            {
                event->accept();
            }
        }

        void dropEvent(QDropEvent* ev) override
        {
            auto mime = ev->mimeData();

            if(mime->formats().contains(iscore::mime::messagelist()))
            {
                Mime<State::MessageList>::Deserializer des{*mime};
                State::MessageList ml = des.deserialize();
                if(ml.size() > 0)
                {
                    this->setText(ml[0].address.toString());
                }
            }
        }
        Validator_T m_validator;
};

class ISCORE_LIB_STATE_EXPORT AddressLineEdit :
        public AddressLineEditBase<AddressValidator>
{
        using AddressLineEditBase<AddressValidator>::AddressLineEditBase;
};
class ISCORE_LIB_STATE_EXPORT AddressAccessorLineEdit :
        public AddressLineEditBase<AddressAccessorValidator>
{
        using AddressLineEditBase<AddressAccessorValidator>::AddressLineEditBase;
};

class ISCORE_LIB_STATE_EXPORT AddressFragmentLineEdit :
        public QLineEdit
{
    public:
        AddressFragmentLineEdit(QWidget* parent):
            QLineEdit{parent}
        {
            setValidator(new AddressFragmentValidator{this});
        }
};
}
