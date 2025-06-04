#include "LiveBPEditorSubsystem.h"
#include "LiveBPEditor.h"
#include "LiveBPNotificationSystem.h"
#include "LiveBPPerformanceMonitor.h"
#include "AssetEditorManager.h"
#include "BlueprintEditorModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "BlueprintGraph/Classes/K2Node.h"
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
#include "TimerManager.h"

ULiveBPEditorSubsystem::ULiveBPEditorSubsystem()
	: bCollaborationEnabled(false)
	, LastWirePreviewTime(0.0f)
{
}

void ULiveBPEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogLiveBPEditor, Log, TEXT("Initializing LiveBP Editor Subsystem"));

	// Create core components
	MUEIntegration = NewObject<ULiveBPMUEIntegration>(this);
	LockManager = NewObject<ULiveBPLockManager>(this);
	NotificationSystem = NewObject<ULiveBPNotificationSystem>(this);

	// Bind delegates
	MUEIntegration->OnMessageReceived.AddUObject(this, &ULiveBPEditorSubsystem::OnMUEMessageReceived);
	LockManager->OnNodeLockStateChanged.AddLambda([this](const FGuid& NodeId, const FLiveBPNodeLock& Lock)
	{
		// Show notification for lock state change
		if (Lock.LockState == ELiveBPLockState::Locked && Lock.UserId != MUEIntegration->GetCurrentUserId())
		{
			NotificationSystem->ShowNodeLockedNotification(Lock.UserId, Lock.UserId, NodeId);
		}
		
		// Find and update visual state of locked nodes
		for (const auto& TrackedPair : TrackedGraphEditors)
		{
			UBlueprint* Blueprint = TrackedPair.Key;
			if (Blueprint)
			{
				for (UEdGraph* Graph : Blueprint->UbergraphPages)
				{
					if (UEdGraphNode* Node = FindNodeByGuid(Graph, NodeId))
					{
						UpdateNodeVisualState(Node);
						break;
					}
				}
			}
		}
	});

	RegisterBlueprintCallbacks();
}

void ULiveBPEditorSubsystem::Deinitialize()
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("Deinitializing LiveBP Editor Subsystem"));

	DisableCollaboration();
	UnregisterBlueprintCallbacks();

	if (MUEIntegration)
	{
		MUEIntegration->Shutdown();
		MUEIntegration = nullptr;
	}

	LockManager = nullptr;

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

	UE_LOG(LogLiveBPEditor, Log, TEXT("Enabling Blueprint collaboration"));

	if (MUEIntegration && MUEIntegration->Initialize())
	{
		bCollaborationEnabled = true;
		ShowCollaborationNotification(TEXT("Blueprint collaboration enabled"));
		
		// Start the lock update timer
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			World->GetTimerManager().SetTimer(LockUpdateTimer, [this]()
			{
				if (LockManager)
				{
					LockManager->UpdateLocks(0.1f); // Update every 100ms
				}
			}, 0.1f, true);
		}
	}
	else
	{
		ShowCollaborationNotification(TEXT("Failed to enable Blueprint collaboration - Multi-User Editing not available"));
	}
}

void ULiveBPEditorSubsystem::DisableCollaboration()
{
	if (!bCollaborationEnabled)
	{
		return;
	}

	UE_LOG(LogLiveBPEditor, Log, TEXT("Disabling Blueprint collaboration"));

	bCollaborationEnabled = false;
	
	// Clear the timer
	if (UWorld* World = GEditor->GetEditorWorldContext().World())
	{
		World->GetTimerManager().ClearTimer(LockUpdateTimer);
	}
	
	if (MUEIntegration)
	{
		MUEIntegration->Shutdown();
	}

	if (LockManager)
	{
		LockManager->ClearAllLocks();
	}

	ShowCollaborationNotification(TEXT("Blueprint collaboration disabled"));
}

