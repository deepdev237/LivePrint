#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LiveBPDataTypes.h"
#include "LiveBPLockManager.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNodeLockStateChanged, const FGuid&, const FLiveBPNodeLock&);

UCLASS()
class LIVEBPCORE_API ULiveBPLockManager : public UObject
{
	GENERATED_BODY()

public:
	ULiveBPLockManager();

	// Lock management
	bool RequestLock(const FGuid& NodeId, const FString& UserId, float LockDuration = 30.0f);
	bool ReleaseLock(const FGuid& NodeId, const FString& UserId);
	bool IsLocked(const FGuid& NodeId) const;
	bool IsLockedByUser(const FGuid& NodeId, const FString& UserId) const;
	bool CanUserModify(const FGuid& NodeId, const FString& UserId) const;

	// Lock state queries
	ELiveBPLockState GetLockState(const FGuid& NodeId) const;
	FString GetLockOwner(const FGuid& NodeId) const;
	float GetLockTimeRemaining(const FGuid& NodeId) const;

	// Remote lock handling
	void HandleRemoteLockRequest(const FLiveBPNodeLock& LockRequest);
	void HandleRemoteLockRelease(const FLiveBPNodeLock& LockRelease);

	// Maintenance
	void UpdateLocks(float DeltaTime);
	void ClearAllLocks();
	void ClearUserLocks(const FString& UserId);

	// Events
	FOnNodeLockStateChanged OnNodeLockStateChanged;

private:
	// Lock storage
	UPROPERTY()
	TMap<FGuid, FLiveBPNodeLock> NodeLocks;

	// Pending lock requests (for conflict resolution)
	UPROPERTY()
	TMap<FGuid, TArray<FLiveBPNodeLock>> PendingLockRequests;

	// Helper functions
	void ProcessPendingRequests(const FGuid& NodeId);
	bool IsLockExpired(const FLiveBPNodeLock& Lock) const;
	void ExpireLock(const FGuid& NodeId);
	void GrantLock(const FGuid& NodeId, const FLiveBPNodeLock& LockRequest);

	// Constants
	static constexpr float DEFAULT_LOCK_DURATION = 30.0f;
	static constexpr float LOCK_EXTENSION_TIME = 5.0f;
};
