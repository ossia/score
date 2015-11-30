#pragma once
#include <QComboBox>
#include <QString>
#include <QStringList>
#include <QWidget>

class LambdaFriendlyQComboBox final : public QWidget
{
        Q_OBJECT
    public:
        LambdaFriendlyQComboBox(QWidget* parent);

        void clear()
        { m_combobox.clear(); }

        void setCurrentIndex(int i)
        { m_combobox.setCurrentIndex(i); }

        void addItem(const QString& item)
        { m_combobox.addItem(item);
          m_elements.append(item);}

        int count() const
        { return m_combobox.count(); }

        QString currentText() const
        { return m_combobox.currentText() ;}

        QStringList elements() const
        { return m_elements; }

    signals:
        void activated(QString);

    private:
        QComboBox m_combobox;
        QStringList m_elements;
};
