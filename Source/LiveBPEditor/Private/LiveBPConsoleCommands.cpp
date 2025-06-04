// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveBPConsoleCommands.h"
#include "LiveBPEditor.h"
#include "LiveBPEditorSubsystem.h"
#include "LiveBPTestFramework.h"
#include "LiveBPPerformanceMonitor.h"
#include "LiveBPMUEIntegration.h"
#include "LiveBPLockManager.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

bool FLiveBPConsoleCommands::bCommandsRegistered = false;

// Static console command declarations
TAutoConsoleCommand<FString> FLiveBPConsoleCommands::ShowStatsCmd(
	TEXT("LiveBP.Debug.ShowStats"),
	TEXT("Display Live Blueprint performance statistics"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ShowStats)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::TestConnectionCmd(
	TEXT("LiveBP.Debug.TestConnection"),
	TEXT("Test Multi-User Editing connection"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::TestConnection)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::ClearLocksCmd(
	TEXT("LiveBP.Debug.ClearLocks"),
	TEXT("Clear all node locks (admin only)"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ClearLocks)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::SimulateLatencyCmd(
	TEXT("LiveBP.Debug.SimulateLatency"),
	TEXT("Simulate network latency in milliseconds"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::SimulateLatency)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::DumpMessagesCmd(
	TEXT("LiveBP.Debug.DumpMessages"),
	TEXT("Dump recent collaboration messages to log"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::DumpMessages)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::RunTestsCmd(
	TEXT("LiveBP.Debug.RunTests"),
	TEXT("Run Live Blueprint test suite"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::RunTests)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::ToggleDebugModeCmd(
	TEXT("LiveBP.Debug.ToggleDebugMode"),
	TEXT("Toggle debug visualization mode"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ToggleDebugMode)
);

TAutoConsoleCommand<FString> FLiveBPConsoleCommands::ShowHelpCmd(
	TEXT("LiveBP.Help"),
	TEXT("Show Live Blueprint console commands help"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ShowHelp)
);

void FLiveBPConsoleCommands::RegisterConsoleCommands()
{
	if (bCommandsRegistered)
	{
		return;
	}

	UE_LOG(LogLiveBPEditor, Log, TEXT("Registering Live Blueprint console commands"));
	bCommandsRegistered = true;
}

void FLiveBPConsoleCommands::UnregisterConsoleCommands()
{
	if (!bCommandsRegistered)
	{
		return;
	}

	UE_LOG(LogLiveBPEditor, Log, TEXT("Unregistering Live Blueprint console commands"));
	bCommandsRegistered = false;
}

void FLiveBPConsoleCommands::ShowStats(const TArray<FString>& Args)
{
	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("LiveBP Editor Subsystem not available"));
		return;
	}

	// Get performance monitor
	if (FLiveBPPerformanceMonitor* PerfMonitor = EditorSubsystem->GetPerformanceMonitor())
	{
		FLiveBPPerformanceMonitor::FPerformanceSnapshot Stats = PerfMonitor->GetCurrentSnapshot();
		
		UE_LOG(LogLiveBPEditor, Log, TEXT("=== Live Blueprint Performance Stats ==="));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Messages Sent: %d"), Stats.MessagesSent);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Messages Received: %d"), Stats.MessagesReceived);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Average Latency: %.1fms"), Stats.AverageLatency * 1000.0f);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Peak Latency: %.1fms"), Stats.PeakLatency * 1000.0f);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Throughput: %.1f msg/s"), Stats.Throughput);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Memory Usage: %.1f KB"), Stats.MemoryUsage / 1024.0f);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Active Users: %d"), Stats.ActiveUsers);
		
		// Also print to screen
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, 
				FString::Printf(TEXT("LiveBP Stats: %d sent, %d recv, %.1fms latency, %d users"), 
					Stats.MessagesSent, Stats.MessagesReceived, Stats.AverageLatency * 1000.0f, Stats.ActiveUsers));
		}
	}
}

