#include "LiveBPEditorSubsystem.h"
#include "LiveBPEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "BlueprintGraph/Classes/K2Node.h"
#include "BlueprintGraph/Classes/BlueprintGraph.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SGraphEditor.h"
#include "GraphEditor.h"
#include "EdGraphSchema_K2.h"
#include "Toolkits/AssetEditorManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Editor.h"
#include "Subsystems/EditorSubsystem.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "EditorSubsystemBlueprintLibrary.h"

ULiveBPEditorSubsystem::ULiveBPEditorSubsystem()
	: bCollaborationEnabled(false)
	, bDebugModeEnabled(false)
	, LastWirePreviewTime(0.0f)
{
}

void ULiveBPEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogLiveBPEditor, Log, TEXT("Initializing LiveBP Editor Subsystem"));

	// Create core components
	MUEIntegration = NewObject<ULiveBPMUEIntegration>(this);

	// Bind delegates
	MUEIntegration->OnMessageReceived.AddUObject(this, &ULiveBPEditorSubsystem::OnMUEMessageReceived);

	RegisterBlueprintCallbacks();
}

void ULiveBPEditorSubsystem::Deinitialize()
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("Deinitializing LiveBP Editor Subsystem"));

	DisableCollaboration();
	UnregisterBlueprintCallbacks();

	if (MUEIntegration)
	{
		MUEIntegration->ShutdownConcertIntegration();
		MUEIntegration = nullptr;
	}

	Super::Deinitialize();
}

void ULiveBPEditorSubsystem::ToggleCollaboration()
{
	if (bCollaborationEnabled)
	{
		DisableCollaboration();
	}
	else
	{
		EnableCollaboration();
	}
}

void ULiveBPEditorSubsystem::EnableCollaboration()
{
	if (bCollaborationEnabled)
	{
		return;
	}

	UE_LOG(LogLiveBPEditor, Log, TEXT("Enabling LiveBP collaboration"));

	if (!MUEIntegration || !MUEIntegration->IsConnected())
	{
		ShowCollaborationNotification(TEXT("Cannot enable collaboration: not connected to MUE session"), 5.0f);
		return;
	}

	bCollaborationEnabled = true;
	ShowCollaborationNotification(TEXT("LiveBP collaboration enabled"), 3.0f);
}

void ULiveBPEditorSubsystem::DisableCollaboration()
{
	if (!bCollaborationEnabled)
	{
		return;
	}

	UE_LOG(LogLiveBPEditor, Log, TEXT("Disabling LiveBP collaboration"));

	bCollaborationEnabled = false;
	
	// Release all node locks
	NodeLocks.Empty();
	
	ShowCollaborationNotification(TEXT("LiveBP collaboration disabled"), 3.0f);
}

bool ULiveBPEditorSubsystem::RequestNodeLock(UEdGraphNode* Node, float LockDuration)
{
	if (!IsCollaborationEnabled() || !Node)
	{
		return false;
	}

	FGuid NodeId = GetNodeGuid(Node);
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
	if (!Blueprint)
	{
		return false;
	}

	// Check if node is already locked by someone else
	if (IsNodeLockedByOther(Node))
	{
		ShowCollaborationNotification(FString::Printf(TEXT("Node is locked by another user")), 3.0f);
		return false;
	}

	// Create lock request
	FLiveBPNodeLock LockRequest;
	LockRequest.NodeId = NodeId;
	LockRequest.LockState = ELiveBPLockState::Locked;
	LockRequest.UserId = MUEIntegration->GetCurrentUserId();
	LockRequest.LockTime = FPlatformTime::Seconds();
	LockRequest.ExpiryTime = LockRequest.LockTime + LockDuration;

	// Send lock request
	FGuid BlueprintId = GetBlueprintGuid(Blueprint);
	FGuid GraphId = GetGraphGuid(Node->GetGraph());
	
	if (MUEIntegration->SendLockRequest(LockRequest, BlueprintId, GraphId))
	{
		// Store lock locally
		NodeLocks.Add(NodeId, LockRequest);
		UpdateNodeVisualState(Node);
		return true;
	}

	return false;
}

