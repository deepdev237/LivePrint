// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

/**
 * Console commands for Live Blueprint debugging and development
 */
class LIVEBPEDITOR_API FLiveBPConsoleCommands
{
public:
	static void RegisterConsoleCommands();
	static void UnregisterConsoleCommands();

private:
	// Console command executors
	static void ShowStats(const TArray<FString>& Args);
	static void TestConnection(const TArray<FString>& Args);
	static void ClearLocks(const TArray<FString>& Args);
	static void SimulateLatency(const TArray<FString>& Args);
	static void DumpMessages(const TArray<FString>& Args);
	static void RunTests(const TArray<FString>& Args);
	static void ToggleDebugMode(const TArray<FString>& Args);
	static void ShowHelp(const TArray<FString>& Args);

	static bool bCommandsRegistered;
};
