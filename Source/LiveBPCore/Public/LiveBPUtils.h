#pragma once

#include "CoreMinimal.h"
#include "LiveBPDataTypes.h"

/**
 * Utility functions for Live Blueprint collaboration
 */
class LIVEBPCORE_API FLiveBPUtils
{
public:
	// Message serialization helpers
	static TArray<uint8> SerializeToJson(const FLiveBPNodeOperationData& NodeOperation);
	static TArray<uint8> SerializeToJson(const FLiveBPNodeLock& NodeLock);
	static bool DeserializeFromJson(const TArray<uint8>& Data, FLiveBPNodeOperationData& OutNodeOperation);
	static bool DeserializeFromJson(const TArray<uint8>& Data, FLiveBPNodeLock& OutNodeLock);

	// Binary serialization for wire previews
	static TArray<uint8> SerializeToBinary(const FLiveBPWirePreview& WirePreview);
	static bool DeserializeFromBinary(const TArray<uint8>& Data, FLiveBPWirePreview& OutWirePreview);

	// Validation helpers
	static bool IsValidMessage(const FLiveBPMessage& Message);
	static bool IsValidNodeOperation(const FLiveBPNodeOperationData& NodeOperation);
	static bool IsValidWirePreview(const FLiveBPWirePreview& WirePreview);
	static bool IsValidNodeLock(const FLiveBPNodeLock& NodeLock);

	// Performance helpers
	static bool ShouldThrottleMessage(ELiveBPMessageType MessageType, float LastSentTime, float CurrentTime);
	static float GetThrottleInterval(ELiveBPMessageType MessageType);

	// String conversion helpers
	static FString NodeOperationToString(ELiveBPNodeOperation Operation);
	static FString LockStateToString(ELiveBPLockState LockState);
	static FString MessageTypeToString(ELiveBPMessageType MessageType);

	// Hash functions for efficient lookups
	static uint32 GetNodeOperationHash(const FLiveBPNodeOperationData& NodeOperation);
	static uint32 GetWirePreviewHash(const FLiveBPWirePreview& WirePreview);

	// Distance and proximity calculations
	static float CalculateDistance2D(const FVector2D& A, const FVector2D& B);
	static bool ArePositionsNearby(const FVector2D& A, const FVector2D& B, float Threshold = 5.0f);

	// Time utilities
	static float GetCurrentTimestamp() { return FPlatformTime::Seconds(); }
	static bool IsTimestampRecent(float Timestamp, float MaxAge = 5.0f);

private:
	// Internal JSON helpers
	static TSharedPtr<class FJsonObject> NodeOperationToJson(const FLiveBPNodeOperationData& NodeOperation);
	static TSharedPtr<class FJsonObject> NodeLockToJson(const FLiveBPNodeLock& NodeLock);
	static bool JsonToNodeOperation(const TSharedPtr<class FJsonObject>& JsonObject, FLiveBPNodeOperationData& OutNodeOperation);
	static bool JsonToNodeLock(const TSharedPtr<class FJsonObject>& JsonObject, FLiveBPNodeLock& OutNodeLock);
};
