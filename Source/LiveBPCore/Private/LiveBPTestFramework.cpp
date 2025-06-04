
#include "LiveBPTestFramework.h"
#include "LiveBPCore.h"
#include "LiveBPUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

FLiveBPTestFramework::FLiveBPTestFramework()
{
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Test Framework initialized"));
}

FLiveBPTestFramework::~FLiveBPTestFramework()
{
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Test Framework destroyed"));
}

FLiveBPTestFramework::FTestResults FLiveBPTestFramework::RunAllTests()
{
	FTestResults Results;
	float StartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogLiveBPCore, Log, TEXT("Running LiveBP Test Suite..."));
	
	// Test serialization
	if (TestMessageSerialization())
	{
		Results.TestsPassed++;
		UE_LOG(LogLiveBPCore, Log, TEXT("✓ Message Serialization Test PASSED"));
	}
	else
	{
		Results.TestsFailed++;
		Results.FailureReasons.Add(TEXT("Message Serialization Test FAILED"));
		UE_LOG(LogLiveBPCore, Error, TEXT("✗ Message Serialization Test FAILED"));
	}
	Results.TestsRun++;
	
	// Test throttling
	if (TestMessageThrottling())
	{
		Results.TestsPassed++;
		UE_LOG(LogLiveBPCore, Log, TEXT("✓ Message Throttling Test PASSED"));
	}
	else
	{
		Results.TestsFailed++;
		Results.FailureReasons.Add(TEXT("Message Throttling Test FAILED"));
		UE_LOG(LogLiveBPCore, Error, TEXT("✗ Message Throttling Test FAILED"));
	}
	Results.TestsRun++;
	
	// Test lock management
	if (TestLockManagement())
	{
		Results.TestsPassed++;
		UE_LOG(LogLiveBPCore, Log, TEXT("✓ Lock Management Test PASSED"));
	}
	else
	{
		Results.TestsFailed++;
		Results.FailureReasons.Add(TEXT("Lock Management Test FAILED"));
		UE_LOG(LogLiveBPCore, Error, TEXT("✗ Lock Management Test FAILED"));
	}
	Results.TestsRun++;
	
	// Test performance monitoring
	if (TestPerformanceMonitoring())
	{
		Results.TestsPassed++;
		UE_LOG(LogLiveBPCore, Log, TEXT("✓ Performance Monitoring Test PASSED"));
	}
	else
	{
		Results.TestsFailed++;
		Results.FailureReasons.Add(TEXT("Performance Monitoring Test FAILED"));
		UE_LOG(LogLiveBPCore, Error, TEXT("✗ Performance Monitoring Test FAILED"));
	}
	Results.TestsRun++;
	
	// Test network simulation
	if (TestNetworkSimulation())
	{
		Results.TestsPassed++;
		UE_LOG(LogLiveBPCore, Log, TEXT("✓ Network Simulation Test PASSED"));
	}
	else
	{
		Results.TestsFailed++;
		Results.FailureReasons.Add(TEXT("Network Simulation Test FAILED"));
		UE_LOG(LogLiveBPCore, Error, TEXT("✗ Network Simulation Test FAILED"));
	}
	Results.TestsRun++;
	
	Results.TotalTestTime = FPlatformTime::Seconds() - StartTime;
	
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Test Suite Complete: %d/%d tests passed (%.1f%%) in %.3fs"), 
		Results.TestsPassed, Results.TestsRun, Results.GetSuccessRate() * 100.0f, Results.TotalTestTime);
	
	return Results;
}

