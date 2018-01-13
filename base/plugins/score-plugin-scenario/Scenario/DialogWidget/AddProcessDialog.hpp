#pragma once
#include <QDialog>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <utility>
#include <vector>
#include <set>
#include <QListWidget>

#include <score/plugins/customfactory/UuidKey.hpp>

#include <Process/ProcessList.hpp>
namespace Scenario
{
class AddProcessDialog final : public QDialog
{
public:
    using Key = typename Process::ProcessFactoryList::key_type;

  AddProcessDialog(
      const Process::ProcessFactoryList& plist,
      Process::ProcessFlags acceptable,
      QWidget* parent);

  void launchWindow();

  std::function<void(Key, QString)> on_okPressed;

private:
  const Process::ProcessFactoryList& m_factoryList;
  Process::ProcessFlags m_flags{};
  QListWidget* m_categories{};
  QListWidget* m_processes{};

  void updateProcesses(const QString& str);
  void setup();
};

}
