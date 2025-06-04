#include "LiveBPLockManager.h"
#include "LiveBPCore.h"

ULiveBPLockManager::ULiveBPLockManager()
{
}

bool ULiveBPLockManager::RequestLock(const FGuid& NodeId, const FString& UserId, float LockDuration)
{
	if (NodeId.IsValid() == false || UserId.IsEmpty())
	{
		return false;
	}

	const float CurrentTime = FPlatformTime::Seconds();
	
	// Check if node is already locked
	if (FLiveBPNodeLock* ExistingLock = NodeLocks.Find(NodeId))
	{
		// If expired, we can take it
		if (IsLockExpired(*ExistingLock))
		{
			ExpireLock(NodeId);
		}
		// If locked by same user, extend the lock
		else if (ExistingLock->UserId == UserId)
		{
			ExistingLock->ExpiryTime = CurrentTime + LockDuration;
			OnNodeLockStateChanged.Broadcast(NodeId, *ExistingLock);
			return true;
		}
		// Otherwise, add to pending requests
		else
		{
			FLiveBPNodeLock PendingRequest;
			PendingRequest.NodeId = NodeId;
			PendingRequest.UserId = UserId;
			PendingRequest.LockState = ELiveBPLockState::Pending;
			PendingRequest.LockTime = CurrentTime;
			PendingRequest.ExpiryTime = CurrentTime + LockDuration;

			PendingLockRequests.FindOrAdd(NodeId).Add(PendingRequest);
			return false; // Request is pending
		}
	}

	// Grant the lock
	FLiveBPNodeLock NewLock;
	NewLock.NodeId = NodeId;
	NewLock.UserId = UserId;
	NewLock.LockState = ELiveBPLockState::Locked;
	NewLock.LockTime = CurrentTime;
	NewLock.ExpiryTime = CurrentTime + LockDuration;

	GrantLock(NodeId, NewLock);
	return true;
}

bool ULiveBPLockManager::ReleaseLock(const FGuid& NodeId, const FString& UserId)
{
	FLiveBPNodeLock* ExistingLock = NodeLocks.Find(NodeId);
	if (!ExistingLock || ExistingLock->UserId != UserId)
	{
		return false;
	}

	NodeLocks.Remove(NodeId);
	
	// Notify about lock release
	FLiveBPNodeLock ReleasedLock = *ExistingLock;
	ReleasedLock.LockState = ELiveBPLockState::Unlocked;
	OnNodeLockStateChanged.Broadcast(NodeId, ReleasedLock);

	// Process pending requests
	ProcessPendingRequests(NodeId);
	
	return true;
}

bool ULiveBPLockManager::IsLocked(const FGuid& NodeId) const
{
	const FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId);
	return Lock && !IsLockExpired(*Lock);
}

bool ULiveBPLockManager::IsLockedByUser(const FGuid& NodeId, const FString& UserId) const
{
	const FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId);
	return Lock && Lock->UserId == UserId && !IsLockExpired(*Lock);
}

bool ULiveBPLockManager::CanUserModify(const FGuid& NodeId, const FString& UserId) const
{
	return !IsLocked(NodeId) || IsLockedByUser(NodeId, UserId);
}

ELiveBPLockState ULiveBPLockManager::GetLockState(const FGuid& NodeId) const
{
	const FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId);
	if (!Lock)
	{
		// Check if there are pending requests
		const TArray<FLiveBPNodeLock>* PendingRequests = PendingLockRequests.Find(NodeId);
		if (PendingRequests && PendingRequests->Num() > 0)
		{
			return ELiveBPLockState::Pending;
		}
		return ELiveBPLockState::Unlocked;
	}

	return IsLockExpired(*Lock) ? ELiveBPLockState::Unlocked : Lock->LockState;
}

FString ULiveBPLockManager::GetLockOwner(const FGuid& NodeId) const
{
	const FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId);
	return (Lock && !IsLockExpired(*Lock)) ? Lock->UserId : FString();
}

float ULiveBPLockManager::GetLockTimeRemaining(const FGuid& NodeId) const
{
	const FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId);
	if (!Lock || IsLockExpired(*Lock))
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, Lock->ExpiryTime - FPlatformTime::Seconds());
}

void ULiveBPLockManager::HandleRemoteLockRequest(const FLiveBPNodeLock& LockRequest)
{
	if (LockRequest.LockState == ELiveBPLockState::Locked)
	{
		// Try to grant the remote lock request
		if (!IsLocked(LockRequest.NodeId))
		{
			GrantLock(LockRequest.NodeId, LockRequest);
		}
		else
		{
			// Add to pending if not already locked by same user
			const FLiveBPNodeLock* ExistingLock = NodeLocks.Find(LockRequest.NodeId);
			if (ExistingLock && ExistingLock->UserId != LockRequest.UserId)
			{
				PendingLockRequests.FindOrAdd(LockRequest.NodeId).Add(LockRequest);
			}
		}
	}
	else if (LockRequest.LockState == ELiveBPLockState::Unlocked)
	{
		// Remote unlock request
		ReleaseLock(LockRequest.NodeId, LockRequest.UserId);
	}
}