bool FLiveBPTestFramework::TestMessageSerialization()
{
	// Test wire preview message serialization
	FLiveBPWirePreviewMessage WireMsg;
	WireMsg.MessageId = FGuid::NewGuid();
	WireMsg.SenderId = TEXT("TestUser1");
	WireMsg.BlueprintId = FGuid::NewGuid();
	WireMsg.StartPinId = FGuid::NewGuid();
	WireMsg.CurrentMousePosition = FVector2D(100.0f, 200.0f);
	WireMsg.Timestamp = FPlatformTime::Seconds();
	
	TArray<uint8> SerializedData;
	if (!FLiveBPUtils::SerializeWirePreviewMessage(WireMsg, SerializedData))
	{
		return false;
	}
	
	FLiveBPWirePreviewMessage DeserializedMsg;
	if (!FLiveBPUtils::DeserializeWirePreviewMessage(SerializedData, DeserializedMsg))
	{
		return false;
	}
	
	// Verify data integrity
	if (DeserializedMsg.MessageId != WireMsg.MessageId ||
		DeserializedMsg.SenderId != WireMsg.SenderId ||
		DeserializedMsg.BlueprintId != WireMsg.BlueprintId ||
		DeserializedMsg.StartPinId != WireMsg.StartPinId ||
		!DeserializedMsg.CurrentMousePosition.Equals(WireMsg.CurrentMousePosition, 0.01f))
	{
		return false;
	}
	
	// Test node operation message serialization
	FLiveBPNodeOperationMessage NodeMsg;
	NodeMsg.MessageId = FGuid::NewGuid();
	NodeMsg.SenderId = TEXT("TestUser2");
	NodeMsg.BlueprintId = FGuid::NewGuid();
	NodeMsg.Operation = ELiveBPNodeOperation::Add;
	NodeMsg.NodeId = FGuid::NewGuid();
	NodeMsg.NodeClass = TEXT("K2Node_CallFunction");
	NodeMsg.Position = FVector2D(300.0f, 400.0f);
	
	FString JsonString;
	if (!FLiveBPUtils::SerializeToJson(NodeMsg, JsonString))
	{
		return false;
	}
	
	FLiveBPNodeOperationMessage DeserializedNodeMsg;
	if (!FLiveBPUtils::DeserializeFromJson(JsonString, DeserializedNodeMsg))
	{
		return false;
	}
	
	// Verify node message integrity
	if (DeserializedNodeMsg.MessageId != NodeMsg.MessageId ||
		DeserializedNodeMsg.SenderId != NodeMsg.SenderId ||
		DeserializedNodeMsg.Operation != NodeMsg.Operation ||
		DeserializedNodeMsg.NodeClass != NodeMsg.NodeClass)
	{
		return false;
	}
	
	return true;
}

bool FLiveBPTestFramework::TestMessageThrottling()
{
	if (!GEngine || !GEngine->GetWorld())
	{
		return false;
	}
	
	auto& Throttler = FLiveBPMessageThrottler::Get();
	FString TestUserId = TEXT("ThrottleTestUser");
	
	// Test wire preview throttling (should allow 10Hz)
	int32 AllowedMessages = 0;
	for (int32 i = 0; i < 20; ++i)
	{
		if (Throttler.CanSendMessage(TestUserId, ELiveBPMessageType::WirePreview))
		{
			AllowedMessages++;
		}
		// Small delay to simulate rapid firing
		FPlatformProcess::Sleep(0.01f);
	}
	
	// Should allow some messages but not all 20 in rapid succession
	if (AllowedMessages == 0 || AllowedMessages >= 20)
	{
		return false;
	}
	
	// Test that different message types have different limits
	if (!Throttler.CanSendMessage(TestUserId, ELiveBPMessageType::NodeOperation))
	{
		return false; // Node operations should have higher rate limit
	}
	
	return true;
}

bool FLiveBPTestFramework::TestLockManagement()
{
	FLiveBPLockManager LockManager;
	FGuid NodeId = FGuid::NewGuid();
	FString User1 = TEXT("User1");
	FString User2 = TEXT("User2");
	
	// Test acquiring lock
	if (!LockManager.TryAcquireLock(NodeId, User1))
	{
		return false;
	}
	
	// Test lock conflict
	if (LockManager.TryAcquireLock(NodeId, User2))
	{
		return false; // Should fail - already locked
	}
	
	// Test lock ownership
	if (!LockManager.IsNodeLocked(NodeId) || LockManager.GetNodeLockOwner(NodeId) != User1)
	{
		return false;
	}
	
	// Test releasing lock
	LockManager.ReleaseLock(NodeId, User1);
	if (LockManager.IsNodeLocked(NodeId))
	{
		return false;
	}
	
	// Test lock can be acquired after release
	if (!LockManager.TryAcquireLock(NodeId, User2))
	{
		return false;
	}
	
	return true;
}

