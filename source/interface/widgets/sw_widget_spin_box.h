#pragma once

#include "sw_window_utilities.h"

// Spin box that clamps once typed.
template<typename SpinBoxType>
class swRangeSpinBoxBase : public SpinBoxType
{
public:
	explicit swRangeSpinBoxBase(QWidget* Parent)
		: SpinBoxType(Parent)
	{
		swSetupSpinBoxSelection(this);
	}

	// Shows the maximum after the value.
	void SetReference(const QString& Unit = QString())
	{
		this->setSuffix(QString(" / %1%2").arg(this->textFromValue(this->maximum()), Unit));
	}

	QValidator::State validate(QString& Text, int& Position) const override
	{
		const QValidator::State State = SpinBoxType::validate(Text, Position);

		if (State != QValidator::Invalid)
			return State;

		bool Valid = false;
		GetTypedValue(Text, Valid);

		return Valid ? QValidator::Intermediate : QValidator::Invalid;
	}

	void fixup(QString& Text) const override
	{
		bool Valid = false;
		const double Value = GetTypedValue(Text, Valid);

		if (Valid)
			Text = this->prefix() + this->textFromValue(static_cast<decltype(this->value())>(qBound<double>(this->minimum(), Value, this->maximum()))) + this->suffix();
		else
			SpinBoxType::fixup(Text);
	}

protected:
	// Reads the typed number.
	double GetTypedValue(const QString& Text, bool& Valid) const
	{
		QString Number = Text;

		if (!this->prefix().isEmpty() && Number.startsWith(this->prefix()))
			Number.remove(0, this->prefix().size());

		if (!this->suffix().isEmpty() && Number.endsWith(this->suffix()))
			Number.chop(this->suffix().size());

		return this->locale().toDouble(Number.trimmed(), &Valid);
	}
};

class swRangeSpinBox : public swRangeSpinBoxBase<QSpinBox>
{
public:
	using swRangeSpinBoxBase::swRangeSpinBoxBase;
};

class swRangeDoubleSpinBox : public swRangeSpinBoxBase<QDoubleSpinBox>
{
public:
	using swRangeSpinBoxBase::swRangeSpinBoxBase;
};