bool ULiveBPEditorSubsystem::RequestNodeLock(UEdGraphNode* Node, float LockDuration)
{
	if (!bCollaborationEnabled || !Node || !LockManager || !MUEIntegration)
	{
		return false;
	}

	FGuid NodeId = GetNodeGuid(Node);
	FString UserId = MUEIntegration->GetCurrentUserId();

	if (LockManager->RequestLock(NodeId, UserId, LockDuration))
	{
		// Send lock request to other clients
		FLiveBPNodeLock LockRequest;
		LockRequest.NodeId = NodeId;
		LockRequest.UserId = UserId;
		LockRequest.LockState = ELiveBPLockState::Locked;
		LockRequest.LockTime = FPlatformTime::Seconds();
		LockRequest.ExpiryTime = LockRequest.LockTime + LockDuration;

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
		if (Blueprint)
		{
			FGuid BlueprintId = GetBlueprintGuid(Blueprint);
			FGuid GraphId = GetGraphGuid(Node->GetGraph());
			MUEIntegration->SendLockRequest(LockRequest, BlueprintId, GraphId);
		}

		UpdateNodeVisualState(Node);
		return true;
	}

	return false;
}

bool ULiveBPEditorSubsystem::ReleaseNodeLock(UEdGraphNode* Node)
{
	if (!bCollaborationEnabled || !Node || !LockManager || !MUEIntegration)
	{
		return false;
	}

	FGuid NodeId = GetNodeGuid(Node);
	FString UserId = MUEIntegration->GetCurrentUserId();

	if (LockManager->ReleaseLock(NodeId, UserId))
	{
		// Send unlock request to other clients
		FLiveBPNodeLock UnlockRequest;
		UnlockRequest.NodeId = NodeId;
		UnlockRequest.UserId = UserId;
		UnlockRequest.LockState = ELiveBPLockState::Unlocked;
		UnlockRequest.LockTime = FPlatformTime::Seconds();

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
		if (Blueprint)
		{
			FGuid BlueprintId = GetBlueprintGuid(Blueprint);
			FGuid GraphId = GetGraphGuid(Node->GetGraph());
			MUEIntegration->SendLockRequest(UnlockRequest, BlueprintId, GraphId);
		}

		UpdateNodeVisualState(Node);
		return true;
	}

	return false;
}

bool ULiveBPEditorSubsystem::IsNodeLockedByOther(UEdGraphNode* Node) const
{
	if (!bCollaborationEnabled || !Node || !LockManager || !MUEIntegration)
	{
		return false;
	}

	FGuid NodeId = GetNodeGuid(Node);
	FString UserId = MUEIntegration->GetCurrentUserId();

	return LockManager->IsLocked(NodeId) && !LockManager->IsLockedByUser(NodeId, UserId);
}

bool ULiveBPEditorSubsystem::CanModifyNode(UEdGraphNode* Node) const
{
	if (!bCollaborationEnabled || !Node || !LockManager || !MUEIntegration)
	{
		return true; // If collaboration is disabled, allow all modifications
	}

	FGuid NodeId = GetNodeGuid(Node);
	FString UserId = MUEIntegration->GetCurrentUserId();

	return LockManager->CanUserModify(NodeId, UserId);
}

void ULiveBPEditorSubsystem::RegisterBlueprintCallbacks()
{
	// Register for asset editor events
	FAssetEditorManager::Get().OnAssetOpenedInEditor().AddUObject(this, &ULiveBPEditorSubsystem::OnAssetOpened);
	FAssetEditorManager::Get().OnAssetEditorRequestedClose().AddUObject(this, &ULiveBPEditorSubsystem::OnAssetClosed);

	// Register for Blueprint compilation events
	if (FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("BlueprintEditorModule"))
	{
		BlueprintEditorModule->OnBlueprintPreCompile().AddUObject(this, &ULiveBPEditorSubsystem::OnBlueprintPreCompile);
		BlueprintEditorModule->OnBlueprintCompiled().AddUObject(this, &ULiveBPEditorSubsystem::OnBlueprintCompiled);
	}
}