void FLiveBPConsoleCommands::TestConnection(const TArray<FString>& Args)
{
	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("LiveBP Editor Subsystem not available"));
		return;
	}

	// Test MUE connection
	if (FLiveBPMUEIntegration* MUEIntegration = EditorSubsystem->GetMUEIntegration())
	{
		bool bConnected = MUEIntegration->IsConnected();
		FString SessionName = MUEIntegration->GetSessionName();
		int32 UserCount = MUEIntegration->GetConnectedUserCount();
		
		UE_LOG(LogLiveBPEditor, Log, TEXT("=== MUE Connection Test ==="));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Connected: %s"), bConnected ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Session: %s"), *SessionName);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Users: %d"), UserCount);
		
		if (GEngine)
		{
			FColor StatusColor = bConnected ? FColor::Green : FColor::Red;
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, StatusColor, 
				FString::Printf(TEXT("MUE Status: %s (%d users in %s)"), 
					bConnected ? TEXT("Connected") : TEXT("Disconnected"), UserCount, *SessionName));
		}
	}
}

void FLiveBPConsoleCommands::ClearLocks(const TArray<FString>& Args)
{
	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("LiveBP Editor Subsystem not available"));
		return;
	}

	// Clear all locks - this should be admin only in production
	if (FLiveBPLockManager* LockManager = EditorSubsystem->GetLockManager())
	{
		int32 ClearedLocks = LockManager->ClearAllLocks();
		UE_LOG(LogLiveBPEditor, Log, TEXT("Cleared %d node locks"), ClearedLocks);
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
				FString::Printf(TEXT("Cleared %d node locks"), ClearedLocks));
		}
	}
}

void FLiveBPConsoleCommands::SimulateLatency(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("Usage: LiveBP.Debug.SimulateLatency <milliseconds>"));
		return;
	}

	float LatencyMs = FCString::Atof(*Args[0]);
	if (LatencyMs < 0 || LatencyMs > 5000)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("Latency must be between 0-5000ms"));
		return;
	}

	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (EditorSubsystem)
	{
		// Set simulated latency in MUE integration
		if (FLiveBPMUEIntegration* MUEIntegration = EditorSubsystem->GetMUEIntegration())
		{
			MUEIntegration->SetSimulatedLatency(LatencyMs / 1000.0f);
			UE_LOG(LogLiveBPEditor, Log, TEXT("Set simulated latency to %.1fms"), LatencyMs);
			
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, 
					FString::Printf(TEXT("Simulating %.1fms latency"), LatencyMs));
			}
		}
	}
}

void FLiveBPConsoleCommands::DumpMessages(const TArray<FString>& Args)
{
	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("LiveBP Editor Subsystem not available"));
		return;
	}

	// Dump recent message history
	UE_LOG(LogLiveBPEditor, Log, TEXT("=== Recent Collaboration Messages ==="));
	// This would access a message history buffer in the subsystem
	UE_LOG(LogLiveBPEditor, Log, TEXT("Message dump feature not yet implemented"));
}

void FLiveBPConsoleCommands::RunTests(const TArray<FString>& Args)
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("Running Live Blueprint test suite..."));
	
	FLiveBPTestFramework::FTestResults Results = FLiveBPTestFramework::Get().RunAllTests();
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("Test Results: %d/%d passed (%.1f%%) in %.3fs"), 
		Results.TestsPassed, Results.TestsRun, Results.GetSuccessRate() * 100.0f, Results.TotalTestTime);
	
	if (GEngine)
	{
		FColor ResultColor = Results.GetSuccessRate() > 0.8f ? FColor::Green : FColor::Red;
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, ResultColor, 
			FString::Printf(TEXT("LiveBP Tests: %d/%d passed (%.1f%%)"), 
				Results.TestsPassed, Results.TestsRun, Results.GetSuccessRate() * 100.0f));
	}
}

void FLiveBPConsoleCommands::ToggleDebugMode(const TArray<FString>& Args)
{
	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("LiveBP Editor Subsystem not available"));
		return;
	}

	bool bNewDebugMode = !EditorSubsystem->IsDebugModeEnabled();
	EditorSubsystem->SetDebugModeEnabled(bNewDebugMode);
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("Debug mode: %s"), bNewDebugMode ? TEXT("ENABLED") : TEXT("DISABLED"));
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Magenta, 
			FString::Printf(TEXT("LiveBP Debug Mode: %s"), bNewDebugMode ? TEXT("ON") : TEXT("OFF")));
	}
}

void FLiveBPConsoleCommands::ShowHelp(const TArray<FString>& Args)
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("=== Live Blueprint Console Commands ==="));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.ShowStats - Display performance statistics"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.TestConnection - Test MUE connection"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.ClearLocks - Clear all node locks"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.SimulateLatency <ms> - Simulate network latency"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.DumpMessages - Dump recent messages"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.RunTests - Run test suite"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Debug.ToggleDebugMode - Toggle debug visualization"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP.Help - Show this help"));
}
