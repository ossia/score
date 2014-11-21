#pragma once
#include <QNamedObject>
#include "Storey/StoreyModel.hpp"
#include <vector>
class IntervalModel;
class PositionedStoreyModel : public StoreyModel
{
		Q_OBJECT
		Q_PROPERTY(int position READ position WRITE setPosition NOTIFY positionChanged)
		
	public:
		int position() const
		{
			return m_position;
		}
		
	signals:
		void positionChanged(int arg);
		
	public slots:
		void setPosition(int arg)
		{
			if (m_position == arg)
				return;
			
			m_position = arg;
			emit positionChanged(arg);
		}
		
	private:
		int m_position;
};

class IntervalContentModel : public QNamedObject
{
	Q_OBJECT
	
	public:
		IntervalContentModel(IntervalModel* parent);
		
		virtual ~IntervalContentModel() = default;
		int id() const 
		{ return m_id; }
		
		void createStorey();
		void deleteStorey(int storeyId);
		void changeStoreyOrder(int storeyId, int position);
		void duplicateStorey();
		
	private:
		std::vector<StoreyModel*> m_storeys;
		
		int m_id{};
};

