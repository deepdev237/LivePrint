#pragma once

#include "CoreMinimal.h"
#include "LiveBPDataTypes.h"
#include "Containers/Queue.h"

/**
 * Message throttling system to prevent network spam
 * Implements rate limiting per message type and per user
 */
class LIVEBPCORE_API FLiveBPMessageThrottler
{
public:
	FLiveBPMessageThrottler();
	~FLiveBPMessageThrottler();

	/**
	 * Check if a message should be throttled
	 * @param MessageType The type of message to check
	 * @param UserId The user sending the message
	 * @param CurrentTime Current timestamp
	 * @return true if message should be throttled (not sent)
	 */
	bool ShouldThrottleMessage(ELiveBPMessageType MessageType, const FString& UserId, float CurrentTime);

	/**
	 * Record that a message was sent
	 * @param MessageType The type of message sent
	 * @param UserId The user who sent the message
	 * @param CurrentTime Current timestamp
	 */
	void RecordMessageSent(ELiveBPMessageType MessageType, const FString& UserId, float CurrentTime);

	/**
	 * Clean up old message records to prevent memory leaks
	 * @param CurrentTime Current timestamp
	 */
	void CleanupOldRecords(float CurrentTime);

	/**
	 * Get the throttle interval for a message type
	 * @param MessageType The message type
	 * @return Minimum interval between messages in seconds
	 */
	static float GetThrottleInterval(ELiveBPMessageType MessageType);

	/**
	 * Set custom throttle interval for a message type
	 * @param MessageType The message type
	 * @param Interval Minimum interval in seconds
	 */
	void SetThrottleInterval(ELiveBPMessageType MessageType, float Interval);

	/**
	 * Enable or disable throttling for a specific message type
	 * @param MessageType The message type
	 * @param bEnabled Whether throttling is enabled
	 */
	void SetThrottlingEnabled(ELiveBPMessageType MessageType, bool bEnabled);

	/**
	 * Get statistics about message throttling
	 */
	struct FThrottleStats
	{
		int32 MessagesSent = 0;
		int32 MessagesThrottled = 0;
		float LastMessageTime = 0.0f;
	};

	FThrottleStats GetStatsForUser(const FString& UserId, ELiveBPMessageType MessageType) const;
	void ResetStats();

private:
	struct FMessageRecord
	{
		FString UserId;
		ELiveBPMessageType MessageType;
		float Timestamp;
		
		FMessageRecord() : MessageType(ELiveBPMessageType::Heartbeat), Timestamp(0.0f) {}
		FMessageRecord(const FString& InUserId, ELiveBPMessageType InMessageType, float InTimestamp)
			: UserId(InUserId), MessageType(InMessageType), Timestamp(InTimestamp) {}
	};

	struct FUserMessageStats
	{
		TMap<ELiveBPMessageType, FThrottleStats> Stats;
	};

	// Message history for throttling decisions
	TArray<FMessageRecord> MessageHistory;
	
	// Per-user statistics
	TMap<FString, FUserMessageStats> UserStats;
	
	// Custom throttle intervals
	TMap<ELiveBPMessageType, float> CustomThrottleIntervals;
	
	// Throttling enabled flags
	TMap<ELiveBPMessageType, bool> ThrottlingEnabled;
	
	// Maximum age for message records (older records are cleaned up)
	static const float MAX_RECORD_AGE;
	
	// Maximum number of records to keep in memory
	static const int32 MAX_RECORD_COUNT;

	// Helper functions
	float GetLastMessageTime(const FString& UserId, ELiveBPMessageType MessageType) const;
	void UpdateUserStats(const FString& UserId, ELiveBPMessageType MessageType, bool bWasThrottled);
};

/**
 * Global throttler instance
 */
class LIVEBPCORE_API FLiveBPGlobalThrottler
{
public:
	static FLiveBPMessageThrottler& Get();
	static void Initialize();
	static void Shutdown();

private:
	static TUniquePtr<FLiveBPMessageThrottler> Instance;
};
