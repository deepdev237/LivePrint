
#include "LiveBPGraphEditor.h"
#include "LiveBPEditor.h"
#include "LiveBPEditorSubsystem.h"
#include "LiveBPSettings.h"
#include "SGraphEditor.h"
#include "SGraphPanel.h"
#include "GraphEditor.h"
#include "BlueprintGraph/Classes/K2Node.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Engine/Engine.h"

void SLiveBPGraphEditor::Construct(const FArguments& InArgs)
{
	bIsWireDragging = false;
	DragStartPinId = FGuid();
	DragStartPosition = FVector2D::ZeroVector;
	LastMousePosition = FVector2D::ZeroVector;
	LastCleanupTime = 0.0f;
	
	// Get editor subsystem
	if (GEngine)
	{
		EditorSubsystem = GEngine->GetEngineSubsystem<ULiveBPEditorSubsystem>();
	}
	
	// Create the standard graph editor
	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
	
	SGraphEditor::FArguments GraphArgs;
	GraphArgs._GraphToEdit = InArgs._GraphToEdit;
	GraphArgs._IsEditable = InArgs._IsEditable;
	GraphArgs._OnNodeDoubleClicked = InArgs._OnNodeDoubleClicked;
	GraphArgs._OnSelectionChanged = InArgs._OnSelectionChanged;
	
	// Add collaboration-specific callbacks
	GraphArgs._OnSpawnNodeByShortcut = FOnSpawnNodeByShortcut::CreateSP(this, &SLiveBPGraphEditor::OnSpawnNodeByShortcut);
	GraphArgs._OnNodeSingleClicked = FOnNodeSingleClicked::CreateSP(this, &SLiveBPGraphEditor::OnNodeSingleClicked);
	
	GraphEditor = SNew(SGraphEditor)
		.GraphToEdit(InArgs._GraphToEdit)
		.IsEditable(InArgs._IsEditable)
		.OnNodeDoubleClicked(InArgs._OnNodeDoubleClicked)
		.OnSelectionChanged(InArgs._OnSelectionChanged)
		.OnSpawnNodeByShortcut_Lambda([this](FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph)
		{
			return OnSpawnNodeByShortcut(InChord, InPosition, InGraph);
		})
		.OnNodeSingleClicked_Lambda([this](UEdGraphNode* InNode)
		{
			OnNodeSingleClicked(InNode);
		});
	
	// Store current blueprint
	if (InArgs._GraphToEdit)
	{
		CurrentBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(InArgs._GraphToEdit);
	}
	
	// Set up main layout
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			GraphEditor.ToSharedRef()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			// Collaboration overlay (will be drawn in OnPaint)
			SNullWidget::NullWidget
		]
	];
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP Graph Editor constructed"));
}

void SLiveBPGraphEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	// Cleanup stale remote data periodically
	if (InCurrentTime - LastCleanupTime > 1.0f)
	{
		CleanupStaleRemoteData();
		LastCleanupTime = InCurrentTime;
	}
	
	// Update collaboration overlay
	UpdateCollaborationOverlay();
}

int32 SLiveBPGraphEditor::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, 
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Paint the base graph editor first
	int32 MaxLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	
	// Draw collaboration overlays on top
	const ULiveBPSettings* Settings = GetDefault<ULiveBPSettings>();
	if (!Settings || !Settings->bShowRemoteUserCursors)
	{
		return MaxLayerId;
	}
	
	// Draw remote user cursors
	for (const auto& CursorPair : RemoteUserCursors)
	{
		if (CursorPair.Value.bIsVisible)
		{
			DrawRemoteUserCursor(AllottedGeometry, OutDrawElements, MaxLayerId + 1, CursorPair.Key, CursorPair.Value);
		}
	}
	
	// Draw wire drag previews
	if (Settings->bShowWireDragPreviews)
	{
		for (const auto& PreviewPair : WireDragPreviews)
		{
			if (PreviewPair.Value.bIsActive)
			{
				DrawWireDragPreview(AllottedGeometry, OutDrawElements, MaxLayerId + 2, PreviewPair.Key, PreviewPair.Value);
			}
		}
	}
	
	// Draw node lock feedback
	if (Settings->bShowNodeLockFeedback)
	{
		for (const auto& LockPair : NodeLockVisuals)
		{
			if (LockPair.Value.bIsLocked)
			{
				DrawNodeLockFeedback(AllottedGeometry, OutDrawElements, MaxLayerId + 3, LockPair.Value);
			}
		}
	}
	
	return MaxLayerId + 4;
}