bool ULiveBPEditorSubsystem::ReleaseNodeLock(UEdGraphNode* Node)
{
	if (!IsCollaborationEnabled() || !Node)
	{
		return false;
	}

	FGuid NodeId = GetNodeGuid(Node);
	
	// Check if we have this node locked
	if (FLiveBPNodeLock* ExistingLock = NodeLocks.Find(NodeId))
	{
		if (ExistingLock->UserId == MUEIntegration->GetCurrentUserId())
		{
			// Create unlock request
			FLiveBPNodeLock UnlockRequest = *ExistingLock;
			UnlockRequest.LockState = ELiveBPLockState::Unlocked;
			
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
			if (Blueprint)
			{
				FGuid BlueprintId = GetBlueprintGuid(Blueprint);
				FGuid GraphId = GetGraphGuid(Node->GetGraph());
				
				if (MUEIntegration->SendLockRequest(UnlockRequest, BlueprintId, GraphId))
				{
					NodeLocks.Remove(NodeId);
					UpdateNodeVisualState(Node);
					return true;
				}
			}
		}
	}

	return false;
}

bool ULiveBPEditorSubsystem::IsNodeLockedByOther(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return false;
	}

	FGuid NodeId = GetNodeGuid(Node);
	if (const FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId))
	{
		return Lock->LockState == ELiveBPLockState::Locked && 
			   Lock->UserId != MUEIntegration->GetCurrentUserId() &&
			   FPlatformTime::Seconds() < Lock->ExpiryTime;
	}

	return false;
}

bool ULiveBPEditorSubsystem::CanModifyNode(UEdGraphNode* Node) const
{
	if (!IsCollaborationEnabled())
	{
		return true;
	}

	return !IsNodeLockedByOther(Node);
}

// Blueprint editor integration
void ULiveBPEditorSubsystem::RegisterBlueprintCallbacks()
{
	// Register for asset editor events
	FAssetEditorManager::Get().OnAssetOpenedInEditor().AddUObject(this, &ULiveBPEditorSubsystem::OnAssetOpened);
	FAssetEditorManager::Get().OnAssetEditorRequestClose().AddUObject(this, &ULiveBPEditorSubsystem::OnAssetClosed);
}

void ULiveBPEditorSubsystem::UnregisterBlueprintCallbacks()
{
	FAssetEditorManager::Get().OnAssetOpenedInEditor().RemoveAll(this);
	FAssetEditorManager::Get().OnAssetEditorRequestClose().RemoveAll(this);

	// Unregister all Blueprint-specific callbacks
	for (auto& Pair : BlueprintDelegateHandles)
	{
		if (UBlueprint* Blueprint = Pair.Key)
		{
			UnregisterGraphEditorCallbacks(Blueprint);
		}
	}
	BlueprintDelegateHandles.Empty();
}

void ULiveBPEditorSubsystem::OnAssetOpened(UObject* Asset, IAssetEditorInstance* EditorInstance)
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		UE_LOG(LogLiveBPEditor, Log, TEXT("Blueprint opened: %s"), *Blueprint->GetName());
		
		// Store Blueprint GUID mapping
		FGuid BlueprintId = GetBlueprintGuid(Blueprint);
		BlueprintGuidMap.Add(BlueprintId, Blueprint);
		
		// Register for Blueprint-specific events if collaboration is enabled
		if (IsCollaborationEnabled())
		{
			RegisterGraphEditorCallbacks(Blueprint);
		}
	}
}

void ULiveBPEditorSubsystem::OnAssetClosed(UObject* Asset, IAssetEditorInstance* EditorInstance)
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		UE_LOG(LogLiveBPEditor, Log, TEXT("Blueprint closed: %s"), *Blueprint->GetName());
		
		// Remove from tracking
		UnregisterGraphEditorCallbacks(Blueprint);
		TrackedGraphEditors.Remove(Blueprint);
		BlueprintDelegateHandles.Remove(Blueprint);
		
		// Remove from GUID mapping
		FGuid BlueprintId = GetBlueprintGuid(Blueprint);
		BlueprintGuidMap.Remove(BlueprintId);
		
		// Release any locks on nodes in this Blueprint
		TArray<FGuid> LocksToRemove;
		for (const auto& LockPair : NodeLocks)
		{
			// Check if this lock belongs to a node in the closing Blueprint
			for (UEdGraph* Graph : Blueprint->UbergraphPages)
			{
				if (FindNodeByGuid(Graph, LockPair.Key))
				{
					LocksToRemove.Add(LockPair.Key);
					break;
				}
			}
		}
		
		for (const FGuid& LockId : LocksToRemove)
		{
			NodeLocks.Remove(LockId);
		}
	}
}

void ULiveBPEditorSubsystem::OnBlueprintPreCompile(UBlueprint* Blueprint)
{
	// Handle pre-compilation if needed
}

void ULiveBPEditorSubsystem::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	// Handle post-compilation if needed
}

