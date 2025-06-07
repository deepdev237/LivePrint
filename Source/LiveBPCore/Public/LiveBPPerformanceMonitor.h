#pragma once

#include "CoreMinimal.h"
#include "LiveBPDataTypes.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/CircularBuffer.h"

/**
 * Performance monitoring and profiling system for Live Blueprint collaboration
 */
class LIVEBPCORE_API FLiveBPPerformanceMonitor
{
public:
	struct FPerformanceMetrics
	{
		// Message throughput
		float MessagesPerSecond = 0.0f;
		float PeakMessagesPerSecond = 0.0f;
		int32 TotalMessagesSent = 0;
		int32 TotalMessagesReceived = 0;
		
		// Network latency
		float AverageLatencyMs = 0.0f;
		float PeakLatencyMs = 0.0f;
		float LatencyStandardDeviation = 0.0f;
		
		// Memory usage
		int32 MessageQueueSize = 0;
		int32 ActiveLockCount = 0;
		int32 CachedUserCount = 0;
		float EstimatedMemoryUsageMB = 0.0f;
		
		// Error rates
		float MessageFailureRate = 0.0f;
		int32 TotalErrors = 0;
		int32 NetworkErrors = 0;
		int32 SerializationErrors = 0;
		
		// Frame performance
		float AverageFrameTimeMs = 0.0f;
		float CollaborationOverheadMs = 0.0f;
		
		// Session info
		float SessionDurationSeconds = 0.0f;
		int32 ConnectedUserCount = 0;
		bool bIsSessionActive = false;
	};

	struct FScopeTimer
	{
		FScopeTimer(const FString& InName, FLiveBPPerformanceMonitor* InMonitor);
		~FScopeTimer();
		
	private:
		FString Name;
		FLiveBPPerformanceMonitor* Monitor;
		double StartTime;
	};

	FLiveBPPerformanceMonitor();
	~FLiveBPPerformanceMonitor();

	/**
	 * Start performance monitoring
	 */
	void StartMonitoring();

	/**
	 * Stop performance monitoring
	 */
	void StopMonitoring();

	/**
	 * Get current performance metrics
	 * @return Current performance metrics
	 */
	FPerformanceMetrics GetCurrentMetrics() const;

	/**
	 * Record a message sent
	 * @param MessageType Type of message
	 * @param PayloadSize Size in bytes
	 */
	void RecordMessageSent(ELiveBPMessageType MessageType, int32 PayloadSize);

	/**
	 * Record a message received
	 * @param MessageType Type of message
	 * @param PayloadSize Size in bytes
	 * @param LatencyMs Round-trip latency in milliseconds
	 */
	void RecordMessageReceived(ELiveBPMessageType MessageType, int32 PayloadSize, float LatencyMs);

	/**
	 * Record an error
	 * @param ErrorType Type of error
	 * @param bIsNetworkError Whether this is a network-related error
	 */
	void RecordError(const FString& ErrorType, bool bIsNetworkError = false);

	/**
	 * Update session information
	 * @param ConnectedUsers Number of connected users
	 * @param bIsActive Whether session is active
	 */
	void UpdateSessionInfo(int32 ConnectedUsers, bool bIsActive);

	/**
	 * Update memory usage statistics
	 * @param MessageQueueSize Current message queue size
	 * @param ActiveLockCount Number of active locks
	 * @param CachedUserCount Number of cached users
	 */
	void UpdateMemoryStats(int32 MessageQueueSize, int32 ActiveLockCount, int32 CachedUserCount);

	/**
	 * Record frame performance
	 * @param FrameTimeMs Frame time in milliseconds
	 * @param CollaborationOverheadMs Time spent on collaboration in milliseconds
	 */
	void RecordFramePerformance(float FrameTimeMs, float CollaborationOverheadMs);

	/**
	 * Add a scoped timer measurement
	 * @param Name Name of the operation
	 * @param DurationMs Duration in milliseconds
	 */
	void AddTimerMeasurement(const FString& Name, float DurationMs);

	/**
	 * Get detailed timing information
	 * @return Map of operation names to average times
	 */
	TMap<FString, float> GetDetailedTimings() const;

	/**
	 * Reset all statistics
	 */
	void ResetStats();

	/**
	 * Enable or disable performance monitoring
	 * @param bEnabled Whether monitoring is enabled
	 */
	void SetMonitoringEnabled(bool bEnabled);