FReply SLiveBPGraphEditor::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	LastMousePosition = MousePosition;
	
	// Handle local wire dragging
	if (bIsWireDragging)
	{
		OnLocalWireDragUpdate(ScreenToGraphPosition(MyGeometry, MousePosition));
		SendWirePreviewMessage(ScreenToGraphPosition(MyGeometry, MousePosition));
	}
	
	// Let the base graph editor handle the event first
	FReply Reply = GraphEditor->OnMouseMove(MyGeometry, MouseEvent);
	
	// Send cursor position to other users if enabled
	const ULiveBPSettings* Settings = GetDefault<ULiveBPSettings>();
	if (Settings && Settings->bBroadcastCursorPosition && EditorSubsystem.IsValid())
	{
		FVector2D GraphPos = ScreenToGraphPosition(MyGeometry, MousePosition);
		EditorSubsystem->BroadcastCursorPosition(GraphPos);
	}
	
	return Reply;
}

FReply SLiveBPGraphEditor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Check if clicking on a pin to start wire drag
	FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D GraphPosition = ScreenToGraphPosition(MyGeometry, MousePosition);
	
	FGuid PinId = FindPinAtPosition(GraphPosition);
	if (PinId.IsValid() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Check if we can acquire a lock on the node containing this pin
		if (EditorSubsystem.IsValid())
		{
			// Find the node that owns this pin
			UEdGraphNode* OwnerNode = nullptr;
			if (CurrentBlueprint.IsValid() && CurrentBlueprint->GeneratedClass)
			{
				// Implementation would find the node that owns the pin
				// For now, we'll use a placeholder
			}
			
			if (OwnerNode)
			{
				FGuid NodeId; // Would get actual node ID
				if (EditorSubsystem->TryAcquireNodeLock(NodeId))
				{
					OnLocalWireDragStart(PinId, GraphPosition);
				}
				else
				{
					// Show notification that node is locked
					UE_LOG(LogLiveBPEditor, Warning, TEXT("Cannot start wire drag - node is locked by another user"));
					return FReply::Handled();
				}
			}
		}
	}
	
	return GraphEditor->OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SLiveBPGraphEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsWireDragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		FVector2D GraphPosition = ScreenToGraphPosition(MyGeometry, MousePosition);
		
		// Check if we're over a valid connection target
		FGuid TargetPinId = FindPinAtPosition(GraphPosition);
		bool bConnected = TargetPinId.IsValid();
		
		OnLocalWireDragEnd(GraphPosition, bConnected);
	}
	
	return GraphEditor->OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SLiveBPGraphEditor::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Let the graph editor handle drag detection first
	return GraphEditor->OnDragDetected(MyGeometry, MouseEvent);
}

void SLiveBPGraphEditor::UpdateRemoteUserCursor(const FString& UserId, const FVector2D& Position, const FLinearColor& Color)
{
	FRemoteUserCursor& Cursor = RemoteUserCursors.FindOrAdd(UserId);
	Cursor.Position = Position;
	Cursor.Color = Color;
	Cursor.LastUpdateTime = FPlatformTime::Seconds();
	Cursor.bIsVisible = true;
}

void SLiveBPGraphEditor::UpdateWireDragPreview(const FLiveBPWirePreviewMessage& Message)
{
	FWireDragPreview& Preview = WireDragPreviews.FindOrAdd(Message.SenderId);
	Preview.StartPinId = Message.StartPinId;
	Preview.CurrentPosition = Message.CurrentMousePosition;
	Preview.Color = GetUserColor(Message.SenderId);
	Preview.LastUpdateTime = FPlatformTime::Seconds();
	Preview.bIsActive = true;
	
	// Find start position from pin
	// This would require blueprint graph traversal to find the actual pin position
	Preview.StartPosition = FVector2D(0, 0); // Placeholder
}

void SLiveBPGraphEditor::ClearWireDragPreview(const FString& UserId)
{
	if (FWireDragPreview* Preview = WireDragPreviews.Find(UserId))
	{
		Preview->bIsActive = false;
	}
}

void SLiveBPGraphEditor::ShowNodeLockFeedback(const FGuid& NodeId, const FString& LockedByUser, bool bIsLocked)
{
	FNodeLockVisual& LockVisual = NodeLockVisuals.FindOrAdd(NodeId);
	LockVisual.NodeId = NodeId;
	LockVisual.LockedByUser = LockedByUser;
	LockVisual.LockColor = GetUserColor(LockedByUser);
	LockVisual.bIsLocked = bIsLocked;
	LockVisual.LockTime = FPlatformTime::Seconds();
}

void SLiveBPGraphEditor::UpdateCollaborationOverlay()
{
	// This method can be used to update any time-sensitive collaboration visuals
	// Currently handled in Tick and OnPaint
}

