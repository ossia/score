#include "DeviceEditDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>

#include "NodeFactory.hpp"
#include "ProtocolSettingsWidget.hpp"


DeviceEditDialog::DeviceEditDialog(QWidget *parent)
  : QDialog(parent),
    m_protocolWidget(nullptr), m_index(-1)
{
  setModal(true);
  buildGUI();
}

DeviceEditDialog::~DeviceEditDialog()
{

}

void
DeviceEditDialog::buildGUI()
{
  QLabel *protocolLabel = new QLabel(tr("Protocol"), this);
  m_protocolCBox = new QComboBox(this);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok 
						     | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject())); 


  m_gLayout = new QGridLayout;
  
  m_gLayout->addWidget(protocolLabel, 0, 0, 1, 1);
  m_gLayout->addWidget(m_protocolCBox, 0, 1, 1, 1);
  //keep one row for m_protocolWidget
  m_gLayout->addWidget(buttonBox, 2, 0, 1, 2);
  
  setLayout(m_gLayout);

  initAvailableProtocols(); //populate m_protocolCBox
     
  connect(m_protocolCBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateProtocolWidget()));

  if (m_protocolCBox->count() > 0) {
    Q_ASSERT(m_protocolCBox->currentIndex() == 0);
    updateProtocolWidget();
  }
  
  //TODO ???
  //connect(this, SIGNAL(accepted()), &updater, SLOT(update()));

  


}

void
DeviceEditDialog::initAvailableProtocols()
{
  Q_ASSERT(m_protocolCBox);

  m_protocolCBox->addItems(NodeFactory::instance().getAvailableProtocols());
  
  //initialize previous settings 
  m_previousSettings.clear();
  for (int i=0; i<m_protocolCBox->count(); ++i)
    m_previousSettings.append(QList<QString>());

  m_index = m_protocolCBox->currentIndex();
}



void
DeviceEditDialog::updateProtocolWidget()
{
  Q_ASSERT(m_protocolCBox);

  if (m_protocolWidget) {
    Q_ASSERT(m_index < m_protocolCBox->count());
    Q_ASSERT(m_index < m_previousSettings.count());

    m_previousSettings[m_index] = m_protocolWidget->getSettings();
    delete m_protocolWidget;
  }
  
  m_index = m_protocolCBox->currentIndex();

  //TODO: we should access a factory from MainWindow ? Model ???  

  const QString protocol = m_protocolCBox->currentText();
  m_protocolWidget = NodeFactory::instance().getProtocolWidget(protocol);
  if (m_protocolWidget) {
    if (! m_previousSettings.at(m_index).empty())
      m_protocolWidget->setSettings(m_previousSettings.at(m_index));
    
    m_gLayout->addWidget(m_protocolWidget, 1, 0, 1, 2);
    updateGeometry();
  }

}

QList<QString>
DeviceEditDialog::getSettings() const
{
  QList<QString> settings;
  if (m_protocolWidget)
    settings = m_protocolWidget->getSettings();
  settings.insert(0, m_protocolCBox->currentText()); //protocol as first element
  return settings;
}

void
DeviceEditDialog::setSettings(QList<QString> &settings)
{
  Q_ASSERT(settings.size() >= 1);

  const QString protocol = settings.at(0);
  const int index = m_protocolCBox->findText(protocol);
  Q_ASSERT(index != -1);
  Q_ASSERT(index < m_protocolCBox->count());
  
  m_protocolCBox->setCurrentIndex(index); //will emit currentIndexChanged(int) & call slot
}
