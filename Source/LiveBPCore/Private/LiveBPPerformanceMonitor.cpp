#include "LiveBPPerformanceMonitor.h"
#include "LiveBPCore.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"

// Global instance
TUniquePtr<FLiveBPPerformanceMonitor> FLiveBPGlobalPerformanceMonitor::Instance = nullptr;

// FScopeTimer implementation
FLiveBPPerformanceMonitor::FScopeTimer::FScopeTimer(const FString& InName, FLiveBPPerformanceMonitor* InMonitor)
	: Name(InName)
	, Monitor(InMonitor)
	, StartTime(FPlatformTime::Seconds())
{
}

FLiveBPPerformanceMonitor::FScopeTimer::~FScopeTimer()
{
	if (Monitor && Monitor->IsMonitoringEnabled())
	{
		double EndTime = FPlatformTime::Seconds();
		float DurationMs = (EndTime - StartTime) * 1000.0f;
		Monitor->AddTimerMeasurement(Name, DurationMs);
	}
}

// FLiveBPPerformanceMonitor implementation
FLiveBPPerformanceMonitor::FLiveBPPerformanceMonitor()
	: bIsMonitoring(false)
	, SessionStartTime(0.0f)
	, LatencyHistory()
	, TotalErrorCount(0)
	, NetworkErrorCount(0)
	, SerializationErrorCount(0)
	, TimingHistory()
	, FrameTimeHistory()
	, CollaborationOverheadHistory()
	, CurrentConnectedUsers(0)
	, bIsSessionActive(false)
	, CurrentMessageQueueSize(0)
	, CurrentActiveLockCount(0)
	, CurrentCachedUserCount(0)
{
}

FLiveBPPerformanceMonitor::~FLiveBPPerformanceMonitor()
{
	StopMonitoring();
}

void FLiveBPPerformanceMonitor::StartMonitoring()
{
	FScopeLock Lock(&StatsMutex);
	
	if (!bIsMonitoring)
	{
		bIsMonitoring = true;
		SessionStartTime = GetCurrentTime();
		ResetStats();
		
		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Performance monitoring started"));
	}
}

void FLiveBPPerformanceMonitor::StopMonitoring()
{
	FScopeLock Lock(&StatsMutex);
	
	if (bIsMonitoring)
	{
		bIsMonitoring = false;
		
		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Performance monitoring stopped"));
		UE_LOG(LogLiveBPCore, Log, TEXT("Final Performance Report:\n%s"), *GetPerformanceReport());
	}
}

FLiveBPPerformanceMonitor::FPerformanceMetrics FLiveBPPerformanceMonitor::GetCurrentMetrics() const
{
	FScopeLock Lock(&StatsMutex);
	
	FPerformanceMetrics Metrics;
	
	// Calculate session duration
	float CurrentTime = GetCurrentTime();
	Metrics.SessionDurationSeconds = bIsMonitoring ? (CurrentTime - SessionStartTime) : 0.0f;
	
	// Message throughput
	if (Metrics.SessionDurationSeconds > 0.0f)
	{
		Metrics.MessagesPerSecond = (SentMessages.Count + ReceivedMessages.Count) / Metrics.SessionDurationSeconds;
	}
	Metrics.TotalMessagesSent = SentMessages.Count;
	Metrics.TotalMessagesReceived = ReceivedMessages.Count;
	
	// Calculate latency statistics
	if (LatencyHistory.Num() > 0)
	{
		float TotalLatency = 0.0f;
		float MaxLatency = 0.0f;
		
		for (const FLatencyMeasurement& Measurement : LatencyHistory)
		{
			TotalLatency += Measurement.LatencyMs;
			MaxLatency = FMath::Max(MaxLatency, Measurement.LatencyMs);
		}
		
		Metrics.AverageLatencyMs = TotalLatency / LatencyHistory.Num();
		Metrics.PeakLatencyMs = MaxLatency;
		Metrics.LatencyStandardDeviation = CalculateStandardDeviation(LatencyHistory, Metrics.AverageLatencyMs);
	}
	
	// Memory usage
	Metrics.MessageQueueSize = CurrentMessageQueueSize;
	Metrics.ActiveLockCount = CurrentActiveLockCount;
	Metrics.CachedUserCount = CurrentCachedUserCount;
	Metrics.EstimatedMemoryUsageMB = EstimateMemoryUsage();
	
	// Error rates
	int32 TotalMessages = SentMessages.Count + ReceivedMessages.Count;
	if (TotalMessages > 0)
	{
		Metrics.MessageFailureRate = static_cast<float>(TotalErrorCount) / TotalMessages;
	}
	Metrics.TotalErrors = TotalErrorCount;
	Metrics.NetworkErrors = NetworkErrorCount;
	Metrics.SerializationErrors = SerializationErrorCount;
	
	// Frame performance
	Metrics.AverageFrameTimeMs = CalculateAverage(FrameTimeHistory);
	Metrics.CollaborationOverheadMs = CalculateAverage(CollaborationOverheadHistory);
	
	// Session info
	Metrics.ConnectedUserCount = CurrentConnectedUsers;
	Metrics.bIsSessionActive = bIsSessionActive;
	
	return Metrics;
}

