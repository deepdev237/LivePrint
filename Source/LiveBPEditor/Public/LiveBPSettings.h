#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LiveBPSettings.generated.h"

UCLASS(Config = EditorSettings, DefaultConfig, Category = "Live Blueprint")
class LIVEBPEDITOR_API ULiveBPSettings : public UObject
{
	GENERATED_BODY()

public:
	ULiveBPSettings();

	// Wire preview settings
	UPROPERTY(Config, EditAnywhere, Category = "Wire Previews", meta = (ClampMin = "1", ClampMax = "60"))
	int32 WirePreviewUpdateRate = 10; // Hz

	UPROPERTY(Config, EditAnywhere, Category = "Wire Previews")
	bool bShowRemoteWirePreviews = true;

	UPROPERTY(Config, EditAnywhere, Category = "Wire Previews")
	FLinearColor RemoteWirePreviewColor = FLinearColor(1.0f, 0.5f, 0.0f, 0.8f);

	// Lock settings
	UPROPERTY(Config, EditAnywhere, Category = "Node Locking", meta = (ClampMin = "5", ClampMax = "300"))
	float DefaultLockDuration = 30.0f; // Seconds

	UPROPERTY(Config, EditAnywhere, Category = "Node Locking", meta = (ClampMin = "1", ClampMax = "60"))
	float LockExtensionTime = 5.0f; // Seconds

	UPROPERTY(Config, EditAnywhere, Category = "Node Locking")
	bool bAutoRequestLockOnEdit = true;

	UPROPERTY(Config, EditAnywhere, Category = "Node Locking")
	bool bShowLockIndicators = true;

	UPROPERTY(Config, EditAnywhere, Category = "Node Locking")
	FLinearColor LockedNodeBorderColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Node Locking")
	FLinearColor OwnLockedNodeBorderColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

	// Performance settings
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxConcurrentUsers = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "10", ClampMax = "1000"))
	int32 MaxMessageQueueSize = 100;

	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	bool bThrottleMessages = true;

	// UI settings
	UPROPERTY(Config, EditAnywhere, Category = "User Interface")
	bool bShowCollaboratorCursors = true;

	UPROPERTY(Config, EditAnywhere, Category = "User Interface")
	bool bShowCollaboratorNames = true;

	UPROPERTY(Config, EditAnywhere, Category = "User Interface")
	bool bShowActivityNotifications = true;

	UPROPERTY(Config, EditAnywhere, Category = "User Interface", meta = (ClampMin = "1", ClampMax = "10"))
	float NotificationDuration = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category = "User Interface")
	bool bShowPerformanceStats = false;

	UPROPERTY(Config, EditAnywhere, Category = "User Interface")
	bool bShowNetworkLatency = true;

	// Testing settings
	UPROPERTY(Config, EditAnywhere, Category = "Testing")
	bool bEnableAutomatedTesting = false;

	UPROPERTY(Config, EditAnywhere, Category = "Testing")
	bool bRunTestsOnStartup = false;

	UPROPERTY(Config, EditAnywhere, Category = "Testing", meta = (ClampMin = "10", ClampMax = "10000"))
	int32 StressTestMessageCount = 1000;

	UPROPERTY(Config, EditAnywhere, Category = "Testing", meta = (ClampMin = "1", ClampMax = "20"))
	int32 StressTestUserCount = 5;

	// Advanced settings
	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bEnableConflictResolution = true;

	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bUseBinarySerializationForPreviews = true;

	UPROPERTY(Config, EditAnywhere, Category = "Advanced", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MinimumMovementThreshold = 0.1f; // Minimum movement in pixels to trigger update

	UPROPERTY(Config, EditAnywhere, Category = "Advanced", meta = (ClampMin = "10", ClampMax = "5000"))
	int32 MaxHistoryEntries = 500;

	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bAutoCleanupExpiredLocks = true;

	UPROPERTY(Config, EditAnywhere, Category = "Advanced", meta = (ClampMin = "1", ClampMax = "3600"))
	float HistoryCleanupInterval = 60.0f; // Seconds

	// Debug settings
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bEnableVerboseLogging = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogAllMessages = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowDebugOverlay = false;
};
