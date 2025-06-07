// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveBPConsoleCommands.h"
#include "CoreMinimal.h"
#include "LiveBPEditor.h"
#include "LiveBPEditorSubsystem.h"
#include "LiveBPTestFramework.h"
#include "LiveBPPerformanceMonitor.h"
#include "LiveBPMUEIntegration.h"
#include "LiveBPLockManager.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

bool FLiveBPConsoleCommands::bCommandsRegistered = false;

// Static console command declarations - Fixed for UE 5.5
static FAutoConsoleCommand ShowStatsCmd(
	TEXT("LiveBP.Debug.ShowStats"),
	TEXT("Display Live Blueprint performance statistics"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ShowStats)
);

static FAutoConsoleCommand TestConnectionCmd(
	TEXT("LiveBP.Debug.TestConnection"),
	TEXT("Test Multi-User Editing connection"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::TestConnection)
);

static FAutoConsoleCommand ClearLocksCmd(
	TEXT("LiveBP.Debug.ClearLocks"),
	TEXT("Clear all node locks (admin only)"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ClearLocks)
);

static FAutoConsoleCommand SimulateLatencyCmd(
	TEXT("LiveBP.Debug.SimulateLatency"),
	TEXT("Simulate network latency in milliseconds"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::SimulateLatency)
);

static FAutoConsoleCommand DumpMessagesCmd(
	TEXT("LiveBP.Debug.DumpMessages"),
	TEXT("Dump recent collaboration messages to log"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::DumpMessages)
);

static FAutoConsoleCommand RunTestsCmd(
	TEXT("LiveBP.Debug.RunTests"),
	TEXT("Run Live Blueprint test suite"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::RunTests)
);

static FAutoConsoleCommand ToggleDebugModeCmd(
	TEXT("LiveBP.Debug.ToggleDebugMode"),
	TEXT("Toggle debug visualization mode"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FLiveBPConsoleCommands::ToggleDebugMode)
);

static FAutoConsoleCommand ShowHelpCmd(
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

	// Get MUE integration for stats
	if (ULiveBPMUEIntegration* MUEIntegration = EditorSubsystem->GetMUEIntegration())
	{
		bool bConnected = MUEIntegration->IsConnected();
		FString CurrentUserId = MUEIntegration->GetCurrentUserId();
		TArray<FString> ConnectedUsers = MUEIntegration->GetConnectedUsers();
		
		UE_LOG(LogLiveBPEditor, Log, TEXT("=== Live Blueprint Stats ==="));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Collaboration Enabled: %s"), EditorSubsystem->IsCollaborationEnabled() ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogLiveBPEditor, Log, TEXT("MUE Connected: %s"), bConnected ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Current User: %s"), *CurrentUserId);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Connected Users: %d"), ConnectedUsers.Num());
		UE_LOG(LogLiveBPEditor, Log, TEXT("Debug Mode: %s"), EditorSubsystem->IsDebugModeEnabled() ? TEXT("ENABLED") : TEXT("DISABLED"));
		
		// Print connected users
		for (const FString& User : ConnectedUsers)
		{
			UE_LOG(LogLiveBPEditor, Log, TEXT("  - %s"), *User);
		}
		
		// Also print to screen
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, 
				FString::Printf(TEXT("LiveBP: %s, %d users connected"), 
					bConnected ? TEXT("Connected") : TEXT("Disconnected"), ConnectedUsers.Num()));
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
	if (ULiveBPMUEIntegration* MUEIntegration = EditorSubsystem->GetMUEIntegration())
	{
		bool bConnected = MUEIntegration->IsConnected();
		bool bHasSession = MUEIntegration->HasActiveSession();
		FString CurrentUserId = MUEIntegration->GetCurrentUserId();
		TArray<FString> ConnectedUsers = MUEIntegration->GetConnectedUsers();
		
		UE_LOG(LogLiveBPEditor, Log, TEXT("=== MUE Connection Test ==="));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Connected: %s"), bConnected ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Has Session: %s"), bHasSession ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogLiveBPEditor, Log, TEXT("User ID: %s"), *CurrentUserId);
		UE_LOG(LogLiveBPEditor, Log, TEXT("Users: %d"), ConnectedUsers.Num());
		
		if (GEngine)
		{
			FColor StatusColor = bConnected ? FColor::Green : FColor::Red;
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, StatusColor, 
				FString::Printf(TEXT("MUE Status: %s (%d users)"), 
					bConnected ? TEXT("Connected") : TEXT("Disconnected"), ConnectedUsers.Num()));
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

	// Clear all locks - this accesses the private NodeLocks map through a public method we need to add
	UE_LOG(LogLiveBPEditor, Warning, TEXT("Clear locks functionality requires additional implementation"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("Use Blueprint Editor to manually release locks"));
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
			TEXT("Clear locks feature pending implementation"));
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

	// For now, just log the simulated latency
	UE_LOG(LogLiveBPEditor, Log, TEXT("Latency simulation feature pending implementation"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("Would simulate %.1fms latency"), LatencyMs);
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, 
			FString::Printf(TEXT("Latency simulation: %.1fms (placeholder)"), LatencyMs));
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
	UE_LOG(LogLiveBPEditor, Log, TEXT("Message history feature pending implementation"));
	UE_LOG(LogLiveBPEditor, Log, TEXT("Check the Output Log for LiveBPEditor and LiveBPCore categories"));
}

void FLiveBPConsoleCommands::RunTests(const TArray<FString>& Args)
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("Running Live Blueprint test suite..."));
	
	// For now, run basic validation tests
	ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOG(LogLiveBPEditor, Error, TEXT("TEST FAILED: Editor Subsystem not available"));
		return;
	}

	bool bTestsPassed = true;
	int32 TestsRun = 0;
	int32 TestsPassed = 0;

	// Test 1: Subsystem initialization
	TestsRun++;
	if (EditorSubsystem->GetMUEIntegration())
	{
		UE_LOG(LogLiveBPEditor, Log, TEXT("✓ MUE Integration initialized"));
		TestsPassed++;
	}
	else
	{
		UE_LOG(LogLiveBPEditor, Error, TEXT("✗ MUE Integration not initialized"));
		bTestsPassed = false;
	}

	// Test 2: Collaboration toggle
	TestsRun++;
	bool bOriginalState = EditorSubsystem->IsCollaborationEnabled();
	EditorSubsystem->ToggleCollaboration();
	if (EditorSubsystem->IsCollaborationEnabled() != bOriginalState)
	{
		EditorSubsystem->ToggleCollaboration(); // Restore original state
		UE_LOG(LogLiveBPEditor, Log, TEXT("✓ Collaboration toggle works"));
		TestsPassed++;
	}
	else
	{
		UE_LOG(LogLiveBPEditor, Error, TEXT("✗ Collaboration toggle failed"));
		bTestsPassed = false;
	}

	float SuccessRate = TestsRun > 0 ? (float)TestsPassed / TestsRun : 0.0f;
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("Test Results: %d/%d passed (%.1f%%)"), 
		TestsPassed, TestsRun, SuccessRate * 100.0f);
	
	if (GEngine)
	{
		FColor ResultColor = SuccessRate > 0.8f ? FColor::Green : FColor::Red;
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, ResultColor, 
			FString::Printf(TEXT("LiveBP Tests: %d/%d passed (%.1f%%)"), 
				TestsPassed, TestsRun, SuccessRate * 100.0f));
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
