#pragma once
#include <QString>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_process_export.h>

#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/Identifier.hpp>

class QObject;
namespace Process
{
class LayerModelPanelProxy;
class LayerFactory;
class ProcessModel;

/**
 * @brief The LayerModel class
 *
 * Interface to implement to make a layer.
 */
class ISCORE_LIB_PROCESS_EXPORT LayerModel
    : public IdentifiedObject<LayerModel>,
      public iscore::SerializableInterface<LayerFactory>
{
  Q_OBJECT
public:
  virtual ~LayerModel();
  ProcessModel& processModel() const;

protected:
  // TODO this argument order sucks
  LayerModel(
      const Id<LayerModel>& viewModelId,
      const QString& name,
      ProcessModel& sharedProcess,
      QObject* parent);

  template <typename Impl>
  LayerModel(
      Deserializer<Impl>& vis, ProcessModel& sharedProcess, QObject* parent)
      : IdentifiedObject{vis, parent}, m_sharedProcessModel{sharedProcess}
  {
    // Nothing else to load
  }

private:
  ProcessModel& m_sharedProcessModel;
};

template <typename Process_T>
class LayerModel_T final : public LayerModel
{
public:
  using process_type = Process_T;
  explicit LayerModel_T(
      ProcessModel& model, const Id<Process::LayerModel>& id, QObject* parent)
      : LayerModel{id, Metadata<ObjectKey_k, LayerModel_T>::get(), model,
                   parent}
  {
  }

  explicit LayerModel_T(
      const LayerModel_T& other,
      ProcessModel& model,
      const Id<Process::LayerModel>& id,
      QObject* parent)
      : LayerModel_T{model, id, parent}
  {
  }

  template <typename Impl>
  explicit LayerModel_T(
      Deserializer<Impl>& vis, ProcessModel& model, QObject* parent)
      : Process::LayerModel{vis, model, parent}
  {
    // Nothing to load
  }

  Process_T& processModel() const
  {
    return static_cast<Process_T&>(LayerModel::processModel());
  }

  key_type concreteKey() const final override
  {
    return Metadata<ConcreteKey_k, LayerModel_T>::get();
  }

protected:
  // Nothing to save
  void serialize_impl(const VisitorVariant&) const final override
  {
  }
};
}

/**
 * @brief model Returns the casted version of a shared model given a view
 * model.
 * @param viewModel A view model which has a directive "using model_type =
 * MySharedModelType;" in its class body
 *
 * @return a pointer of the correct type.
 */
template <typename T>
typename T::model_type* model(const T* viewModel)
{
  return static_cast<typename T::model_type*>(viewModel->processModel());
}

template <typename T>
typename T::model_type& model(const T& viewModel)
{
  return static_cast<typename T::model_type&>(viewModel.processModel());
}

DEFAULT_MODEL_METADATA(Process::LayerModel, "LayerModel")