void FLiveBPPerformanceMonitor::RecordMessageSent(ELiveBPMessageType MessageType, int32 PayloadSize)
{
	if (!bIsMonitoring)
		return;
	
	FScopeLock Lock(&StatsMutex);
	
	SentMessages.Count++;
	SentMessages.TotalSize += PayloadSize;
	SentMessages.LastTime = GetCurrentTime();
	
	FMessageStats& TypeStats = MessageTypeStats.FindOrAdd(MessageType);
	TypeStats.Count++;
	TypeStats.TotalSize += PayloadSize;
	TypeStats.LastTime = SentMessages.LastTime;
}

void FLiveBPPerformanceMonitor::RecordMessageReceived(ELiveBPMessageType MessageType, int32 PayloadSize, float LatencyMs)
{
	if (!bIsMonitoring)
		return;
	
	FScopeLock Lock(&StatsMutex);
	
	ReceivedMessages.Count++;
	ReceivedMessages.TotalSize += PayloadSize;
	ReceivedMessages.LastTime = GetCurrentTime();
	
	// Record latency
	LatencyHistory.Add(FLatencyMeasurement(LatencyMs, ReceivedMessages.LastTime));
	
	FMessageStats& TypeStats = MessageTypeStats.FindOrAdd(MessageType);
	TypeStats.Count++;
	TypeStats.TotalSize += PayloadSize;
	TypeStats.LastTime = ReceivedMessages.LastTime;
}

void FLiveBPPerformanceMonitor::RecordError(const FString& ErrorType, bool bIsNetworkError)
{
	if (!bIsMonitoring)
		return;
	
	FScopeLock Lock(&StatsMutex);
	
	TotalErrorCount++;
	
	if (bIsNetworkError)
	{
		NetworkErrorCount++;
	}
	else
	{
		SerializationErrorCount++;
	}
	
	int32& Count = ErrorTypeCount.FindOrAdd(ErrorType);
	Count++;
	
	UE_LOG(LogLiveBPCore, Warning, TEXT("LiveBP Error recorded: %s (Network: %s)"), 
		*ErrorType, bIsNetworkError ? TEXT("Yes") : TEXT("No"));
}

void FLiveBPPerformanceMonitor::UpdateSessionInfo(int32 ConnectedUsers, bool bIsActive)
{
	FScopeLock Lock(&StatsMutex);
	
	CurrentConnectedUsers = ConnectedUsers;
	bIsSessionActive = bIsActive;
}

void FLiveBPPerformanceMonitor::UpdateMemoryStats(int32 MessageQueueSize, int32 ActiveLockCount, int32 CachedUserCount)
{
	FScopeLock Lock(&StatsMutex);
	
	CurrentMessageQueueSize = MessageQueueSize;
	CurrentActiveLockCount = ActiveLockCount;
	CurrentCachedUserCount = CachedUserCount;
}

void FLiveBPPerformanceMonitor::RecordFramePerformance(float FrameTimeMs, float CollaborationOverheadMs)
{
	if (!bIsMonitoring)
		return;
	
	FScopeLock Lock(&StatsMutex);
	
	FrameTimeHistory.Add(FrameTimeMs);
	CollaborationOverheadHistory.Add(CollaborationOverheadMs);
}

