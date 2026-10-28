#pragma once

class Project;
class swInstructions;
class swLDrawHighlighter;
class swEditorTextEdit;

// the editor panel shows the text the project saves, applying an edited text loads the project from it
class swInstructionsEditorWidget : public QDockWidget
{
	Q_OBJECT

public:
	swInstructionsEditorWidget(QWidget* Parent, Project* Project, swInstructions* Instructions);

	void SetProject(Project* Project);
	void ScheduleUpdate();

	int GetTextPosition() const;
	int GetTextScroll() const;
	void SetTextPosition(int Position, int Scroll);

protected:
	void UpdateText();
	void UpdateButtons();
	void ApplyClicked();
	void ApplyText(bool Save);
	bool ValidateText();
	void RevertClicked();
	void SetHighlightEnabled(bool Enabled);
	void SetNumbersEnabled(bool Enabled);
	void SetWrapEnabled(bool Enabled);
	void SetMarkersEnabled(bool Enabled);
	void SetFindVisible(bool Visible);
	void FindText(bool Backward);
	void ReplaceClicked();
	void ReplaceAllClicked();
	void UpdateMatches();
	QTextDocument::FindFlags GetFindFlags() const;

	Project* mProject = nullptr;
	swInstructions* mInstructions = nullptr;
	bool mUpdatePending = false;

	swLDrawHighlighter* mHighlighter = nullptr;
	swEditorTextEdit* mTextEdit = nullptr;
	QToolButton* mApplyButton = nullptr;
	QToolButton* mRevertButton = nullptr;
	QAction* mFindAction = nullptr;
	QWidget* mFindWidget = nullptr;
	QLineEdit* mFindEdit = nullptr;
	QLineEdit* mReplaceEdit = nullptr;
	QLabel* mMatchLabel = nullptr;
	bool mMatchCase = false;
	bool mWholeWords = false;
};
