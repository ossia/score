#pragma once

class QComboBox;
class QGridLayout;
class QLineEdit;
class AddressSettingsWidget;

#include <QDialog>
#include <QList>
#include <QString>


class AddressEditDialog : public QDialog
{
  Q_OBJECT

public:

  AddressEditDialog(QWidget *parent);
  ~AddressEditDialog();

  //TODO: use QVariant ???
  /*
    first element is node name, second element is node value type.
  */
  QList<QString> getSettings() const;

  void setSettings(QList<QString> &settings);

protected slots:

  void updateNodeWidget();

protected:
  
  void buildGUI();

  void initAvailableValueTypes();

protected:
  QLineEdit *m_nameEdit;
  QComboBox *m_valueTypeCBox;
  AddressSettingsWidget *m_addressWidget;
  QGridLayout *m_gLayout;
  QList<QList<QString> > m_previousSettings;
  int m_index;
};
  
  
