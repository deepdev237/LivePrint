
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "BlueprintGraph/Classes/K2Node.h"
#include "SGraphEditor.h"
#include "LiveBPDataTypes.h"
#include "LiveBPEditorSubsystem.h"

class SGraphPanel;
class UBlueprint;
class UEdGraph;
struct FGeometry;
struct FPointerEvent;

/**
 * Custom SGraphEditor extension for Live Blueprint collaboration
 * Handles real-time wire drag previews, cursor tracking, and visual feedback
 */
class LIVEBPEDITOR_API SLiveBPGraphEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveBPGraphEditor)
		: _GraphToEdit(nullptr)
		, _IsEditable(true)
		{}
		
		SLATE_ARGUMENT(UEdGraph*, GraphToEdit)
		SLATE_ARGUMENT(bool, IsEditable)
		SLATE_EVENT(FOnNodeDoubleClicked, OnNodeDoubleClicked)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	/** Get the underlying SGraphEditor widget */
	TSharedPtr<SGraphEditor> GetGraphEditor() const { return GraphEditor; }
	
	/** Update remote user cursors */
	void UpdateRemoteUserCursor(const FString& UserId, const FVector2D& Position, const FLinearColor& Color);
	
	/** Update wire drag preview from remote user */
	void UpdateWireDragPreview(const FLiveBPWirePreviewMessage& Message);
	
	/** Clear wire drag preview for a user */
	void ClearWireDragPreview(const FString& UserId);
	
	/** Show node lock visual feedback */
	void ShowNodeLockFeedback(const FGuid& NodeId, const FString& LockedByUser, bool bIsLocked);
	
	/** Update collaboration overlay (cursors, previews, locks) */
	void UpdateCollaborationOverlay();

protected:
	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, 
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	/** The main graph editor widget */
	TSharedPtr<SGraphEditor> GraphEditor;
	
	/** Current blueprint being edited */
	TWeakObjectPtr<UBlueprint> CurrentBlueprint;
	
	/** Editor subsystem for collaboration */
	TWeakObjectPtr<ULiveBPEditorSubsystem> EditorSubsystem;
	
	/** Remote user cursor data */
	struct FRemoteUserCursor
	{
		FVector2D Position;
		FLinearColor Color;
		float LastUpdateTime;
		bool bIsVisible;
		
		FRemoteUserCursor() : Position(FVector2D::ZeroVector), Color(FLinearColor::White), LastUpdateTime(0.0f), bIsVisible(false) {}
	};
	TMap<FString, FRemoteUserCursor> RemoteUserCursors;
	
	/** Wire drag preview data */
	struct FWireDragPreview
	{
		FGuid StartPinId;
		FVector2D StartPosition;
		FVector2D CurrentPosition;
		FLinearColor Color;
		float LastUpdateTime;
		bool bIsActive;
		
		FWireDragPreview() : StartPinId(), StartPosition(FVector2D::ZeroVector), CurrentPosition(FVector2D::ZeroVector), 
			Color(FLinearColor::White), LastUpdateTime(0.0f), bIsActive(false) {}
	};
	TMap<FString, FWireDragPreview> WireDragPreviews;
	
	/** Node lock visual feedback */
	struct FNodeLockVisual
	{
		FGuid NodeId;
		FString LockedByUser;
		FLinearColor LockColor;
		bool bIsLocked;
		float LockTime;
		
		FNodeLockVisual() : NodeId(), LockedByUser(), LockColor(FLinearColor::Red), bIsLocked(false), LockTime(0.0f) {}
	};
	TMap<FGuid, FNodeLockVisual> NodeLockVisuals;
	
	/** Current wire drag state (local user) */
	bool bIsWireDragging;
	FGuid DragStartPinId;
	FVector2D DragStartPosition;
	FVector2D LastMousePosition;
	
	/** Collaboration colors for different users */
	TMap<FString, FLinearColor> UserColors;
	
	/** Get or create a color for a user */
	FLinearColor GetUserColor(const FString& UserId);
	
	/** Handle local wire drag start */
	void OnLocalWireDragStart(const FGuid& PinId, const FVector2D& Position);
	
	/** Handle local wire drag update */
	void OnLocalWireDragUpdate(const FVector2D& Position);
	
	/** Handle local wire drag end */
	void OnLocalWireDragEnd(const FVector2D& Position, bool bConnected);
	
	/** Send wire preview message to other users */
	void SendWirePreviewMessage(const FVector2D& MousePosition);
	
	/** Draw remote user cursor */
	void DrawRemoteUserCursor(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FString& UserId, const FRemoteUserCursor& Cursor) const;
	
	/** Draw wire drag preview */
	void DrawWireDragPreview(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FString& UserId, const FWireDragPreview& Preview) const;
	
	/** Draw node lock visual feedback */
	void DrawNodeLockFeedback(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FNodeLockVisual& LockVisual) const;
	
	/** Convert screen position to graph position */
	FVector2D ScreenToGraphPosition(const FGeometry& Geometry, const FVector2D& ScreenPosition) const;
	
	/** Convert graph position to screen position */
	FVector2D GraphToScreenPosition(const FGeometry& Geometry, const FVector2D& GraphPosition) const;
	
	/** Find pin at graph position */
	FGuid FindPinAtPosition(const FVector2D& GraphPosition) const;
	
	/** Get node position by ID */
	FVector2D GetNodePosition(const FGuid& NodeId) const;
	
	/** Timer for cleaning up stale remote data */
	float LastCleanupTime;
	void CleanupStaleRemoteData();
	
	/** Constants */
	static constexpr float REMOTE_CURSOR_TIMEOUT = 5.0f;
	static constexpr float WIRE_PREVIEW_TIMEOUT = 2.0f;
	static constexpr float CURSOR_SIZE = 16.0f;
	static constexpr float WIRE_THICKNESS = 3.0f;
	static constexpr float LOCK_BORDER_THICKNESS = 4.0f;
};