void ULiveBPLockManager::HandleRemoteLockRelease(const FLiveBPNodeLock& LockRelease)
{
	ReleaseLock(LockRelease.NodeId, LockRelease.UserId);
}

void ULiveBPLockManager::UpdateLocks(float DeltaTime)
{
	const float CurrentTime = FPlatformTime::Seconds();
	TArray<FGuid> ExpiredLocks;

	// Check for expired locks
	for (auto& LockPair : NodeLocks)
	{
		if (IsLockExpired(LockPair.Value))
		{
			ExpiredLocks.Add(LockPair.Key);
		}
		// Extend locks that are about to expire (if user is still active)
		else if (GetLockTimeRemaining(LockPair.Key) < LOCK_EXTENSION_TIME)
		{
			// Here you could implement logic to check if user is still active
			// For now, we'll let locks expire naturally
		}
	}

	// Remove expired locks
	for (const FGuid& NodeId : ExpiredLocks)
	{
		ExpireLock(NodeId);
		ProcessPendingRequests(NodeId);
	}
}

void ULiveBPLockManager::ClearAllLocks()
{
	TArray<FGuid> AllNodeIds;
	NodeLocks.GetKeys(AllNodeIds);

	NodeLocks.Empty();
	PendingLockRequests.Empty();

	// Notify about all lock releases
	for (const FGuid& NodeId : AllNodeIds)
	{
		FLiveBPNodeLock ReleasedLock;
		ReleasedLock.NodeId = NodeId;
		ReleasedLock.LockState = ELiveBPLockState::Unlocked;
		OnNodeLockStateChanged.Broadcast(NodeId, ReleasedLock);
	}
}

void ULiveBPLockManager::ClearUserLocks(const FString& UserId)
{
	TArray<FGuid> UserLockedNodes;
	
	// Find all nodes locked by this user
	for (const auto& LockPair : NodeLocks)
	{
		if (LockPair.Value.UserId == UserId)
		{
			UserLockedNodes.Add(LockPair.Key);
		}
	}

	// Release all user's locks
	for (const FGuid& NodeId : UserLockedNodes)
	{
		ReleaseLock(NodeId, UserId);
	}

	// Remove user's pending requests
	for (auto& PendingPair : PendingLockRequests)
	{
		PendingPair.Value.RemoveAll([&UserId](const FLiveBPNodeLock& Lock) {
			return Lock.UserId == UserId;
		});
	}
}

void ULiveBPLockManager::ProcessPendingRequests(const FGuid& NodeId)
{
	TArray<FLiveBPNodeLock>* PendingRequests = PendingLockRequests.Find(NodeId);
	if (!PendingRequests || PendingRequests->Num() == 0)
	{
		return;
	}

	// Grant lock to first pending request (FIFO)
	FLiveBPNodeLock NextLock = (*PendingRequests)[0];
	PendingRequests->RemoveAt(0);

	// Clean up empty pending arrays
	if (PendingRequests->Num() == 0)
	{
		PendingLockRequests.Remove(NodeId);
	}

	// Grant the lock
	GrantLock(NodeId, NextLock);
}

bool ULiveBPLockManager::IsLockExpired(const FLiveBPNodeLock& Lock) const
{
	return FPlatformTime::Seconds() > Lock.ExpiryTime;
}

void ULiveBPLockManager::ExpireLock(const FGuid& NodeId)
{
	if (FLiveBPNodeLock* Lock = NodeLocks.Find(NodeId))
	{
		FLiveBPNodeLock ExpiredLock = *Lock;
		ExpiredLock.LockState = ELiveBPLockState::Unlocked;
		
		NodeLocks.Remove(NodeId);
		OnNodeLockStateChanged.Broadcast(NodeId, ExpiredLock);
		
		UE_LOG(LogLiveBPCore, Log, TEXT("Lock expired for node %s (user: %s)"), 
			*NodeId.ToString(), *ExpiredLock.UserId);
	}
}

void ULiveBPLockManager::GrantLock(const FGuid& NodeId, const FLiveBPNodeLock& LockRequest)
{
	FLiveBPNodeLock GrantedLock = LockRequest;
	GrantedLock.LockState = ELiveBPLockState::Locked;
	
	NodeLocks.Add(NodeId, GrantedLock);
	OnNodeLockStateChanged.Broadcast(NodeId, GrantedLock);
	
	UE_LOG(LogLiveBPCore, Log, TEXT("Lock granted for node %s to user %s"), 
		*NodeId.ToString(), *GrantedLock.UserId);
}
