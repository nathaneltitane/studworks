#include "sw_global.h"
#include "sw_object_property.h"
#include "sw_math.h"

#define SW_OBJECT_PROPERTY(T) \
	template void swObjectProperty<T>::Update(swStep Step); \
	template bool swObjectProperty<T>::ChangeKey(const T& Value, swStep Step, bool AddKey); \
	template void swObjectProperty<T>::InsertTime(swStep Start, swStep Time); \
	template void swObjectProperty<T>::RemoveTime(swStep Start, swStep Time); \
	template bool swObjectProperty<T>::HasKeyFrame(swStep Time) const; \
	template bool swObjectProperty<T>::SetKeyFrame(swStep Time, bool KeyFrame); \
	template void swObjectProperty<T>::Save(QTextStream& Stream, const char* ObjectName, const char* VariableName, bool SaveEmpty) const; \
	template bool swObjectProperty<T>::Load(QTextStream& Stream, const QString& Token, const char* VariableName);

SW_OBJECT_PROPERTY(float)
SW_OBJECT_PROPERTY(int)
SW_OBJECT_PROPERTY(swVector2i)
SW_OBJECT_PROPERTY(swVector2)
SW_OBJECT_PROPERTY(swVector3)
SW_OBJECT_PROPERTY(swVector4)
SW_OBJECT_PROPERTY(swMatrix33)

template<typename T>
static void swObjectPropertySaveValue(QTextStream& Stream, const T& Value)
{
	constexpr int Count = sizeof(T) / sizeof(float);

	for (int ValueIndex = 0; ValueIndex < Count; ValueIndex++)
		Stream << ((const float*)&Value)[ValueIndex] << ' ';
}

template<>
void swObjectPropertySaveValue(QTextStream& Stream, const int& Value)
{
	constexpr int Count = sizeof(int) / sizeof(int);

	for (int ValueIndex = 0; ValueIndex < Count; ValueIndex++)
		Stream << ((const int*)&Value)[ValueIndex] << ' ';
}

template<>
void swObjectPropertySaveValue(QTextStream& Stream, const swVector2i& Value)
{
	constexpr int Count = sizeof(swVector2i) / sizeof(int);

	for (int ValueIndex = 0; ValueIndex < Count; ValueIndex++)
		Stream << ((const int*)&Value)[ValueIndex] << ' ';
}

template<typename T>
static void swObjectPropertyLoadValue(QTextStream& Stream, T& Value)
{
	constexpr int Count = sizeof(T) / sizeof(float);

	for (int ValueIdx = 0; ValueIdx < Count; ValueIdx++)
		Stream >> ((float*)&Value)[ValueIdx];
}

template<>
void swObjectPropertyLoadValue(QTextStream& Stream, int& Value)
{
	constexpr int Count = sizeof(int) / sizeof(int);

	for (int ValueIdx = 0; ValueIdx < Count; ValueIdx++)
		Stream >> ((int*)&Value)[ValueIdx];
}

template<>
void swObjectPropertyLoadValue(QTextStream& Stream, swVector2i& Value)
{
	constexpr int Count = sizeof(swVector2i) / sizeof(int);

	for (int ValueIdx = 0; ValueIdx < Count; ValueIdx++)
		Stream >> ((int*)&Value)[ValueIdx];
}

template<typename T>
void swObjectProperty<T>::Update(swStep Step)
{
	if (mKeys.empty())
		return;

	const swObjectPropertyKey<T>* PreviousKey = &mKeys[0];

	for (const swObjectPropertyKey<T>& Key : mKeys)
	{
		if (Key.Step > Step)
			break;

		PreviousKey = &Key;
	}

	mValue = PreviousKey->Value;
}

template<typename T>
bool swObjectProperty<T>::ChangeKey(const T& Value, swStep Step, bool AddKey)
{
	if (!AddKey && mKeys.empty())
	{
		if (mValue == Value)
			return false;

		mValue = Value;

		return true;
	}

	for (typename std::vector<swObjectPropertyKey<T>>::iterator KeyIt = mKeys.begin(); KeyIt != mKeys.end(); KeyIt++)
	{
		if (KeyIt->Step < Step)
			continue;

		if (KeyIt->Step == Step)
		{
			if (KeyIt->Value == Value)
				return false;

			KeyIt->Value = Value;
		}
		else if (AddKey)
			mKeys.insert(KeyIt, swObjectPropertyKey<T>{ Step, Value });
		else if (KeyIt == mKeys.begin())
		{
			if (KeyIt->Value == Value)
				return false;

			KeyIt->Value = Value;
		}
		else
		{
			KeyIt = KeyIt - 1;

			if (KeyIt->Value == Value)
				return false;

			KeyIt->Value = Value;
		}

		return true;
	}

	if (AddKey || mKeys.empty())
		mKeys.emplace_back(swObjectPropertyKey<T>{ Step, Value });
	else
		mKeys.back().Value = Value;

	return true;
}

