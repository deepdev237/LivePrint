#pragma once

#include "CoreMinimal.h"
#include "LiveBPDataTypes.h"
#include "LiveBPMUEIntegration.h"
#include "LiveBPLockManager.h"
#include "LiveBPMessageThrottler.h"
#include "LiveBPPerformanceMonitor.h"
#include "Misc/AutomationTest.h"

/**
 * Test framework for Live Blueprint collaboration features
 */
class LIVEBPCORE_API FLiveBPTestFramework
{
public:
	struct FTestMessage
	{
		ELiveBPMessageType Type;
		FString SenderId;
		FString ReceiverId;
		float Timestamp;
		bool bExpectedToSucceed;
		
		FTestMessage(ELiveBPMessageType InType, const FString& InSender, const FString& InReceiver = TEXT(""), bool bSuccess = true)
			: Type(InType), SenderId(InSender), ReceiverId(InReceiver), Timestamp(FPlatformTime::Seconds()), bExpectedToSucceed(bSuccess) {}
	};

	struct FTestResults
	{
		int32 TestsRun = 0;
		int32 TestsPassed = 0;
		int32 TestsFailed = 0;
		TArray<FString> FailureReasons;
		float TotalTestTime = 0.0f;
		
		float GetSuccessRate() const { return TestsRun > 0 ? (float)TestsPassed / TestsRun : 0.0f; }
		bool AllTestsPassed() const { return TestsRun > 0 && TestsFailed == 0; }
	};

	FLiveBPTestFramework();
	~FLiveBPTestFramework();

	/**
	 * Run all tests
	 * @return Test results
	 */
	FTestResults RunAllTests();

	/**
	 * Test message serialization and deserialization
	 * @return true if all serialization tests pass
	 */
	bool TestMessageSerialization();

	/**
	 * Test message throttling system
	 * @return true if throttling tests pass
	 */
	bool TestMessageThrottling();

	/**
	 * Test node locking system
	 * @return true if locking tests pass
	 */
	bool TestNodeLocking();

	/**
	 * Test performance monitoring
	 * @return true if performance tests pass
	 */
	bool TestPerformanceMonitoring();

	/**
	 * Test MUE integration (requires active MUE session)
	 * @return true if MUE tests pass
	 */
	bool TestMUEIntegration();

	/**
	 * Test conflict resolution
	 * @return true if conflict resolution tests pass
	 */
	bool TestConflictResolution();

	/**
	 * Stress test with high message volume
	 * @param MessageCount Number of messages to send
	 * @param UserCount Number of simulated users
	 * @return true if stress test passes
	 */
	bool StressTest(int32 MessageCount = 1000, int32 UserCount = 5);

	/**
	 * Load test with large Blueprints
	 * @param NodeCount Number of nodes to simulate
	 * @return true if load test passes
	 */
	bool LoadTest(int32 NodeCount = 500);

	/**
	 * Memory leak test
	 * @param Iterations Number of test iterations
	 * @return true if no memory leaks detected
	 */
	bool MemoryLeakTest(int32 Iterations = 100);

	/**
	 * Get detailed test report
	 * @return Formatted test report string
	 */
	FString GetTestReport() const;

	/**
	 * Reset test framework state
	 */
	void Reset();

private:
	FTestResults CurrentResults;
	TArray<FTestMessage> TestMessages;
	TUniquePtr<FLiveBPMessageThrottler> TestThrottler;
	float TestStartTime;

	// Test helper functions
	bool RunSingleTest(const FString& TestName, TFunction<bool()> TestFunction);
	void LogTestResult(const FString& TestName, bool bPassed, const FString& Details = TEXT(""));
	
	// Serialization test helpers
	bool TestWirePreviewSerialization();
	bool TestNodeOperationSerialization();
	bool TestNodeLockSerialization();
	bool TestInvalidDataHandling();
	
	// Throttling test helpers
	bool TestWirePreviewThrottling();
	bool TestStructuralMessageNoThrottling();
	bool TestPerUserThrottling();
	bool TestThrottleIntervalSettings();
	
	// Locking test helpers
	bool TestBasicLocking();
	bool TestLockExpiry();
	bool TestConflictingLocks();
	bool TestLockHierarchy();
	
	// Performance test helpers
	bool TestMessageThroughputMeasurement();
	bool TestLatencyMeasurement();
	bool TestMemoryUsageTracking();
	bool TestDetailedTimings();
	
	// MUE integration test helpers
	bool TestMUEConnection();
	bool TestMessageRoundTrip();
	bool TestMultiUserScenario();
	
	// Conflict resolution test helpers
	bool TestSimultaneousNodeEdit();
	bool TestWireConflicts();
	bool TestFIFOResolution();
	
	// Stress test helpers
	void GenerateStressTestMessages(int32 MessageCount, int32 UserCount);
	bool ValidateStressTestResults();
	
	// Utility functions
	FLiveBPWirePreview CreateTestWirePreview(const FString& UserId = TEXT("TestUser"));
	FLiveBPNodeOperationData CreateTestNodeOperation(ELiveBPNodeOperation Operation, const FString& UserId = TEXT("TestUser"));
	FLiveBPNodeLock CreateTestNodeLock(ELiveBPLockState LockState, const FString& UserId = TEXT("TestUser"));
	FString GenerateRandomUserId();
	FGuid GenerateRandomNodeId();
	
	// Memory tracking
	SIZE_T GetCurrentMemoryUsage() const;
	SIZE_T InitialMemoryUsage;
	SIZE_T PeakMemoryUsage;
};

/**
 * Automated test cases using Unreal's automation framework
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveBPSerializationTest, "LiveBlueprint.Core.Serialization", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::HighPriority)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveBPThrottlingTest, "LiveBlueprint.Core.Throttling", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::MediumPriority)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveBPLockingTest, "LiveBlueprint.Core.Locking", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::HighPriority)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveBPPerformanceTest, "LiveBlueprint.Core.Performance", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::MediumPriority)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveBPStressTest, "LiveBlueprint.Core.StressTest", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::LowPriority)

/**
 * Console commands for manual testing
 */
class LIVEBPCORE_API FLiveBPTestCommands
{
public:
	static void RegisterCommands();
	static void UnregisterCommands();

private:
	// Console command handlers
	static void RunAllTests(const TArray<FString>& Args);
	static void RunSpecificTest(const TArray<FString>& Args);
	static void StressTest(const TArray<FString>& Args);
	static void ShowTestReport(const TArray<FString>& Args);
	static void ResetTests(const TArray<FString>& Args);
	
	static TUniquePtr<FLiveBPTestFramework> TestFramework;
	static bool bCommandsRegistered;
};