void ULiveBPEditorSubsystem::RegisterGraphEditorCallbacks(UBlueprint* Blueprint)
{
	if (!Blueprint || BlueprintDelegateHandles.Contains(Blueprint))
	{
		return;
	}

	// For now, we'll use a simple approach - in a full implementation,
	// we would hook into the graph editor's drag/drop events
	FDelegateHandle Handle;
	BlueprintDelegateHandles.Add(Blueprint, Handle);
}

void ULiveBPEditorSubsystem::UnregisterGraphEditorCallbacks(UBlueprint* Blueprint)
{
	if (FDelegateHandle* Handle = BlueprintDelegateHandles.Find(Blueprint))
	{
		// Remove any registered delegates
		BlueprintDelegateHandles.Remove(Blueprint);
	}
}

// Node operation handlers
void ULiveBPEditorSubsystem::OnNodeAdded(UEdGraphNode* Node)
{
	if (!IsCollaborationEnabled() || !Node)
	{
		return;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
	if (!Blueprint)
	{
		return;
	}

	// Create node operation data
	FLiveBPNodeOperationData NodeOp;
	NodeOp.Operation = ELiveBPNodeOperation::Add;
	NodeOp.NodeId = GetNodeGuid(Node);
	NodeOp.Position = FVector2D(Node->NodePosX, Node->NodePosY);
	NodeOp.NodeClass = Node->GetClass()->GetName();
	NodeOp.UserId = MUEIntegration->GetCurrentUserId();
	NodeOp.Timestamp = FPlatformTime::Seconds();

	// Send to other clients
	FGuid BlueprintId = GetBlueprintGuid(Blueprint);
	FGuid GraphId = GetGraphGuid(Node->GetGraph());
	MUEIntegration->SendNodeOperation(NodeOp, BlueprintId, GraphId);
}

void ULiveBPEditorSubsystem::OnNodeRemoved(UEdGraphNode* Node)
{
	if (!IsCollaborationEnabled() || !Node)
	{
		return;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
	if (!Blueprint)
	{
		return;
	}

	// Create node operation data
	FLiveBPNodeOperationData NodeOp;
	NodeOp.Operation = ELiveBPNodeOperation::Delete;
	NodeOp.NodeId = GetNodeGuid(Node);
	NodeOp.UserId = MUEIntegration->GetCurrentUserId();
	NodeOp.Timestamp = FPlatformTime::Seconds();

	// Send to other clients
	FGuid BlueprintId = GetBlueprintGuid(Blueprint);
	FGuid GraphId = GetGraphGuid(Node->GetGraph());
	MUEIntegration->SendNodeOperation(NodeOp, BlueprintId, GraphId);
	
	// Remove any locks for this node
	FGuid NodeId = GetNodeGuid(Node);
	NodeLocks.Remove(NodeId);
}

void ULiveBPEditorSubsystem::OnNodeMoved(UEdGraphNode* Node)
{
	if (!IsCollaborationEnabled() || !Node)
	{
		return;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
	if (!Blueprint)
	{
		return;
	}

	// Create node operation data
	FLiveBPNodeOperationData NodeOp;
	NodeOp.Operation = ELiveBPNodeOperation::Move;
	NodeOp.NodeId = GetNodeGuid(Node);
	NodeOp.Position = FVector2D(Node->NodePosX, Node->NodePosY);
	NodeOp.UserId = MUEIntegration->GetCurrentUserId();
	NodeOp.Timestamp = FPlatformTime::Seconds();

	// Send to other clients
	FGuid BlueprintId = GetBlueprintGuid(Blueprint);
	FGuid GraphId = GetGraphGuid(Node->GetGraph());
	MUEIntegration->SendNodeOperation(NodeOp, BlueprintId, GraphId);
}

void ULiveBPEditorSubsystem::OnPinConnected(UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
{
	if (!IsCollaborationEnabled() || !OutputPin || !InputPin)
	{
		return;
	}

	UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
	UEdGraphNode* InputNode = InputPin->GetOwningNode();
	
	if (!OutputNode || !InputNode)
	{
		return;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(OutputNode);
	if (!Blueprint)
	{
		return;
	}

	// Create node operation data
	FLiveBPNodeOperationData NodeOp;
	NodeOp.Operation = ELiveBPNodeOperation::PinConnect;
	NodeOp.NodeId = GetNodeGuid(OutputNode);
	NodeOp.TargetNodeId = GetNodeGuid(InputNode);
	NodeOp.PinName = OutputPin->PinName.ToString();
	NodeOp.TargetPinName = InputPin->PinName.ToString();
	NodeOp.UserId = MUEIntegration->GetCurrentUserId();
	NodeOp.Timestamp = FPlatformTime::Seconds();

	// Send to other clients
	FGuid BlueprintId = GetBlueprintGuid(Blueprint);
	FGuid GraphId = GetGraphGuid(OutputNode->GetGraph());
	MUEIntegration->SendNodeOperation(NodeOp, BlueprintId, GraphId);
}

void ULiveBPEditorSubsystem::OnPinDisconnected(UEdGraphPin* Pin)
{
	if (!IsCollaborationEnabled() || !Pin)
	{
		return;
	}

	UEdGraphNode* Node = Pin->GetOwningNode();
	if (!Node)
	{
		return;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
	if (!Blueprint)
	{
		return;
	}

	// Create node operation data
	FLiveBPNodeOperationData NodeOp;
	NodeOp.Operation = ELiveBPNodeOperation::PinDisconnect;
	NodeOp.NodeId = GetNodeGuid(Node);
	NodeOp.PinName = Pin->PinName.ToString();
	NodeOp.UserId = MUEIntegration->GetCurrentUserId();
	NodeOp.Timestamp = FPlatformTime::Seconds();

	// Send to other clients
	FGuid BlueprintId = GetBlueprintGuid(Blueprint);
	FGuid GraphId = GetGraphGuid(Node->GetGraph());
	MUEIntegration->SendNodeOperation(NodeOp, BlueprintId, GraphId);
}

// Wire preview handling
void ULiveBPEditorSubsystem::OnWireDragStart(const TSharedRef<SGraphEditor>& GraphEditor, UEdGraphNode* Node, const FString& PinName)
{
	// Wire drag started - we could start tracking this
}

void ULiveBPEditorSubsystem::OnWireDragUpdate(const TSharedRef<SGraphEditor>& GraphEditor, const FVector2D& Position)
{
	if (!IsCollaborationEnabled())
	{
		return;
	}

	// Throttle wire preview updates
	float CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastWirePreviewTime < WIRE_PREVIEW_THROTTLE)
	{
		return;
	}
	LastWirePreviewTime = CurrentTime;

	// Create and send wire preview
	// In a full implementation, we would get the actual drag data from the graph editor
	FLiveBPWirePreview WirePreview;
	WirePreview.EndPosition = Position;
	WirePreview.UserId = MUEIntegration->GetCurrentUserId();
	WirePreview.Timestamp = CurrentTime;

	// We would need to identify the Blueprint and Graph here
	// For now, this is a placeholder
}

void ULiveBPEditorSubsystem::OnWireDragEnd(const TSharedRef<SGraphEditor>& GraphEditor)
{
	// Wire drag ended
}

// Message handling
void ULiveBPEditorSubsystem::OnMUEMessageReceived(const FLiveBPMessage& Message)
{
	if (!IsCollaborationEnabled())
	{
		return;
	}

	// Process message based on type
	switch (Message.MessageType)
	{
		case ELiveBPMessageType::WirePreview:
			ProcessWirePreviewMessage(Message);
			break;
		case ELiveBPMessageType::NodeOperation:
			ProcessNodeOperationMessage(Message);
			break;
		case ELiveBPMessageType::LockRequest:
			ProcessLockMessage(Message);
			break;
		default:
			break;
	}
}

void ULiveBPEditorSubsystem::ProcessWirePreviewMessage(const FLiveBPMessage& Message)
{
	// Find the Blueprint and broadcast the wire preview
	UBlueprint* Blueprint = FindBlueprintByGuid(Message.BlueprintId);
	if (!Blueprint)
	{
		return;
	}

	// Deserialize wire preview data
	FLiveBPWirePreview WirePreview;
	// Implementation would deserialize from Message.PayloadData
	
	OnRemoteWirePreview.Broadcast(Blueprint, WirePreview, Message.UserId);
}

void ULiveBPEditorSubsystem::ProcessNodeOperationMessage(const FLiveBPMessage& Message)
{
	// Find the Blueprint and broadcast the node operation
	UBlueprint* Blueprint = FindBlueprintByGuid(Message.BlueprintId);
	if (!Blueprint)
	{
		return;
	}

	// Deserialize node operation data
	FLiveBPNodeOperationData NodeOperation;
	// Implementation would deserialize from Message.PayloadData
	
	OnRemoteNodeOperation.Broadcast(Blueprint, NodeOperation, Message.UserId);
}

void ULiveBPEditorSubsystem::ProcessLockMessage(const FLiveBPMessage& Message)  
{
	// Deserialize lock request
	// Implementation would deserialize from Message.PayloadData
	FLiveBPNodeLock LockRequest;
	// For now, create a placeholder
	
	// Update local lock state
	if (LockRequest.LockState == ELiveBPLockState::Locked)
	{
		NodeLocks.Add(LockRequest.NodeId, LockRequest);
	}
	else
	{
		NodeLocks.Remove(LockRequest.NodeId);
	}

	// Find and update visual state of the node
	UBlueprint* Blueprint = FindBlueprintByGuid(Message.BlueprintId);
	if (Blueprint)
	{
		UEdGraph* Graph = FindGraphByGuid(Blueprint, Message.GraphId);
		if (Graph)
		{
			UEdGraphNode* Node = FindNodeByGuid(Graph, LockRequest.NodeId);
			if (Node)
			{
				UpdateNodeVisualState(Node);
			}
		}
	}
}

// Utility functions
UBlueprint* ULiveBPEditorSubsystem::FindBlueprintByGuid(const FGuid& BlueprintId) const
{
	if (UBlueprint* const* Found = BlueprintGuidMap.Find(BlueprintId))
	{
		return *Found;
	}
	return nullptr;
}

UEdGraph* ULiveBPEditorSubsystem::FindGraphByGuid(UBlueprint* Blueprint, const FGuid& GraphId) const
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Simple implementation - in reality we'd need a better graph identification system
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (GetGraphGuid(Graph) == GraphId)
		{
			return Graph;
		}
	}

	return nullptr;
}

UEdGraphNode* ULiveBPEditorSubsystem::FindNodeByGuid(UEdGraph* Graph, const FGuid& NodeId) const
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (GetNodeGuid(Node) == NodeId)
		{
			return Node;
		}
	}

	return nullptr;
}

FGuid ULiveBPEditorSubsystem::GetBlueprintGuid(UBlueprint* Blueprint) const
{
	if (!Blueprint)
	{
		return FGuid();
	}

	// Use the Blueprint's package GUID for consistency across sessions
	if (UPackage* Package = Blueprint->GetPackage())
	{
		return Package->GetGuid();
	}
	
	// Fallback: generate based on asset path for consistency
	FString AssetPath = Blueprint->GetPathName();
	return FGuid::NewNameGuid(AssetPath);
}

FGuid ULiveBPEditorSubsystem::GetGraphGuid(UEdGraph* Graph) const
{
	if (!Graph)
	{
		return FGuid();
	}

	// Generate consistent GUID based on graph name and owning Blueprint
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FString GraphIdentifier = FString::Printf(TEXT("%s_%s"), *Blueprint->GetPathName(), *Graph->GetName());
		return FGuid::NewNameGuid(GraphIdentifier);
	}
	
	// Fallback
	return FGuid::NewNameGuid(Graph->GetPathName());
}

