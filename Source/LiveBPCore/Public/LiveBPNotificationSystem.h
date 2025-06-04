#pragma once

#include "CoreMinimal.h"
#include "LiveBPDataTypes.h"
#include "Engine/EngineTypes.h"
#include "LiveBPNotificationSystem.generated.h"

UENUM(BlueprintType)
enum class ELiveBPNotificationType : uint8
{
	UserJoined,
	UserLeft,
	NodeLocked,
	NodeUnlocked,
	NodeAdded,
	NodeDeleted,
	NodeMoved,
	ConnectionMade,
	ConnectionBroken,
	ConflictResolved,
	SyncError,
	NetworkError
};

USTRUCT(BlueprintType)
struct LIVEBPCORE_API FLiveBPNotificationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	ELiveBPNotificationType NotificationType;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString UserId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString UserDisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid BlueprintId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float Timestamp;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float Duration; // How long to show the notification

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FLinearColor Color; // Color for the notification

	FLiveBPNotificationData()
		: NotificationType(ELiveBPNotificationType::UserJoined)
		, Timestamp(0.0f)
		, Duration(3.0f)
		, Color(FLinearColor::White)
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLiveBPNotification, const FLiveBPNotificationData&, NotificationData);

/**
 * System for managing and displaying collaboration notifications
 */
UCLASS(BlueprintType)
class LIVEBPCORE_API ULiveBPNotificationSystem : public UObject
{
	GENERATED_BODY()

public:
	ULiveBPNotificationSystem();

	/**
	 * Show a notification to the user
	 * @param NotificationData The notification data
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ShowNotification(const FLiveBPNotificationData& NotificationData);

	/**
	 * Create and show a user joined notification
	 * @param UserId The user ID
	 * @param UserDisplayName The user's display name
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ShowUserJoinedNotification(const FString& UserId, const FString& UserDisplayName);

	/**
	 * Create and show a user left notification
	 * @param UserId The user ID
	 * @param UserDisplayName The user's display name
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ShowUserLeftNotification(const FString& UserId, const FString& UserDisplayName);

	/**
	 * Create and show a node lock notification
	 * @param UserId The user who locked the node
	 * @param UserDisplayName The user's display name
	 * @param NodeId The node that was locked
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ShowNodeLockedNotification(const FString& UserId, const FString& UserDisplayName, const FGuid& NodeId);

	/**
	 * Create and show a conflict resolution notification
	 * @param ConflictType Description of the conflict
	 * @param Resolution Description of how it was resolved
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ShowConflictResolvedNotification(const FString& ConflictType, const FString& Resolution);

	/**
	 * Create and show an error notification
	 * @param ErrorMessage The error message
	 * @param bIsNetworkError Whether this is a network-related error
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ShowErrorNotification(const FString& ErrorMessage, bool bIsNetworkError = false);

	/**
	 * Clear all notifications
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void ClearAllNotifications();

	/**
	 * Set whether notifications are enabled
	 * @param bEnabled Whether notifications should be shown
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void SetNotificationsEnabled(bool bEnabled);

	/**
	 * Set the default duration for notifications
	 * @param Duration Duration in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	void SetDefaultNotificationDuration(float Duration);

	/**
	 * Get the number of active notifications
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	int32 GetActiveNotificationCount() const;

	/**
	 * Delegate called when a new notification is created
	 */
	UPROPERTY(BlueprintAssignable, Category = "LiveBP")
	FOnLiveBPNotification OnNotificationCreated;

	/**
	 * Get notification color for a specific type
	 * @param NotificationType The notification type
	 * @return Color for the notification
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	static FLinearColor GetNotificationColor(ELiveBPNotificationType NotificationType);

	/**
	 * Get notification message template for a specific type
	 * @param NotificationType The notification type
	 * @return Message template
	 */
	UFUNCTION(BlueprintCallable, Category = "LiveBP")
	static FString GetNotificationMessageTemplate(ELiveBPNotificationType NotificationType);

protected:
	/**
	 * Create a notification data structure
	 * @param NotificationType The type of notification
	 * @param UserId The user ID (optional)
	 * @param UserDisplayName The user display name (optional)
	 * @param Message Custom message (optional)
	 * @param NodeId Node ID (optional)
	 * @return Configured notification data
	 */
	FLiveBPNotificationData CreateNotificationData(
		ELiveBPNotificationType NotificationType,
		const FString& UserId = TEXT(""),
		const FString& UserDisplayName = TEXT(""),
		const FString& Message = TEXT(""),
		const FGuid& NodeId = FGuid()
	);

private:
	UPROPERTY()
	bool bNotificationsEnabled;

	UPROPERTY()
	float DefaultNotificationDuration;

	UPROPERTY()
	TArray<FLiveBPNotificationData> ActiveNotifications;

	// Timer handle for cleaning up expired notifications
	FTimerHandle CleanupTimerHandle;

	/**
	 * Clean up expired notifications
	 */
	void CleanupExpiredNotifications();

	/**
	 * Format a message template with parameters
	 * @param Template The message template
	 * @param UserId User ID to substitute
	 * @param UserDisplayName User display name to substitute
	 * @param NodeId Node ID to substitute
	 * @return Formatted message
	 */
	FString FormatNotificationMessage(const FString& Template, const FString& UserId, const FString& UserDisplayName, const FGuid& NodeId) const;
};
