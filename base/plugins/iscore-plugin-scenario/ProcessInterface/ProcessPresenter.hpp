#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/ZoomHelper.hpp>

class ProcessModel;
class ProcessViewModel;
namespace iscore
{
    class SerializableCommand;
}
class ProcessPresenter : public NamedObject
{
        Q_OBJECT
        bool m_focus{false};

    public:
        using NamedObject::NamedObject;

        bool focused() const;
        void setFocus(bool focus);
        virtual void on_focusChanged();

        virtual ~ProcessPresenter() = default;

        virtual void setWidth(int width) = 0;
        virtual void setHeight(int height) = 0;

        virtual void putToFront() = 0;
        virtual void putBehind() = 0;

        virtual void on_zoomRatioChanged(ZoomRatio) = 0;
        virtual void parentGeometryChanged() = 0;

        virtual const ProcessViewModel& viewModel() const = 0;
        virtual const id_type<ProcessModel>& modelId() const = 0;

};