template<typename T>
void swObjectProperty<T>::InsertTime(swStep Start, swStep Time)
{
	bool EndKey = false;

	for (typename std::vector<swObjectPropertyKey<T>>::iterator KeyIt = mKeys.begin(); KeyIt != mKeys.end();)
	{
		if ((KeyIt->Step < Start) || (KeyIt->Step == 1))
		{
			KeyIt++;
			continue;
		}

		if (EndKey)
		{
			KeyIt = mKeys.erase(KeyIt);
			continue;
		}

		if (KeyIt->Step >= SW_STEP_MAXIMUM - Time)
		{
			KeyIt->Step = SW_STEP_MAXIMUM;
			EndKey = true;
		}
		else
			KeyIt->Step += Time;

		KeyIt++;
	}
}

template<typename T>
void swObjectProperty<T>::RemoveTime(swStep Start, swStep Time)
{
	for (typename std::vector<swObjectPropertyKey<T>>::iterator KeyIt = mKeys.begin(); KeyIt != mKeys.end();)
	{
		if ((KeyIt->Step < Start) || (KeyIt->Step == 1))
		{
			KeyIt++;
			continue;
		}

		if (KeyIt->Step < Start + Time)
		{
			KeyIt = mKeys.erase(KeyIt);
			continue;
		}

		KeyIt->Step -= Time;
		KeyIt++;
	}
}

template<typename T>
bool swObjectProperty<T>::HasKeyFrame(swStep Time) const
{
	for (typename std::vector<swObjectPropertyKey<T>>::const_iterator KeyIt = mKeys.begin(); KeyIt != mKeys.end(); KeyIt++)
	{
		if (KeyIt->Step == Time)
			return true;
		else if (KeyIt->Step > Time)
			return false;
	}

	return false;
}

template<typename T>
bool swObjectProperty<T>::SetKeyFrame(swStep Time, bool KeyFrame)
{
	if (KeyFrame)
	{
		typename std::vector<swObjectPropertyKey<T>>::const_iterator KeyIt;

		for (KeyIt = mKeys.begin(); KeyIt != mKeys.end(); KeyIt++)
		{
			if (KeyIt->Step == Time)
				return false;
			else if (KeyIt->Step > Time)
				break;
		}

		mKeys.insert(KeyIt, swObjectPropertyKey<T>{ Time, mValue });

		return true;
	}
	else
	{
		for (typename std::vector<swObjectPropertyKey<T>>::const_iterator KeyIt = mKeys.begin(); KeyIt != mKeys.end(); KeyIt++)
		{
			if (KeyIt->Step == Time)
			{
				if (mKeys.size() == 1)
					mValue = KeyIt->Value;

				mKeys.erase(KeyIt);

				return true;
			}
			else if (KeyIt->Step > Time)
				return false;
		}
	}

	return false;
}

template<typename T>
void swObjectProperty<T>::Save(QTextStream& Stream, const char* ObjectName, const char* VariableName, bool SaveEmpty) const
{
	if (mKeys.empty())
	{
		if (SaveEmpty)
		{
			Stream << QLatin1String("0 !LEOCAD ") << ObjectName << ' ' << VariableName << ' ';

			swObjectPropertySaveValue(Stream, mValue);

			Stream << QLatin1String("\r\n");
		}
	}
	else
	{
		for (const swObjectPropertyKey<T>& Key : mKeys)
		{
			Stream << QLatin1String("0 !LEOCAD ") << ObjectName << ' ' << VariableName << "_KEY " << Key.Step << ' ';

			swObjectPropertySaveValue(Stream, Key.Value);

			Stream << QLatin1String("\r\n");
		}
	}
}

template<typename T>
bool swObjectProperty<T>::Load(QTextStream& Stream, const QString& Token, const char* VariableName)
{
	if (Token == VariableName)
	{
		swObjectPropertyLoadValue(Stream, mValue);

		return true;
	}

	if (Token.endsWith(QLatin1String("_KEY")) && Token.left(Token.size() - 4) == VariableName)
	{
		QString StepString;
		Stream >> StepString;

		const int Step = StepString.toInt();
		T Value;

		constexpr int Count = sizeof(T) / sizeof(float);

		for (int ValueIdx = 0; ValueIdx < Count; ValueIdx++)
			Stream >> ((float*)&Value)[ValueIdx];

		ChangeKey(Value, Step, true);

		return true;
	}

	return false;
}
