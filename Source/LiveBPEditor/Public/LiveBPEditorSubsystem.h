#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/Blueprint.h"
#include "BlueprintGraph/Classes/K2Node.h"
#include "LiveBPDataTypes.h"
#include "LiveBPMUEIntegration.h"
#include "LiveBPLockManager.h"
#include "LiveBPNotificationSystem.h"
#include "LiveBPPerformanceMonitor.h"
#include "LiveBPEditorSubsystem.generated.h"

class SGraphEditor;
class UEdGraph;
class UEdGraphNode;
class FBlueprintEditor;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnRemoteWirePreview, UBlueprint*, const FLiveBPWirePreview&, const FString&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnRemoteNodeOperation, UBlueprint*, const FLiveBPNodeOperationData&, const FString&);

UCLASS()
class LIVEBPEDITOR_API ULiveBPEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	ULiveBPEditorSubsystem();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Main interface
	bool IsCollaborationEnabled() const { return bCollaborationEnabled; }
	void ToggleCollaboration();
	void EnableCollaboration();
	void DisableCollaboration();

	// Debug and development interface
	bool IsDebugModeEnabled() const { return bDebugModeEnabled; }
	void SetDebugModeEnabled(bool bEnabled) { bDebugModeEnabled = bEnabled; }
	
	// Access to components (for console commands)
	FLiveBPMUEIntegration* GetMUEIntegration() const { return MUEIntegration.Get(); }
	FLiveBPLockManager* GetLockManager() const { return LockManager.Get(); }
	FLiveBPPerformanceMonitor* GetPerformanceMonitor() const { return PerformanceMonitor.Get(); }

	// Node locking
	bool RequestNodeLock(UEdGraphNode* Node, float LockDuration = 30.0f);
	bool ReleaseNodeLock(UEdGraphNode* Node);
	bool IsNodeLockedByOther(UEdGraphNode* Node) const;
	bool CanModifyNode(UEdGraphNode* Node) const;

	// Events
	FOnRemoteWirePreview OnRemoteWirePreview;
	FOnRemoteNodeOperation OnRemoteNodeOperation;

private:
	// Core components
	UPROPERTY()
	TObjectPtr<ULiveBPMUEIntegration> MUEIntegration;

	UPROPERTY()
	TObjectPtr<ULiveBPLockManager> LockManager;

	UPROPERTY()
	TObjectPtr<ULiveBPNotificationSystem> NotificationSystem;

	UPROPERTY()
	TObjectPtr<FLiveBPPerformanceMonitor> PerformanceMonitor;

	// State
	bool bCollaborationEnabled;
	bool bDebugModeEnabled;
	TMap<UBlueprint*, TWeakPtr<SGraphEditor>> TrackedGraphEditors;
	TMap<UBlueprint*, FDelegateHandle> BlueprintDelegateHandles;
	TMap<FGuid, UBlueprint*> BlueprintGuidMap;
	FTimerHandle LockUpdateTimer;

	// Wire preview throttling
	float LastWirePreviewTime;
	static constexpr float WIRE_PREVIEW_THROTTLE = 0.1f; // 10Hz

	// Blueprint editor integration
	void RegisterBlueprintCallbacks();
	void UnregisterBlueprintCallbacks();
	
	void OnAssetOpened(UObject* Asset, IAssetEditorInstance* EditorInstance);
	void OnAssetClosed(UObject* Asset, IAssetEditorInstance* EditorInstance);
	
	void OnBlueprintPreCompile(UBlueprint* Blueprint);
	void OnBlueprintCompiled(UBlueprint* Blueprint);
	
	// Graph editor hooks
	void RegisterGraphEditorCallbacks(UBlueprint* Blueprint);
	void UnregisterGraphEditorCallbacks(UBlueprint* Blueprint);
	
	void OnNodeAdded(UEdGraphNode* Node);
	void OnNodeRemoved(UEdGraphNode* Node);
	void OnNodeMoved(UEdGraphNode* Node);
	void OnPinConnected(class UEdGraphPin* OutputPin, class UEdGraphPin* InputPin);
	void OnPinDisconnected(class UEdGraphPin* Pin);
	
	// Wire preview handling
	void OnWireDragStart(const TSharedRef<SGraphEditor>& GraphEditor, UEdGraphNode* Node, const FString& PinName);
	void OnWireDragUpdate(const TSharedRef<SGraphEditor>& GraphEditor, const FVector2D& Position);
	void OnWireDragEnd(const TSharedRef<SGraphEditor>& GraphEditor);
	
	// Message handling
	void OnMUEMessageReceived(const FLiveBPMessage& Message, const FConcertSessionContext& Context);
	void ProcessWirePreviewMessage(const FLiveBPMessage& Message);
	void ProcessNodeOperationMessage(const FLiveBPMessage& Message);
	void ProcessLockMessage(const FLiveBPMessage& Message);
	
	// Utility functions
	UBlueprint* FindBlueprintByGuid(const FGuid& BlueprintId) const;
	UEdGraph* FindGraphByGuid(UBlueprint* Blueprint, const FGuid& GraphId) const;
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FGuid& NodeId) const;
	FGuid GetBlueprintGuid(UBlueprint* Blueprint) const;
	FGuid GetGraphGuid(UEdGraph* Graph) const;
	FGuid GetNodeGuid(UEdGraphNode* Node) const;
	
	// Visual feedback
	void UpdateNodeVisualState(UEdGraphNode* Node);
	void ShowCollaborationNotification(const FString& Message, float Duration = 3.0f);
};
