#pragma once
#include <QDialog>
#include <Process/ProcessList.hpp>
class QListWidget;
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
  ~AddProcessDialog();

  void launchWindow();

  std::function<void(Key, QString)> on_okPressed;

private:
  void updateProcesses(const QString& str);
  void setup();

  const Process::ProcessFactoryList& m_factoryList;
  QListWidget* m_categories{};
  QListWidget* m_processes{};
  Process::ProcessFlags m_flags{};
};

}