void FLiveBPPerformanceMonitor::AddTimerMeasurement(const FString& Name, float DurationMs)
{
	if (!bIsMonitoring)
		return;
	
	FScopeLock Lock(&StatsMutex);
	
	TimingHistory.Add(FTimingMeasurement(Name, DurationMs, GetCurrentTime()));
	
	TArray<float>& Timings = DetailedTimings.FindOrAdd(Name);
	Timings.Add(DurationMs);
	
	// Keep only recent timings
	if (Timings.Num() > 50)
	{
		Timings.RemoveAt(0);
	}
}

TMap<FString, float> FLiveBPPerformanceMonitor::GetDetailedTimings() const
{
	FScopeLock Lock(&StatsMutex);
	
	TMap<FString, float> AverageTimings;
	
	for (const auto& Pair : DetailedTimings)
	{
		const TArray<float>& Timings = Pair.Value;
		if (Timings.Num() > 0)
		{
			float Total = 0.0f;
			for (float Time : Timings)
			{
				Total += Time;
			}
			AverageTimings.Add(Pair.Key, Total / Timings.Num());
		}
	}
	
	return AverageTimings;
}

void FLiveBPPerformanceMonitor::ResetStats()
{
	FScopeLock Lock(&StatsMutex);
	
	// Reset message stats
	SentMessages = FMessageStats();
	ReceivedMessages = FMessageStats();
	MessageTypeStats.Empty();
	
	// Reset latency tracking
	LatencyHistory.Reset();
	
	// Reset error tracking
	TotalErrorCount = 0;
	NetworkErrorCount = 0;
	SerializationErrorCount = 0;
	ErrorTypeCount.Empty();
	
	// Reset performance timing
	TimingHistory.Reset();
	DetailedTimings.Empty();
	
	// Reset frame performance
	FrameTimeHistory.Reset();
	CollaborationOverheadHistory.Reset();
}

void FLiveBPPerformanceMonitor::SetMonitoringEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		StartMonitoring();
	}
	else
	{
		StopMonitoring();
	}
}

bool FLiveBPPerformanceMonitor::IsMonitoringEnabled() const
{
	return bIsMonitoring;
}

FString FLiveBPPerformanceMonitor::GetPerformanceReport() const
{
	FPerformanceMetrics Metrics = GetCurrentMetrics();
	TMap<FString, float> DetailedTimingsMap = GetDetailedTimings();
	
	FString Report;
	Report += TEXT("=== LiveBP Performance Report ===\n");
	Report += FString::Printf(TEXT("Session Duration: %.1f seconds\n"), Metrics.SessionDurationSeconds);
	Report += FString::Printf(TEXT("Session Active: %s\n"), Metrics.bIsSessionActive ? TEXT("Yes") : TEXT("No"));
	Report += FString::Printf(TEXT("Connected Users: %d\n"), Metrics.ConnectedUserCount);
	Report += TEXT("\n");
	
	Report += TEXT("--- Message Statistics ---\n");
	Report += FString::Printf(TEXT("Messages Sent: %d\n"), Metrics.TotalMessagesSent);
	Report += FString::Printf(TEXT("Messages Received: %d\n"), Metrics.TotalMessagesReceived);
	Report += FString::Printf(TEXT("Messages Per Second: %.1f\n"), Metrics.MessagesPerSecond);
	Report += TEXT("\n");
	
	Report += TEXT("--- Network Performance ---\n");
	Report += FString::Printf(TEXT("Average Latency: %.1f ms\n"), Metrics.AverageLatencyMs);
	Report += FString::Printf(TEXT("Peak Latency: %.1f ms\n"), Metrics.PeakLatencyMs);
	Report += FString::Printf(TEXT("Latency Std Dev: %.1f ms\n"), Metrics.LatencyStandardDeviation);
	Report += TEXT("\n");
	
	Report += TEXT("--- Memory Usage ---\n");
	Report += FString::Printf(TEXT("Message Queue Size: %d\n"), Metrics.MessageQueueSize);
	Report += FString::Printf(TEXT("Active Locks: %d\n"), Metrics.ActiveLockCount);
	Report += FString::Printf(TEXT("Cached Users: %d\n"), Metrics.CachedUserCount);
	Report += FString::Printf(TEXT("Estimated Memory: %.1f MB\n"), Metrics.EstimatedMemoryUsageMB);
	Report += TEXT("\n");
	
	Report += TEXT("--- Error Statistics ---\n");
	Report += FString::Printf(TEXT("Total Errors: %d\n"), Metrics.TotalErrors);
	Report += FString::Printf(TEXT("Network Errors: %d\n"), Metrics.NetworkErrors);
	Report += FString::Printf(TEXT("Serialization Errors: %d\n"), Metrics.SerializationErrors);
	Report += FString::Printf(TEXT("Message Failure Rate: %.2f%%\n"), Metrics.MessageFailureRate * 100.0f);
	Report += TEXT("\n");
	
	Report += TEXT("--- Frame Performance ---\n");
	Report += FString::Printf(TEXT("Average Frame Time: %.1f ms\n"), Metrics.AverageFrameTimeMs);
	Report += FString::Printf(TEXT("Collaboration Overhead: %.1f ms\n"), Metrics.CollaborationOverheadMs);
	Report += TEXT("\n");
	
	if (DetailedTimingsMap.Num() > 0)
	{
		Report += TEXT("--- Detailed Timings ---\n");
		for (const auto& Pair : DetailedTimingsMap)
		{
			Report += FString::Printf(TEXT("%s: %.2f ms\n"), *Pair.Key, Pair.Value);
		}
	}
	
	return Report;
}

