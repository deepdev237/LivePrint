#include "LiveBPMessageThrottler.h"
#include "LiveBPCore.h"
#include "Engine/Engine.h"

// Static constants
const float FLiveBPMessageThrottler::MAX_RECORD_AGE = 30.0f; // 30 seconds
const int32 FLiveBPMessageThrottler::MAX_RECORD_COUNT = 1000;

// Global throttler instance
TUniquePtr<FLiveBPMessageThrottler> FLiveBPGlobalThrottler::Instance = nullptr;

FLiveBPMessageThrottler::FLiveBPMessageThrottler()
{
	// Initialize default throttling settings
	ThrottlingEnabled.Add(ELiveBPMessageType::WirePreview, true);
	ThrottlingEnabled.Add(ELiveBPMessageType::NodeOperation, false);
	ThrottlingEnabled.Add(ELiveBPMessageType::LockRequest, false);
	ThrottlingEnabled.Add(ELiveBPMessageType::LockRelease, false);
	ThrottlingEnabled.Add(ELiveBPMessageType::Heartbeat, true);
}

FLiveBPMessageThrottler::~FLiveBPMessageThrottler()
{
	MessageHistory.Empty();
	UserStats.Empty();
}

bool FLiveBPMessageThrottler::ShouldThrottleMessage(ELiveBPMessageType MessageType, const FString& UserId, float CurrentTime)
{
	// Check if throttling is enabled for this message type
	const bool* bEnabled = ThrottlingEnabled.Find(MessageType);
	if (!bEnabled || !(*bEnabled))
	{
		return false;
	}

	// Get throttle interval
	float ThrottleInterval = GetThrottleInterval(MessageType);
	if (ThrottleInterval <= 0.0f)
	{
		return false;
	}

	// Check last message time for this user and message type
	float LastMessageTime = GetLastMessageTime(UserId, MessageType);
	bool bShouldThrottle = (CurrentTime - LastMessageTime) < ThrottleInterval;

	// Update statistics
	UpdateUserStats(UserId, MessageType, bShouldThrottle);

	return bShouldThrottle;
}

void FLiveBPMessageThrottler::RecordMessageSent(ELiveBPMessageType MessageType, const FString& UserId, float CurrentTime)
{
	// Add record to history
	MessageHistory.Emplace(UserId, MessageType, CurrentTime);
	
	// Update user statistics
	UpdateUserStats(UserId, MessageType, false);
	
	// Clean up old records if we have too many
	if (MessageHistory.Num() > MAX_RECORD_COUNT)
	{
		CleanupOldRecords(CurrentTime);
	}
}

void FLiveBPMessageThrottler::CleanupOldRecords(float CurrentTime)
{
	// Remove records older than MAX_RECORD_AGE
	MessageHistory.RemoveAll([CurrentTime](const FMessageRecord& Record)
	{
		return (CurrentTime - Record.Timestamp) > MAX_RECORD_AGE;
	});
}

float FLiveBPMessageThrottler::GetThrottleInterval(ELiveBPMessageType MessageType)
{
	switch (MessageType)
	{
	case ELiveBPMessageType::WirePreview:
		return 0.1f; // 10Hz
	case ELiveBPMessageType::NodeOperation:
		return 0.0f; // No throttling for structural changes
	case ELiveBPMessageType::LockRequest:
	case ELiveBPMessageType::LockRelease:
		return 0.0f; // No throttling for locks
	case ELiveBPMessageType::Heartbeat:
		return 1.0f; // 1 second heartbeat
	default:
		return 0.0f;
	}
}

void FLiveBPMessageThrottler::SetThrottleInterval(ELiveBPMessageType MessageType, float Interval)
{
	CustomThrottleIntervals.Add(MessageType, Interval);
}

void FLiveBPMessageThrottler::SetThrottlingEnabled(ELiveBPMessageType MessageType, bool bEnabled)
{
	ThrottlingEnabled.Add(MessageType, bEnabled);
}

FLiveBPMessageThrottler::FThrottleStats FLiveBPMessageThrottler::GetStatsForUser(const FString& UserId, ELiveBPMessageType MessageType) const
{
	const FUserMessageStats* UserStatsPtr = UserStats.Find(UserId);
	if (UserStatsPtr)
	{
		const FThrottleStats* StatsPtr = UserStatsPtr->Stats.Find(MessageType);
		if (StatsPtr)
		{
			return *StatsPtr;
		}
	}
	
	return FThrottleStats();
}

void FLiveBPMessageThrottler::ResetStats()
{
	UserStats.Empty();
}

float FLiveBPMessageThrottler::GetLastMessageTime(const FString& UserId, ELiveBPMessageType MessageType) const
{
	float LastTime = 0.0f;
	
	// Search backwards through message history for the most recent matching record
	for (int32 i = MessageHistory.Num() - 1; i >= 0; --i)
	{
		const FMessageRecord& Record = MessageHistory[i];
		if (Record.UserId == UserId && Record.MessageType == MessageType)
		{
			LastTime = Record.Timestamp;
			break;
		}
	}
	
	return LastTime;
}

void FLiveBPMessageThrottler::UpdateUserStats(const FString& UserId, ELiveBPMessageType MessageType, bool bWasThrottled)
{
	FUserMessageStats& UserStatsRef = UserStats.FindOrAdd(UserId);
	FThrottleStats& Stats = UserStatsRef.Stats.FindOrAdd(MessageType);
	
	if (bWasThrottled)
	{
		Stats.MessagesThrottled++;
	}
	else
	{
		Stats.MessagesSent++;
		Stats.LastMessageTime = FPlatformTime::Seconds();
	}
}

// Global throttler implementation
FLiveBPMessageThrottler& FLiveBPGlobalThrottler::Get()
{
	if (!Instance.IsValid())
	{
		Initialize();
	}
	return *Instance;
}

void FLiveBPGlobalThrottler::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FLiveBPMessageThrottler>();
		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Message Throttler initialized"));
	}
}

void FLiveBPGlobalThrottler::Shutdown()
{
	if (Instance.IsValid())
	{
		Instance.Reset();
		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Message Throttler shutdown"));
	}
}