void ULiveBPEditorSubsystem::UnregisterBlueprintCallbacks()
{
	// Unregister all blueprint-specific callbacks
	for (auto& HandlePair : BlueprintDelegateHandles)
	{
		UnregisterGraphEditorCallbacks(HandlePair.Key);
	}
	BlueprintDelegateHandles.Empty();
	TrackedGraphEditors.Empty();

	// Unregister asset editor events
	FAssetEditorManager::Get().OnAssetOpenedInEditor().RemoveAll(this);
	FAssetEditorManager::Get().OnAssetEditorRequestedClose().RemoveAll(this);

	// Unregister Blueprint compilation events
	if (FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("BlueprintEditorModule"))
	{
		BlueprintEditorModule->OnBlueprintPreCompile().RemoveAll(this);
		BlueprintEditorModule->OnBlueprintCompiled().RemoveAll(this);
	}
}

void ULiveBPEditorSubsystem::OnAssetOpened(UObject* Asset, IAssetEditorInstance* EditorInstance)
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		UE_LOG(LogLiveBPEditor, Log, TEXT("Blueprint opened: %s"), *Blueprint->GetName());
		RegisterGraphEditorCallbacks(Blueprint);
		
		// Store blueprint GUID mapping for later lookup
		BlueprintGuidMap.Add(GetBlueprintGuid(Blueprint), Blueprint);
	}
}

void ULiveBPEditorSubsystem::OnAssetClosed(UObject* Asset, IAssetEditorInstance* EditorInstance)
{
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		UE_LOG(LogLiveBPEditor, Log, TEXT("Blueprint closed: %s"), *Blueprint->GetName());
		UnregisterGraphEditorCallbacks(Blueprint);
		
		// Remove from GUID mapping
		FGuid BlueprintId = GetBlueprintGuid(Blueprint);
		BlueprintGuidMap.Remove(BlueprintId);
		
		// Release all locks for this user on this blueprint
		if (bCollaborationEnabled && LockManager && MUEIntegration)
		{
			FString UserId = MUEIntegration->GetCurrentUserId();
			// In a full implementation, you'd iterate through all nodes in the blueprint
			// and release locks owned by this user
		}
	}
}

void ULiveBPEditorSubsystem::OnBlueprintPreCompile(UBlueprint* Blueprint)
{
	// Release all locks for this blueprint before compilation
	if (bCollaborationEnabled && LockManager)
	{
		FString UserId = MUEIntegration ? MUEIntegration->GetCurrentUserId() : FString();
		if (!UserId.IsEmpty())
		{
			UE_LOG(LogLiveBPEditor, Log, TEXT("Blueprint pre-compile: %s"), *Blueprint->GetName());
			// In a full implementation, release locks for all nodes in this blueprint
		}
	}
}

void ULiveBPEditorSubsystem::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("Blueprint compiled: %s"), *Blueprint->GetName());
	
	// After compilation, update all node visual states
	if (bCollaborationEnabled)
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UpdateNodeVisualState(Node);
			}
		}
	}
}

void ULiveBPEditorSubsystem::RegisterGraphEditorCallbacks(UBlueprint* Blueprint)
{
	if (!Blueprint || BlueprintDelegateHandles.Contains(Blueprint))
	{
		return;
	}

	// Register for graph structure changes
	FDelegateHandle Handle = Blueprint->OnChanged().AddLambda([this, Blueprint]()
	{
		// Handle blueprint changes that might affect collaboration
		if (bCollaborationEnabled)
		{
			UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Blueprint changed: %s"), *Blueprint->GetName());
		}
	});
	
	BlueprintDelegateHandles.Add(Blueprint, Handle);

	// Note: In a complete implementation, you would need to hook into the SGraphEditor widget
	// to capture wire drag events and other real-time interactions. This requires:
	// 1. Custom SGraphEditor subclass or widget extensions
	// 2. Override mouse event handlers for wire dragging
	// 3. Custom rendering for remote user cursors and wire previews
	// 4. Integration with the Blueprint editor's command system
}