FLinearColor SLiveBPGraphEditor::GetUserColor(const FString& UserId)
{
	if (FLinearColor* ExistingColor = UserColors.Find(UserId))
	{
		return *ExistingColor;
	}
	
	// Generate a unique color for this user based on their ID hash
	uint32 Hash = GetTypeHash(UserId);
	float Hue = (Hash % 360) / 360.0f;
	FLinearColor NewColor = FLinearColor::MakeFromHSV8(Hue * 255, 200, 255);
	
	UserColors.Add(UserId, NewColor);
	return NewColor;
}

void SLiveBPGraphEditor::OnLocalWireDragStart(const FGuid& PinId, const FVector2D& Position)
{
	bIsWireDragging = true;
	DragStartPinId = PinId;
	DragStartPosition = Position;
	
	// Send initial wire preview message
	SendWirePreviewMessage(Position);
	
	UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Started local wire drag from pin %s at position %s"), 
		*PinId.ToString(), *Position.ToString());
}

void SLiveBPGraphEditor::OnLocalWireDragUpdate(const FVector2D& Position)
{
	if (!bIsWireDragging)
		return;
	
	// Throttled sending handled by SendWirePreviewMessage
	SendWirePreviewMessage(Position);
}

void SLiveBPGraphEditor::OnLocalWireDragEnd(const FVector2D& Position, bool bConnected)
{
	if (!bIsWireDragging)
		return;
	
	bIsWireDragging = false;
	
	// Send final wire preview message indicating end
	if (EditorSubsystem.IsValid())
	{
		FLiveBPWirePreviewMessage EndMessage;
		EndMessage.MessageId = FGuid::NewGuid();
		EndMessage.SenderId = EditorSubsystem->GetLocalUserId();
		EndMessage.BlueprintId = CurrentBlueprint.IsValid() ? 
			FGuid() : FGuid(); // Would get actual blueprint ID
		EndMessage.StartPinId = DragStartPinId;
		EndMessage.CurrentMousePosition = Position;
		EndMessage.bIsDragEnd = true;
		EndMessage.bWasConnected = bConnected;
		EndMessage.Timestamp = FPlatformTime::Seconds();
		
		EditorSubsystem->SendWirePreviewMessage(EndMessage);
	}
	
	// Release any node locks we were holding
	// Implementation would release locks on nodes involved in the wire drag
	
	UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Ended local wire drag at position %s, connected: %s"), 
		*Position.ToString(), bConnected ? TEXT("true") : TEXT("false"));
}

void SLiveBPGraphEditor::SendWirePreviewMessage(const FVector2D& MousePosition)
{
	if (!EditorSubsystem.IsValid() || !bIsWireDragging)
		return;
	
	FLiveBPWirePreviewMessage Message;
	Message.MessageId = FGuid::NewGuid();
	Message.SenderId = EditorSubsystem->GetLocalUserId();
	Message.BlueprintId = CurrentBlueprint.IsValid() ? 
		FGuid() : FGuid(); // Would get actual blueprint ID
	Message.StartPinId = DragStartPinId;
	Message.CurrentMousePosition = MousePosition;
	Message.bIsDragEnd = false;
	Message.bWasConnected = false;
	Message.Timestamp = FPlatformTime::Seconds();
	
	EditorSubsystem->SendWirePreviewMessage(Message);
}

void SLiveBPGraphEditor::DrawRemoteUserCursor(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, const FString& UserId, const FRemoteUserCursor& Cursor) const
{
	FVector2D ScreenPos = GraphToScreenPosition(AllottedGeometry, Cursor.Position);
	
	// Draw cursor triangle
	TArray<FVector2D> CursorPoints;
	CursorPoints.Add(ScreenPos);
	CursorPoints.Add(ScreenPos + FVector2D(0, CURSOR_SIZE));
	CursorPoints.Add(ScreenPos + FVector2D(CURSOR_SIZE * 0.6f, CURSOR_SIZE * 0.6f));
	
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		CursorPoints,
		ESlateDrawEffect::None,
		Cursor.Color,
		true,
		2.0f
	);
	
	// Draw user name label
	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	FVector2D LabelPos = ScreenPos + FVector2D(CURSOR_SIZE + 5, 0);
	
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(LabelPos, FVector2D(100, 20)),
		UserId,
		FontInfo,
		ESlateDrawEffect::None,
		Cursor.Color
	);
}