TUniquePtr<FLiveBPPerformanceMonitor::FScopeTimer> FLiveBPPerformanceMonitor::CreateScopeTimer(const FString& Name)
{
	if (bIsMonitoring)
	{
		return MakeUnique<FScopeTimer>(Name, this);
	}
	return nullptr;
}

float FLiveBPPerformanceMonitor::CalculateAverage(const TCircularBuffer<float, MAX_FRAME_SAMPLES>& History) const
{
	if (History.Num() == 0)
		return 0.0f;
	
	float Total = 0.0f;
	for (float Value : History)
	{
		Total += Value;
	}
	
	return Total / History.Num();
}

float FLiveBPPerformanceMonitor::CalculateStandardDeviation(const TCircularBuffer<FLatencyMeasurement, MAX_LATENCY_SAMPLES>& History, float Average) const
{
	if (History.Num() <= 1)
		return 0.0f;
	
	float SumSquaredDiff = 0.0f;
	for (const FLatencyMeasurement& Measurement : History)
	{
		float Diff = Measurement.LatencyMs - Average;
		SumSquaredDiff += Diff * Diff;
	}
	
	return FMath::Sqrt(SumSquaredDiff / (History.Num() - 1));
}

float FLiveBPPerformanceMonitor::GetCurrentTime() const
{
	return FPlatformTime::Seconds();
}

float FLiveBPPerformanceMonitor::EstimateMemoryUsage() const
{
	// Rough estimation of memory usage
	float EstimatedBytes = 0.0f;
	
	// Message queue
	EstimatedBytes += CurrentMessageQueueSize * 512; // Assume 512 bytes per message
	
	// Locks
	EstimatedBytes += CurrentActiveLockCount * 256; // Assume 256 bytes per lock
	
	// User cache
	EstimatedBytes += CurrentCachedUserCount * 1024; // Assume 1KB per user
	
	// History buffers
	EstimatedBytes += LatencyHistory.Num() * sizeof(FLatencyMeasurement);
	EstimatedBytes += TimingHistory.Num() * sizeof(FTimingMeasurement);
	EstimatedBytes += FrameTimeHistory.Num() * sizeof(float) * 2; // Frame time + overhead
	
	return EstimatedBytes / (1024.0f * 1024.0f); // Convert to MB
}

// Global performance monitor implementation
FLiveBPPerformanceMonitor& FLiveBPGlobalPerformanceMonitor::Get()
{
	if (!Instance.IsValid())
	{
		Initialize();
	}
	return *Instance;
}

void FLiveBPGlobalPerformanceMonitor::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FLiveBPPerformanceMonitor>();
		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Performance Monitor initialized"));
	}
}

void FLiveBPGlobalPerformanceMonitor::Shutdown()
{
	if (Instance.IsValid())
	{
		Instance.Reset();
		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Performance Monitor shutdown"));
	}
}
