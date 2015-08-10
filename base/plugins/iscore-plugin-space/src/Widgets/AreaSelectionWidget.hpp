#pragma once
#include <QWidget>
class QComboBox;
class QLineEdit;

class AreaSelectionWidget : public QWidget
{
        Q_OBJECT
    public:
        AreaSelectionWidget(QWidget* parent);

        QComboBox* comboBox() const { return m_comboBox; }
        QLineEdit* lineEdit() const { return m_lineEdit; }

    signals:
        void lineEditChanged();

    private:
        QComboBox* m_comboBox{};
        QLineEdit* m_lineEdit{};
};
