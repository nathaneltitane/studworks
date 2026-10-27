#pragma once

class swElidedLabel : public QFrame
{
public:
	explicit swElidedLabel(QWidget* Parent = nullptr)
		: QFrame(Parent)
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}

	void setText(const QString& Text)
	{
		mText = Text;
		update();
	}

protected:
	void paintEvent(QPaintEvent* event) override;

	QString mText;
};