void SLiveBPGraphEditor::DrawWireDragPreview(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, const FString& UserId, const FWireDragPreview& Preview) const
{
	FVector2D StartScreenPos = GraphToScreenPosition(AllottedGeometry, Preview.StartPosition);
	FVector2D EndScreenPos = GraphToScreenPosition(AllottedGeometry, Preview.CurrentPosition);
	
	// Draw preview wire with bezier curve (similar to Blueprint wires)
	FVector2D ControlPoint1 = StartScreenPos + FVector2D(50, 0);
	FVector2D ControlPoint2 = EndScreenPos + FVector2D(-50, 0);
	
	TArray<FVector2D> BezierPoints;
	const int32 NumSegments = 20;
	for (int32 i = 0; i <= NumSegments; ++i)
	{
		float T = (float)i / NumSegments;
		FVector2D Point = FMath::CubicBezier(StartScreenPos, ControlPoint1, ControlPoint2, EndScreenPos, T);
		BezierPoints.Add(Point);
	}
	
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		BezierPoints,
		ESlateDrawEffect::None,
		Preview.Color,
		true,
		WIRE_THICKNESS
	);
}

void SLiveBPGraphEditor::DrawNodeLockFeedback(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, const FNodeLockVisual& LockVisual) const
{
	// Find node bounds
	FVector2D NodePos = GetNodePosition(LockVisual.NodeId);
	FVector2D NodeSize(200, 100); // Placeholder - would get actual node size
	
	FVector2D ScreenPos = GraphToScreenPosition(AllottedGeometry, NodePos);
	FVector2D ScreenSize = NodeSize; // Would need to transform size as well
	
	// Draw lock border
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(ScreenPos - FVector2D(LOCK_BORDER_THICKNESS), 
			ScreenSize + FVector2D(LOCK_BORDER_THICKNESS * 2)),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		LockVisual.LockColor * 0.7f
	);
	
	// Draw lock icon and user name
	FVector2D LabelPos = ScreenPos + FVector2D(0, -25);
	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	
	FString LockText = FString::Printf(TEXT("🔒 Locked by %s"), *LockVisual.LockedByUser);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(LabelPos, FVector2D(200, 20)),
		LockText,
		FontInfo,
		ESlateDrawEffect::None,
		LockVisual.LockColor
	);
}

FVector2D SLiveBPGraphEditor::ScreenToGraphPosition(const FGeometry& Geometry, const FVector2D& ScreenPosition) const
{
	if (GraphEditor.IsValid())
	{
		// This would use the graph editor's coordinate transformation
		// For now, return screen position as placeholder
		return ScreenPosition;
	}
	return ScreenPosition;
}

FVector2D SLiveBPGraphEditor::GraphToScreenPosition(const FGeometry& Geometry, const FVector2D& GraphPosition) const
{
	if (GraphEditor.IsValid())
	{
		// This would use the graph editor's coordinate transformation
		// For now, return graph position as placeholder
		return GraphPosition;
	}
	return GraphPosition;
}

FGuid SLiveBPGraphEditor::FindPinAtPosition(const FVector2D& GraphPosition) const
{
	// This would traverse the graph to find pins at the given position
	// Implementation would require access to the graph's spatial data structures
	return FGuid();
}

FVector2D SLiveBPGraphEditor::GetNodePosition(const FGuid& NodeId) const
{
	// This would find the node by ID and return its position
	// Implementation would require graph traversal
	return FVector2D::ZeroVector;
}

void SLiveBPGraphEditor::CleanupStaleRemoteData()
{
	float CurrentTime = FPlatformTime::Seconds();
	
	// Cleanup stale cursors
	for (auto It = RemoteUserCursors.CreateIterator(); It; ++It)
	{
		if (CurrentTime - It.Value().LastUpdateTime > REMOTE_CURSOR_TIMEOUT)
		{
			It.RemoveCurrent();
		}
	}
	
	// Cleanup stale wire previews
	for (auto It = WireDragPreviews.CreateIterator(); It; ++It)
	{
		if (CurrentTime - It.Value().LastUpdateTime > WIRE_PREVIEW_TIMEOUT)
		{
			It.RemoveCurrent();
		}
	}
}

UEdGraphNode* SLiveBPGraphEditor::OnSpawnNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph)
{
	// Handle collaborative node spawning
	if (EditorSubsystem.IsValid())
	{
		// Check if we can spawn a node at this position
		// Implementation would check for conflicts and acquire necessary locks
		UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Spawning node by shortcut at position %s"), *InPosition.ToString());
	}
	
	// Return nullptr to let standard handling proceed
	return nullptr;
}

void SLiveBPGraphEditor::OnNodeSingleClicked(UEdGraphNode* InNode)
{
	// Handle collaborative node selection
	if (EditorSubsystem.IsValid() && InNode)
	{
		FGuid NodeId; // Would get actual node ID
		EditorSubsystem->BroadcastNodeSelection(NodeId, true);
		
		UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Node single clicked: %s"), *InNode->GetName());
	}
}