void ULiveBPEditorSubsystem::UnregisterGraphEditorCallbacks(UBlueprint* Blueprint)
{
	if (FDelegateHandle* Handle = BlueprintDelegateHandles.Find(Blueprint))
	{
		Blueprint->OnChanged().Remove(*Handle);
		BlueprintDelegateHandles.Remove(Blueprint);
	}
	TrackedGraphEditors.Remove(Blueprint);
}

void ULiveBPEditorSubsystem::OnMUEMessageReceived(const FLiveBPMessage& Message, const FConcertSessionContext& Context)
{
	if (!bCollaborationEnabled)
	{
		return;
	}

	switch (Message.MessageType)
	{
	case ELiveBPMessageType::WirePreview:
		ProcessWirePreviewMessage(Message);
		break;
	case ELiveBPMessageType::NodeOperation:
		ProcessNodeOperationMessage(Message);
		break;
	case ELiveBPMessageType::LockRequest:
	case ELiveBPMessageType::LockRelease:
		ProcessLockMessage(Message);
		break;
	default:
		break;
	}
}

void ULiveBPEditorSubsystem::ProcessWirePreviewMessage(const FLiveBPMessage& Message)
{
	// Deserialize wire preview data
	FLiveBPWirePreview WirePreview;
	FMemoryReader Reader(Message.PayloadData);
	Reader << WirePreview.NodeId;
	Reader << WirePreview.PinName;
	Reader << WirePreview.StartPosition;
	Reader << WirePreview.EndPosition;
	Reader << WirePreview.UserId;
	Reader << WirePreview.Timestamp;

	// Find the target blueprint
	UBlueprint* Blueprint = FindBlueprintByGuid(Message.BlueprintId);
	if (Blueprint)
	{
		OnRemoteWirePreview.Broadcast(Blueprint, WirePreview, Message.UserId);
		UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Processed wire preview from user %s"), *Message.UserId);
	}
}

void ULiveBPEditorSubsystem::ProcessNodeOperationMessage(const FLiveBPMessage& Message)
{
	// Deserialize node operation data from JSON
	FString JsonString;
	JsonString.AppendChars(reinterpret_cast<const TCHAR*>(Message.PayloadData.GetData()), Message.PayloadData.Num() / sizeof(TCHAR));
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FLiveBPNodeOperationData NodeOperation;
		NodeOperation.Operation = static_cast<ELiveBPNodeOperation>(JsonObject->GetIntegerField(TEXT("Operation")));
		FGuid::Parse(JsonObject->GetStringField(TEXT("NodeId")), NodeOperation.NodeId);
		FGuid::Parse(JsonObject->GetStringField(TEXT("TargetNodeId")), NodeOperation.TargetNodeId);
		NodeOperation.PinName = JsonObject->GetStringField(TEXT("PinName"));
		NodeOperation.TargetPinName = JsonObject->GetStringField(TEXT("TargetPinName"));
		NodeOperation.Position.X = JsonObject->GetNumberField(TEXT("PositionX"));
		NodeOperation.Position.Y = JsonObject->GetNumberField(TEXT("PositionY"));
		NodeOperation.NodeClass = JsonObject->GetStringField(TEXT("NodeClass"));
		NodeOperation.PropertyData = JsonObject->GetStringField(TEXT("PropertyData"));
		NodeOperation.UserId = JsonObject->GetStringField(TEXT("UserId"));
		NodeOperation.Timestamp = JsonObject->GetNumberField(TEXT("Timestamp"));

		UBlueprint* Blueprint = FindBlueprintByGuid(Message.BlueprintId);
		if (Blueprint)
		{
			OnRemoteNodeOperation.Broadcast(Blueprint, NodeOperation, Message.UserId);
			UE_LOG(LogLiveBPEditor, Log, TEXT("Processed node operation %d from user %s"), 
				static_cast<int32>(NodeOperation.Operation), *Message.UserId);
		}
	}
}

