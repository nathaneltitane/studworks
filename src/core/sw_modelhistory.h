#pragma once

#include "sw_model.h"
#include "sw_objecthistory.h"

enum class swObjectType;
enum class swSelectionMode;

class swModelHistory
{
public:
	swModelHistory() = default;
	virtual ~swModelHistory() = default;

	virtual void SaveStartState(const swModel* Model) = 0;
	virtual void SaveEndState(const swModel* Model) = 0;
	virtual void LoadStartState(swModel* Model) const = 0;
	virtual void LoadEndState(swModel* Model) const = 0;
	virtual bool StateChanged() const = 0;
	virtual void RenamePieces(const std::vector<swModelPieceRename>& Renames, const swPieceData* ModelPieceData)
	{
		Q_UNUSED(Renames);
		Q_UNUSED(ModelPieceData);
	}

	virtual bool CanMergeWith(const swModelHistory* Other) const
	{
		Q_UNUSED(Other);

		return false;
	}

	virtual void MergeWith(swModelHistory* Other)
	{
		Q_UNUSED(Other);
	}
};

struct swModelHistorySelectState
{
	std::vector<bool> PieceSelection;
	std::vector<bool> CameraSelection;
	std::vector<bool> LightSelection;
	size_t FocusIndex = SIZE_MAX;
	uint32_t FocusSection = ~0U;
	swObjectType FocusObjectType = static_cast<swObjectType>(~0);

	bool operator!=(const swModelHistorySelectState& Other) const
	{
		return PieceSelection != Other.PieceSelection || CameraSelection != Other.CameraSelection || LightSelection != Other.LightSelection ||
			FocusIndex != Other.FocusIndex || FocusSection != Other.FocusSection || FocusObjectType != Other.FocusObjectType;
	}
};

class swModelHistorySelect : public swModelHistory
{
public:
	swModelHistorySelect() = default;
	virtual ~swModelHistorySelect() = default;

	void SaveStartState(const swModel* Model) override;
	void SaveEndState(const swModel* Model) override;
	void LoadStartState(swModel* Model) const override;
	void LoadEndState(swModel* Model) const override;
	bool StateChanged() const override;

protected:
	static void SaveState(swModelHistorySelectState& State, const swModel* Model);
	static void LoadState(const swModelHistorySelectState& State, swModel* Model);

	swModelHistorySelectState mStartState;
	swModelHistorySelectState mEndState;
};

struct swGroupHistoryState;
struct swPieceHistoryState;
struct swCameraHistoryState;
struct swLightHistoryState;

struct swModelHistoryEditState
{
	swObjectHistory<swGroupHistoryState> Groups;
	swObjectHistory<swPieceHistoryState> Pieces;
	swObjectHistory<swCameraHistoryState> Cameras;
	swObjectHistory<swLightHistoryState> Lights;

	bool IsEmpty() const
	{
		return Groups.IsEmpty() && Pieces.IsEmpty() && Cameras.IsEmpty() && Lights.IsEmpty();
	}
};

enum class swModelHistoryEditMerge
{
	None,
	KeyboardMove,
	KeyboardRotate,
	KeyboardZoom,
	KeyboardMoveCamera,
	PropertiesMove,
	PropertiesRotate,
	PropertiesEdit
};

struct swModelHistoryEditMergeKey
{
	swModelHistoryEditMerge Operation;
	swObjectPropertyId PropertyEditId = swObjectPropertyId::Count;
	swStep Step;
	bool AddKeys;

	bool operator==(const swModelHistoryEditMergeKey& Other) const
	{
		return Operation == Other.Operation && PropertyEditId == Other.PropertyEditId && Step == Other.Step && AddKeys == Other.AddKeys;
	}
};

class swModelHistoryEdit : public swModelHistory
{
public:
	swModelHistoryEdit(swModelHistoryEditMergeKey MergeKey);
	virtual ~swModelHistoryEdit();

	void SaveStartState(const swModel* Model) override;
	void SaveEndState(const swModel* Model) override;
	void LoadStartState(swModel* Model) const override;
	void LoadEndState(swModel* Model) const override;
	bool StateChanged() const override;
	void RenamePieces(const std::vector<swModelPieceRename>& Renames, const swPieceData* ModelPieceData) override;

	bool CanMergeWith(const swModelHistory* Other) const override;
	void MergeWith(swModelHistory* Other) override;

protected:
	static void SaveState(swModelHistoryEditState& State, const swModel* Model);
	static void LoadState(const swModelHistoryEditState& State, swModel* Model);

	swModelHistoryEditState mStartState;
	swModelHistoryEditState mEndState;
	swModelHistoryEditMergeKey mMergeKey;
};

class swModelHistoryProperties : public swModelHistory
{
public:
	swModelHistoryProperties() = default;
	virtual ~swModelHistoryProperties() = default;

	void SaveStartState(const swModel* Model) override;
	void SaveEndState(const swModel* Model) override;
	void LoadStartState(swModel* Model) const override;
	void LoadEndState(swModel* Model) const override;
	bool StateChanged() const override;
	void RenamePieces(const std::vector<swModelPieceRename>& Renames, const swPieceData* ModelPieceData) override;

protected:
	static void SaveState(swModelProperties& State, const swModel* Model);
	static void LoadState(const swModelProperties& State, swModel* Model);

	swModelProperties mStartState;
	swModelProperties mEndState;
};
