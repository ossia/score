#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>
#include <core/interface/selection/Selection.hpp>

class ProcessSharedModelInterface;
class ProcessViewModelInterface;
namespace iscore
{
    class SerializableCommand;
}
class ProcessPresenterInterface : public NamedObject
{
        Q_OBJECT
    public:
        using NamedObject::NamedObject;
        virtual ~ProcessPresenterInterface() = default;

        virtual void setWidth(int width) = 0;
        virtual void setHeight(int height) = 0;

        virtual void putToFront() = 0;
        virtual void putBehind() = 0;

        virtual void on_horizontalZoomChanged(int) = 0;
        virtual void parentGeometryChanged() = 0;

        virtual id_type<ProcessViewModelInterface> viewModelId() const = 0;
        virtual id_type<ProcessSharedModelInterface> modelId() const = 0;
};