FGuid ULiveBPEditorSubsystem::GetNodeGuid(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return FGuid();
	}

	// Use the node's GUID if it has one and is valid
	if (Node->NodeGuid.IsValid())
	{
		return Node->NodeGuid;
	}
	
	// Generate a deterministic GUID based on node class and position for consistency
	// This ensures the same node gets the same GUID across different sessions
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
	if (Blueprint)
	{
		FString NodeIdentifier = FString::Printf(TEXT("%s_%s_%d_%d_%s"), 
			*Blueprint->GetPathName(), 
			*Node->GetClass()->GetName(),
			(int32)Node->NodePosX,
			(int32)Node->NodePosY,
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		
		// Generate deterministic GUID from the identifier string
		return FGuid::NewNameGuid(NodeIdentifier);
	}
	
	// Fallback: generate based on class and position only
	FString FallbackIdentifier = FString::Printf(TEXT("%s_%d_%d"), 
		*Node->GetClass()->GetName(),
		(int32)Node->NodePosX,
		(int32)Node->NodePosY);
	
	return FGuid::NewNameGuid(FallbackIdentifier);
}

void ULiveBPEditorSubsystem::UpdateNodeVisualState(UEdGraphNode* Node)
{
	if (!Node)
	{
		return;
	}

	// Update visual state based on lock status
	bool bIsLocked = IsNodeLockedByOther(Node);
	
	// In a full implementation, we would update the node's visual state
	// This might involve custom rendering or UI overlays
	
	UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Updated visual state for node %s (locked: %s)"), 
		*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), 
		bIsLocked ? TEXT("true") : TEXT("false"));
}

void ULiveBPEditorSubsystem::ShowCollaborationNotification(const FString& Message, float Duration)
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP Notification: %s"), *Message);

	// Show notification in the editor
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = Duration;
	Info.bFireAndForget = true;
	Info.bUseLargeFont = false;
	Info.bUseThrobber = false;

	FSlateNotificationManager::Get().AddNotification(Info);
}
