#pragma once

class swCollapsibleWidgetButton : public QToolButton
{
	Q_OBJECT

public:
	swCollapsibleWidgetButton(const QString& Title, QWidget* Parent = nullptr);

	void Collapse();
	void SetCollapsible(bool Collapsible);
	void SetExpanded(bool Expanded);

	bool IsExpanded() const
	{
		return mExpanded;
	}

signals:
	void StateChanged(bool Expanded);

protected slots:
	void Clicked();

protected:
	void UpdateIcon();

	bool mExpanded = true;
	bool mCollapsible = true;
};

class swCollapsibleWidget : public QWidget
{
	Q_OBJECT

public:
	swCollapsibleWidget(const QString& RootTitle, QWidget* Parent = nullptr);

	void Collapse();
	void SetChildLayout(QLayout* Layout);

protected slots:
	void ButtonStateChanged(bool Expanded);

protected:
	swCollapsibleWidgetButton* mTitleButton = nullptr;
	QWidget* mChildWidget = nullptr;
};