bool FLiveBPTestFramework::TestPerformanceMonitoring()
{
	auto& Monitor = FLiveBPPerformanceMonitor::Get();
	
	// Test latency recording
	float TestLatency = 0.05f; // 50ms
	Monitor.RecordLatency(ELiveBPMessageType::WirePreview, TestLatency);
	
	auto Stats = Monitor.GetLatencyStats(ELiveBPMessageType::WirePreview);
	if (Stats.SampleCount != 1 || FMath::Abs(Stats.Average - TestLatency) > 0.001f)
	{
		return false;
	}
	
	// Test throughput recording
	Monitor.RecordThroughput(ELiveBPMessageType::NodeOperation, 1);
	Monitor.RecordThroughput(ELiveBPMessageType::NodeOperation, 2);
	
	auto ThroughputStats = Monitor.GetThroughputStats(ELiveBPMessageType::NodeOperation);
	if (ThroughputStats.SampleCount != 2)
	{
		return false;
	}
	
	// Test error recording
	Monitor.RecordError(ELiveBPMessageType::PinConnection, TEXT("Test connection error"));
	
	auto ErrorStats = Monitor.GetErrorStats(ELiveBPMessageType::PinConnection);
	if (ErrorStats.TotalErrors != 1)
	{
		return false;
	}
	
	return true;
}

bool FLiveBPTestFramework::TestNetworkSimulation()
{
	// Create test messages
	TArray<FTestMessage> TestMessages = {
		FTestMessage(ELiveBPMessageType::WirePreview, TEXT("User1"), TEXT("User2")),
		FTestMessage(ELiveBPMessageType::NodeOperation, TEXT("User2"), TEXT("User1")),
		FTestMessage(ELiveBPMessageType::PinConnection, TEXT("User1"), TEXT("User3")),
		FTestMessage(ELiveBPMessageType::NodeLockRequest, TEXT("User3"), TEXT("User1"))
	};
	
	// Simulate message processing
	int32 ProcessedMessages = 0;
	for (const auto& TestMsg : TestMessages)
	{
		// Simulate network delay
		float SimulatedLatency = FMath::RandRange(0.01f, 0.1f);
		
		// Record metrics
		auto& Monitor = FLiveBPPerformanceMonitor::Get();
		Monitor.RecordLatency(TestMsg.Type, SimulatedLatency);
		
		if (TestMsg.bExpectedToSucceed)
		{
			ProcessedMessages++;
		}
		else
		{
			Monitor.RecordError(TestMsg.Type, TEXT("Simulated network error"));
		}
	}
	
	// Verify all expected messages were processed
	return ProcessedMessages == TestMessages.Num();
}

bool FLiveBPTestFramework::TestMessageValidation()
{
	// Test valid wire preview message
	FLiveBPWirePreviewMessage ValidMsg;
	ValidMsg.MessageId = FGuid::NewGuid();
	ValidMsg.SenderId = TEXT("ValidUser");
	ValidMsg.BlueprintId = FGuid::NewGuid();
	ValidMsg.StartPinId = FGuid::NewGuid();
	ValidMsg.CurrentMousePosition = FVector2D(100.0f, 200.0f);
	ValidMsg.Timestamp = FPlatformTime::Seconds();
	
	if (!FLiveBPUtils::ValidateMessage(ValidMsg))
	{
		return false;
	}
	
	// Test invalid message (empty sender)
	FLiveBPWirePreviewMessage InvalidMsg = ValidMsg;
	InvalidMsg.SenderId = TEXT("");
	
	if (FLiveBPUtils::ValidateMessage(InvalidMsg))
	{
		return false; // Should fail validation
	}
	
	// Test invalid message (invalid GUID)
	InvalidMsg = ValidMsg;
	InvalidMsg.MessageId = FGuid();
	
	if (FLiveBPUtils::ValidateMessage(InvalidMsg))
	{
		return false; // Should fail validation
	}
	
	return true;
}

