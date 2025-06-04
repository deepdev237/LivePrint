#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "IConcertSyncClient.h"
#include "ConcertSyncSessionTypes.h"
#include "LiveBPDataTypes.h"
#include "LiveBPMUEIntegration.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLiveBPMessageReceived, const FLiveBPMessage&, const FConcertSessionContext&);

UCLASS()
class LIVEBPCORE_API ULiveBPMUEIntegration : public UObject
{
	GENERATED_BODY()

public:
	ULiveBPMUEIntegration();

	// Initialize MUE integration
	bool Initialize();
	void Shutdown();

	// Message sending
	bool SendWirePreview(const FLiveBPWirePreview& WirePreview, const FGuid& BlueprintId, const FGuid& GraphId);
	bool SendNodeOperation(const FLiveBPNodeOperationData& NodeOperation, const FGuid& BlueprintId, const FGuid& GraphId);
	bool SendLockRequest(const FLiveBPNodeLock& LockRequest, const FGuid& BlueprintId, const FGuid& GraphId);

	// Message receiving delegate
	FOnLiveBPMessageReceived OnMessageReceived;

	// Connection status
	bool IsConnected() const;
	FString GetCurrentUserId() const;

private:
	// Concert client reference
	TSharedPtr<IConcertSyncClient> ConcertClient;

	// Message handlers
	void HandleConcertMessage(const FConcertSessionContext& Context, const FConcertReliableEvent& Event);
	void HandleLiveBPMessage(const FLiveBPMessage& Message, const FConcertSessionContext& Context);

	// Serialization helpers
	TArray<uint8> SerializeWirePreview(const FLiveBPWirePreview& WirePreview) const;
	TArray<uint8> SerializeNodeOperation(const FLiveBPNodeOperationData& NodeOperation) const;
	TArray<uint8> SerializeLockRequest(const FLiveBPNodeLock& LockRequest) const;

	bool DeserializeMessage(const TArray<uint8>& Data, ELiveBPMessageType MessageType, FLiveBPMessage& OutMessage) const;

	// Internal state
	bool bIsInitialized;
	FString CurrentUserId;
};
