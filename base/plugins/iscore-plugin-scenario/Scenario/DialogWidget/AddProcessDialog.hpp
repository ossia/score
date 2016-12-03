#pragma once
#include <QWidget>
#include <iscore/plugins/customfactory/UuidKey.hpp>

namespace Process
{
class ProcessFactoryList;
class StateProcessList;
class ProcessModelFactory;
class LayerFactory;
class StateProcessFactory;
}
namespace Scenario
{

class AddProcessDialog final : public QWidget
{
  Q_OBJECT

public:
  AddProcessDialog(const Process::ProcessFactoryList& plist, QWidget* parent);

  void launchWindow();

signals:
  void okPressed(UuidKey<Process::ProcessModelFactory>);

private:
  const Process::ProcessFactoryList& m_factoryList;
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