	/**
	 * Check if monitoring is enabled
	 * @return true if monitoring is enabled
	 */
	bool IsMonitoringEnabled() const;

	/**
	 * Get performance report as formatted string
	 * @return Formatted performance report
	 */
	FString GetPerformanceReport() const;

	/**
	 * Create a scoped timer for automatic timing
	 * @param Name Name of the operation to time
	 * @return Scoped timer object
	 */
	TUniquePtr<FScopeTimer> CreateScopeTimer(const FString& Name);

private:
	struct FMessageStats
	{
		int32 Count = 0;
		int32 TotalSize = 0;
		float LastTime = 0.0f;
	};

	struct FLatencyMeasurement
	{
		float LatencyMs;
		float Timestamp;
		
		FLatencyMeasurement(float InLatency = 0.0f, float InTimestamp = 0.0f)
			: LatencyMs(InLatency), Timestamp(InTimestamp) {}
	};

	struct FTimingMeasurement
	{
		FString Name;
		float DurationMs;
		float Timestamp;
		
		FTimingMeasurement(const FString& InName = TEXT(""), float InDuration = 0.0f, float InTimestamp = 0.0f)
			: Name(InName), DurationMs(InDuration), Timestamp(InTimestamp) {}
	};

	mutable FCriticalSection StatsMutex;
	FThreadSafeBool bIsMonitoring;
	float SessionStartTime;
	
	// Message statistics
	FMessageStats SentMessages;
	FMessageStats ReceivedMessages;
	TMap<ELiveBPMessageType, FMessageStats> MessageTypeStats;
	
	// Latency tracking
	static const int32 MAX_LATENCY_SAMPLES = 100;
	TCircularBuffer<FLatencyMeasurement, MAX_LATENCY_SAMPLES> LatencyHistory;
	
	// Error tracking
	int32 TotalErrorCount;
	int32 NetworkErrorCount;
	int32 SerializationErrorCount;
	TMap<FString, int32> ErrorTypeCount;
	
	// Performance timing
	static const int32 MAX_TIMING_SAMPLES = 200;
	TCircularBuffer<FTimingMeasurement, MAX_TIMING_SAMPLES> TimingHistory;
	TMap<FString, TArray<float>> DetailedTimings;
	
	// Frame performance
	static const int32 MAX_FRAME_SAMPLES = 60;
	TCircularBuffer<float, MAX_FRAME_SAMPLES> FrameTimeHistory;
	TCircularBuffer<float, MAX_FRAME_SAMPLES> CollaborationOverheadHistory;
	
	// Session info
	int32 CurrentConnectedUsers;
	bool bIsSessionActive;
	
	// Memory stats
	int32 CurrentMessageQueueSize;
	int32 CurrentActiveLockCount;
	int32 CurrentCachedUserCount;
	
	// Helper functions
	float CalculateAverage(const TCircularBuffer<float, MAX_FRAME_SAMPLES>& History) const;
	float CalculateStandardDeviation(const TCircularBuffer<FLatencyMeasurement, MAX_LATENCY_SAMPLES>& History, float Average) const;
	float GetCurrentTime() const;
	void UpdateMessageRate();
	float EstimateMemoryUsage() const;
};

/**
 * Global performance monitor instance
 */
class LIVEBPCORE_API FLiveBPGlobalPerformanceMonitor
{
public:
	static FLiveBPPerformanceMonitor& Get();
	static void Initialize();
	static void Shutdown();

private:
	static TUniquePtr<FLiveBPPerformanceMonitor> Instance;
};

// Convenience macros for performance monitoring
#define LIVEBP_SCOPE_TIMER(Name) \
	auto ScopeTimer##__LINE__ = FLiveBPGlobalPerformanceMonitor::Get().CreateScopeTimer(Name)

#define LIVEBP_RECORD_MESSAGE_SENT(Type, Size) \
	FLiveBPGlobalPerformanceMonitor::Get().RecordMessageSent(Type, Size)

#define LIVEBP_RECORD_MESSAGE_RECEIVED(Type, Size, Latency) \
	FLiveBPGlobalPerformanceMonitor::Get().RecordMessageReceived(Type, Size, Latency)

#define LIVEBP_RECORD_ERROR(ErrorType, bIsNetwork) \
	FLiveBPGlobalPerformanceMonitor::Get().RecordError(ErrorType, bIsNetwork)
