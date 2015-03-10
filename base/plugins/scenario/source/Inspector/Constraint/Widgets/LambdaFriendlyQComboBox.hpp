#pragma once
#include <QComboBox>
class LambdaFriendlyQComboBox : public QWidget
{
        Q_OBJECT
    public:
        LambdaFriendlyQComboBox(QWidget* parent);

        void clear()
        { m_combobox.clear(); }

        void setCurrentIndex(int i)
        { m_combobox.setCurrentIndex(i); }

        void addItem(const QString& item)
        { m_combobox.addItem(item); }

        int count() const
        { return m_combobox.count(); }

        QString currentText() const
        { return m_combobox.currentText() ;}

    signals:
        void activated(QString);

    private:
        QComboBox m_combobox;
};
