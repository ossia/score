#pragma once
#include <QDialog>
#include <score/plugins/customfactory/UuidKey.hpp>
class QListWidget;

namespace Process
{
class ProcessFactoryList;
class StateProcessList;
class ProcessModelFactory;
class LayerFactory;
class StateProcessFactory;
class ProcessModel;
}
namespace Scenario
{

class AddProcessDialog final : public QDialog
{
  Q_OBJECT

public:
  AddProcessDialog(const Process::ProcessFactoryList& plist, QWidget* parent);

  void launchWindow();

signals:
  void okPressed(UuidKey<Process::ProcessModel>);

private:
  const Process::ProcessFactoryList& m_factoryList;
  QListWidget* m_categories{};
  QListWidget* m_processes{};
  void updateProcesses(const QString& str);
  void setup();
};

class AddStateProcessDialog final : public QWidget
{
  Q_OBJECT

public:
  AddStateProcessDialog(
      const Process::StateProcessList& plist, QWidget* parent);

  void launchWindow();

signals:
  void okPressed(const UuidKey<Process::StateProcessFactory>&);

private:
  const Process::StateProcessList& m_factoryList;
};
}
