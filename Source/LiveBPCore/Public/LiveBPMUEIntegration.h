#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LiveBPDataTypes.h"
#include "Subsystems/EditorSubsystem.h"
#include "IConcertSyncClientModule.h"
#include "IConcertSyncClient.h"
#include "IConcertClientSession.h"
#include "IConcertSession.h"
#include "LiveBPMUEIntegration.generated.h"

// Concert-based delegate for message receiving
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLiveBPMessageReceived, const FLiveBPMessage&);

UCLASS()
class LIVEBPCORE_API ULiveBPMUEIntegration : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	ULiveBPMUEIntegration();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Concert integration
	bool InitializeConcertIntegration();
	void ShutdownConcertIntegration();

	// Message sending via Concert
	bool SendWirePreview(const FLiveBPWirePreview& WirePreview, const FGuid& BlueprintId, const FGuid& GraphId);
	bool SendNodeOperation(const FLiveBPNodeOperationData& NodeOperation, const FGuid& BlueprintId, const FGuid& GraphId);
	bool SendLockRequest(const FLiveBPNodeLock& LockRequest, const FGuid& BlueprintId, const FGuid& GraphId);

	// Message receiving delegate
	FOnLiveBPMessageReceived OnMessageReceived;

	// Session status
	bool IsConnected() const;
	bool HasActiveSession() const;
	FString GetCurrentUserId() const;
	TArray<FString> GetConnectedUsers() const;

private:
	// Concert client references
	IConcertSyncClient* ConcertSyncClient;
	TSharedPtr<IConcertClientSession> ActiveSession;

	// Custom event types for LiveBP collaboration
	static const FString LiveBPWirePreviewChannel;
	static const FString LiveBPNodeOperationChannel;
	static const FString LiveBPLockRequestChannel;

	// Concert event handlers
	void OnCustomEventReceived(const FConcertSessionContext& Context, const struct FLiveBPConcertEvent& Event);
	void OnSessionStartup(TSharedRef<IConcertClientSession> InSession);
	void OnSessionShutdown(TSharedRef<IConcertClientSession> InSession);

	// Serialization helpers for Concert messages
	TArray<uint8> SerializeWirePreview(const FLiveBPWirePreview& WirePreview) const;
	TArray<uint8> SerializeNodeOperation(const FLiveBPNodeOperationData& NodeOperation) const;
	TArray<uint8> SerializeLockRequest(const FLiveBPNodeLock& LockRequest) const;

	bool DeserializeMessage(const FString& Channel, const TArray<uint8>& Data, FLiveBPMessage& OutMessage) const;
	bool SendCustomEvent(const FString& Channel, const TArray<uint8>& EventData);

	// Internal state
	bool bIsInitialized;
	FString CurrentUserId;
};