void ULiveBPEditorSubsystem::ProcessLockMessage(const FLiveBPMessage& Message)
{
	// Deserialize lock data from JSON
	FString JsonString;
	JsonString.AppendChars(reinterpret_cast<const TCHAR*>(Message.PayloadData.GetData()), Message.PayloadData.Num() / sizeof(TCHAR));
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FLiveBPNodeLock LockRequest;
		FGuid::Parse(JsonObject->GetStringField(TEXT("NodeId")), LockRequest.NodeId);
		LockRequest.LockState = static_cast<ELiveBPLockState>(JsonObject->GetIntegerField(TEXT("LockState")));
		LockRequest.UserId = JsonObject->GetStringField(TEXT("UserId"));
		LockRequest.LockTime = JsonObject->GetNumberField(TEXT("LockTime"));
		LockRequest.ExpiryTime = JsonObject->GetNumberField(TEXT("ExpiryTime"));

		if (LockManager)
		{
			if (Message.MessageType == ELiveBPMessageType::LockRequest)
			{
				LockManager->HandleRemoteLockRequest(LockRequest);
			}
			else
			{
				LockManager->HandleRemoteLockRelease(LockRequest);
			}
			
			UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Processed lock message from user %s for node %s"), 
				*LockRequest.UserId, *LockRequest.NodeId.ToString());
		}
	}
}

UBlueprint* ULiveBPEditorSubsystem::FindBlueprintByGuid(const FGuid& BlueprintId) const
{
	if (UBlueprint* const* FoundBlueprint = BlueprintGuidMap.Find(BlueprintId))
	{
		return *FoundBlueprint;
	}
	
	UE_LOG(LogLiveBPEditor, Warning, TEXT("Blueprint not found for GUID: %s"), *BlueprintId.ToString());
	return nullptr;
}

UEdGraph* ULiveBPEditorSubsystem::FindGraphByGuid(UBlueprint* Blueprint, const FGuid& GraphId) const
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Search through all graphs in the blueprint
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (GetGraphGuid(Graph) == GraphId)
		{
			return Graph;
		}
	}

	// Check function graphs as well
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
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

	// Use the blueprint's package name as a basis for a consistent GUID
	// In a production system, you might store this mapping in project settings
	FString PackageName = Blueprint->GetPackage()->GetName();
	return FGuid::NewNameGuid(PackageName);
}

FGuid ULiveBPEditorSubsystem::GetGraphGuid(UEdGraph* Graph) const
{
	if (!Graph)
	{
		return FGuid();
	}

	// Use the graph's existing GUID
	return Graph->GraphGuid;
}

FGuid ULiveBPEditorSubsystem::GetNodeGuid(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return FGuid();
	}

	// Use the node's existing GUID
	return Node->NodeGuid;
}

void ULiveBPEditorSubsystem::UpdateNodeVisualState(UEdGraphNode* Node)
{
	if (!Node || !LockManager)
	{
		return;
	}

	FGuid NodeId = GetNodeGuid(Node);
	ELiveBPLockState LockState = LockManager->GetLockState(NodeId);
	FString LockOwner = LockManager->GetLockOwner(NodeId);

	// In a complete implementation, this would:
	// 1. Update the node's visual appearance (border color, icon overlay, etc.)
	// 2. Add tooltips showing lock information
	// 3. Disable/enable interaction based on lock state
	// 4. Show collaborator cursors and activity indicators

	switch (LockState)
	{
	case ELiveBPLockState::Locked:
		if (!LockOwner.IsEmpty())
		{
			UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Node %s is locked by %s"), 
				*Node->GetName(), *LockOwner);
		}
		break;
	case ELiveBPLockState::Pending:
		UE_LOG(LogLiveBPEditor, VeryVerbose, TEXT("Node %s has pending lock requests"), 
			*Node->GetName());
		break;
	case ELiveBPLockState::Unlocked:
	default:
		// Node is available for editing
		break;
	}
}

void ULiveBPEditorSubsystem::ShowCollaborationNotification(const FString& Message, float Duration)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = Duration;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = true;
	Info.bUseLargeFont = false;
	Info.bFireAndForget = true;

	FSlateNotificationManager::Get().AddNotification(Info);
}
