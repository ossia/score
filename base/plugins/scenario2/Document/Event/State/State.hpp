#pragma once
#include <tools/NamedObject.hpp>

class DataType { };
class Impulse : public DataType { }; // ignore QVariant
class Boolean : public DataType { }; // QVariant = bool
class Integer : public DataType { }; // QVariant = uint32
class Decimal : public DataType { }; // QVariant = float
class String  : public DataType { }; // QVariant = std::string
class Tuple   : public DataType { }; // QVariant = std::tuple ou struct...

class Domain { };
enum class AccessMode { Get, Set, Both };
enum class BoundingMode { Free, Clip, Wrap, Fold };
struct Message
{
		QString address;
		QVariant value; // On peut utiliser val.type() et val.canConvert() directement...
		DataType type; // ?

		AccessMode mode{AccessMode::Both};
		BoundingMode minBoundingMode;
		BoundingMode maxBoundingMode;

		Domain domain;
};

class State : public NamedObject
{
		Q_OBJECT

	public:
		State(QObject* parent): NamedObject{parent, "State"}
		{

		}

		virtual ~State() = default;
		virtual QStringList messages() const = 0;
		virtual void addMessage(QString message) = 0;

	private:

};

class FakeState : public State
{
		Q_OBJECT

	public:
		using State::State;

		virtual QStringList messages() const override
		{
			return {m_address};
		}

		virtual void addMessage(QString message) override
		{
			m_address = message;
		}

	private:
		QString m_address;
		QVariant val;
};