void FLiveBPTestFramework::RunStressTest(int32 NumMessages, float Duration)
{
	UE_LOG(LogLiveBPCore, Log, TEXT("Starting LiveBP Stress Test: %d messages over %.1fs"), NumMessages, Duration);
	
	float StartTime = FPlatformTime::Seconds();
	auto& Throttler = FLiveBPMessageThrottler::Get();
	auto& Monitor = FLiveBPPerformanceMonitor::Get();
	
	int32 MessagesSent = 0;
	int32 MessagesThrottled = 0;
	
	for (int32 i = 0; i < NumMessages && (FPlatformTime::Seconds() - StartTime) < Duration; ++i)
	{
		FString UserId = FString::Printf(TEXT("StressUser%d"), i % 10); // 10 simulated users
		ELiveBPMessageType MsgType = static_cast<ELiveBPMessageType>(i % 4); // Cycle through message types
		
		if (Throttler.CanSendMessage(UserId, MsgType))
		{
			MessagesSent++;
			// Simulate message processing time
			float ProcessingTime = FMath::RandRange(0.001f, 0.01f);
			Monitor.RecordLatency(MsgType, ProcessingTime);
		}
		else
		{
			MessagesThrottled++;
		}
		
		// Small delay between messages
		FPlatformProcess::Sleep(0.001f);
	}
	
	float TotalTime = FPlatformTime::Seconds() - StartTime;
	float MessageRate = MessagesSent / TotalTime;
	
	UE_LOG(LogLiveBPCore, Log, TEXT("Stress Test Complete: %d messages sent, %d throttled, %.1f msg/sec"), 
		MessagesSent, MessagesThrottled, MessageRate);
}

void FLiveBPTestFramework::SimulateUserSession(const FString& UserId, float SessionDuration)
{
	UE_LOG(LogLiveBPCore, Log, TEXT("Simulating user session for %s (%.1fs)"), *UserId, SessionDuration);
	
	float StartTime = FPlatformTime::Seconds();
	auto& Throttler = FLiveBPMessageThrottler::Get();
	auto& Monitor = FLiveBPPerformanceMonitor::Get();
	
	FLiveBPLockManager LockManager;
	TArray<FGuid> LockedNodes;
	
	while ((FPlatformTime::Seconds() - StartTime) < SessionDuration)
	{
		// Simulate different user actions
		int32 ActionType = FMath::RandRange(0, 4);
		
		switch (ActionType)
		{
		case 0: // Wire dragging
			if (Throttler.CanSendMessage(UserId, ELiveBPMessageType::WirePreview))
			{
				Monitor.RecordLatency(ELiveBPMessageType::WirePreview, FMath::RandRange(0.01f, 0.05f));
			}
			break;
			
		case 1: // Node operations
			if (Throttler.CanSendMessage(UserId, ELiveBPMessageType::NodeOperation))
			{
				Monitor.RecordLatency(ELiveBPMessageType::NodeOperation, FMath::RandRange(0.02f, 0.1f));
			}
			break;
			
		case 2: // Lock/unlock nodes
			{
				FGuid NodeId = FGuid::NewGuid();
				if (LockManager.TryAcquireLock(NodeId, UserId))
				{
					LockedNodes.Add(NodeId);
				}
				else if (LockedNodes.Num() > 0)
				{
					FGuid NodeToRelease = LockedNodes[FMath::RandRange(0, LockedNodes.Num() - 1)];
					LockManager.ReleaseLock(NodeToRelease, UserId);
					LockedNodes.Remove(NodeToRelease);
				}
			}
			break;
			
		case 3: // Pin connections
			if (Throttler.CanSendMessage(UserId, ELiveBPMessageType::PinConnection))
			{
				Monitor.RecordLatency(ELiveBPMessageType::PinConnection, FMath::RandRange(0.015f, 0.08f));
			}
			break;
		}
		
		// Simulate user thinking time
		FPlatformProcess::Sleep(FMath::RandRange(0.1f, 1.0f));
	}
	
	// Cleanup - release all locks
	for (const FGuid& NodeId : LockedNodes)
	{
		LockManager.ReleaseLock(NodeId, UserId);
	}
	
	UE_LOG(LogLiveBPCore, Log, TEXT("User session complete for %s"), *UserId);
}
