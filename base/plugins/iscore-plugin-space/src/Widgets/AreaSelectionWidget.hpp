#pragma once
#include <QWidget>
class QComboBox;
class QLineEdit;

namespace Space
{
class SingletonAreaFactoryList;

class AreaSelectionWidget : public QWidget
{
        Q_OBJECT
    public:
        AreaSelectionWidget(
                const SingletonAreaFactoryList& fact,
                QWidget* parent);

        QComboBox* comboBox() const { return m_comboBox; }
        QLineEdit* lineEdit() const { return m_lineEdit; }

    signals:
        void lineEditChanged();

    private:
        QComboBox* m_comboBox{};
        QLineEdit* m_lineEdit{};
};
}
