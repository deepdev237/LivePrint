#include "LiveBPNotificationSystem.h"
#include "LiveBPCore.h"
#include "Engine/World.h"
#include "TimerManager.h"

ULiveBPNotificationSystem::ULiveBPNotificationSystem()
	: bNotificationsEnabled(true)
	, DefaultNotificationDuration(3.0f)
{
}

void ULiveBPNotificationSystem::ShowNotification(const FLiveBPNotificationData& NotificationData)
{
	if (!bNotificationsEnabled)
	{
		return;
	}

	// Log the notification
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Notification: %s"), *NotificationData.Message);

	// Add to active notifications
	ActiveNotifications.Add(NotificationData);

	// Broadcast to delegates
	OnNotificationCreated.Broadcast(NotificationData);

	// Set up cleanup timer if we haven't already
	if (!CleanupTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CleanupTimerHandle,
				this,
				&ULiveBPNotificationSystem::CleanupExpiredNotifications,
				1.0f, // Check every second
				true   // Loop
			);
		}
	}
}

void ULiveBPNotificationSystem::ShowUserJoinedNotification(const FString& UserId, const FString& UserDisplayName)
{
	FLiveBPNotificationData NotificationData = CreateNotificationData(
		ELiveBPNotificationType::UserJoined,
		UserId,
		UserDisplayName
	);
	
	ShowNotification(NotificationData);
}

void ULiveBPNotificationSystem::ShowUserLeftNotification(const FString& UserId, const FString& UserDisplayName)
{
	FLiveBPNotificationData NotificationData = CreateNotificationData(
		ELiveBPNotificationType::UserLeft,
		UserId,
		UserDisplayName
	);
	
	ShowNotification(NotificationData);
}

void ULiveBPNotificationSystem::ShowNodeLockedNotification(const FString& UserId, const FString& UserDisplayName, const FGuid& NodeId)
{
	FLiveBPNotificationData NotificationData = CreateNotificationData(
		ELiveBPNotificationType::NodeLocked,
		UserId,
		UserDisplayName,
		TEXT(""),
		NodeId
	);
	
	ShowNotification(NotificationData);
}

void ULiveBPNotificationSystem::ShowConflictResolvedNotification(const FString& ConflictType, const FString& Resolution)
{
	FLiveBPNotificationData NotificationData = CreateNotificationData(
		ELiveBPNotificationType::ConflictResolved,
		TEXT(""),
		TEXT(""),
		FString::Printf(TEXT("Conflict resolved: %s - %s"), *ConflictType, *Resolution)
	);
	
	ShowNotification(NotificationData);
}

void ULiveBPNotificationSystem::ShowErrorNotification(const FString& ErrorMessage, bool bIsNetworkError)
{
	ELiveBPNotificationType NotificationType = bIsNetworkError ? 
		ELiveBPNotificationType::NetworkError : 
		ELiveBPNotificationType::SyncError;
	
	FLiveBPNotificationData NotificationData = CreateNotificationData(
		NotificationType,
		TEXT(""),
		TEXT(""),
		ErrorMessage
	);
	
	// Error notifications last longer
	NotificationData.Duration = 5.0f;
	
	ShowNotification(NotificationData);
}

void ULiveBPNotificationSystem::ClearAllNotifications()
{
	ActiveNotifications.Empty();
	
	// Clear cleanup timer
	if (CleanupTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CleanupTimerHandle);
		}
		CleanupTimerHandle.Invalidate();
	}
	
	UE_LOG(LogLiveBPCore, Log, TEXT("All LiveBP notifications cleared"));
}

void ULiveBPNotificationSystem::SetNotificationsEnabled(bool bEnabled)
{
	bNotificationsEnabled = bEnabled;
	
	if (!bEnabled)
	{
		ClearAllNotifications();
	}
	
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP notifications %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void ULiveBPNotificationSystem::SetDefaultNotificationDuration(float Duration)
{
	DefaultNotificationDuration = FMath::Max(0.5f, Duration);
}

int32 ULiveBPNotificationSystem::GetActiveNotificationCount() const
{
	return ActiveNotifications.Num();
}

FLinearColor ULiveBPNotificationSystem::GetNotificationColor(ELiveBPNotificationType NotificationType)
{
	switch (NotificationType)
	{
	case ELiveBPNotificationType::UserJoined:
		return FLinearColor::Green;
	case ELiveBPNotificationType::UserLeft:
		return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange
	case ELiveBPNotificationType::NodeLocked:
		return FLinearColor::Yellow;
	case ELiveBPNotificationType::NodeUnlocked:
		return FLinearColor(0.5f, 1.0f, 0.5f, 1.0f); // Light green
	case ELiveBPNotificationType::NodeAdded:
	case ELiveBPNotificationType::ConnectionMade:
		return FLinearColor::Blue;
	case ELiveBPNotificationType::NodeDeleted:
	case ELiveBPNotificationType::ConnectionBroken:
		return FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); // Light red
	case ELiveBPNotificationType::NodeMoved:
		return FLinearColor::Cyan;
	case ELiveBPNotificationType::ConflictResolved:
		return FLinearColor(0.8f, 0.8f, 0.0f, 1.0f); // Yellow-green
	case ELiveBPNotificationType::SyncError:
	case ELiveBPNotificationType::NetworkError:
		return FLinearColor::Red;
	default:
		return FLinearColor::White;
	}
}

FString ULiveBPNotificationSystem::GetNotificationMessageTemplate(ELiveBPNotificationType NotificationType)
{
	switch (NotificationType)
	{
	case ELiveBPNotificationType::UserJoined:
		return TEXT("{UserDisplayName} joined the collaboration session");
	case ELiveBPNotificationType::UserLeft:
		return TEXT("{UserDisplayName} left the collaboration session");
	case ELiveBPNotificationType::NodeLocked:
		return TEXT("{UserDisplayName} locked a node");
	case ELiveBPNotificationType::NodeUnlocked:
		return TEXT("{UserDisplayName} unlocked a node");
	case ELiveBPNotificationType::NodeAdded:
		return TEXT("{UserDisplayName} added a node");
	case ELiveBPNotificationType::NodeDeleted:
		return TEXT("{UserDisplayName} deleted a node");
	case ELiveBPNotificationType::NodeMoved:
		return TEXT("{UserDisplayName} moved a node");
	case ELiveBPNotificationType::ConnectionMade:
		return TEXT("{UserDisplayName} connected pins");
	case ELiveBPNotificationType::ConnectionBroken:
		return TEXT("{UserDisplayName} disconnected pins");
	case ELiveBPNotificationType::ConflictResolved:
		return TEXT("Collaboration conflict resolved");
	case ELiveBPNotificationType::SyncError:
		return TEXT("Synchronization error occurred");
	case ELiveBPNotificationType::NetworkError:
		return TEXT("Network error occurred");
	default:
		return TEXT("Unknown notification");
	}
}

FLiveBPNotificationData ULiveBPNotificationSystem::CreateNotificationData(
	ELiveBPNotificationType NotificationType,
	const FString& UserId,
	const FString& UserDisplayName,
	const FString& Message,
	const FGuid& NodeId)
{
	FLiveBPNotificationData NotificationData;
	NotificationData.NotificationType = NotificationType;
	NotificationData.UserId = UserId;
	NotificationData.UserDisplayName = UserDisplayName;
	NotificationData.NodeId = NodeId;
	NotificationData.Timestamp = FPlatformTime::Seconds();
	NotificationData.Duration = DefaultNotificationDuration;
	NotificationData.Color = GetNotificationColor(NotificationType);
	
	// Use custom message or format template
	if (!Message.IsEmpty())
	{
		NotificationData.Message = Message;
	}
	else
	{
		FString Template = GetNotificationMessageTemplate(NotificationType);
		NotificationData.Message = FormatNotificationMessage(Template, UserId, UserDisplayName, NodeId);
	}
	
	return NotificationData;
}

void ULiveBPNotificationSystem::CleanupExpiredNotifications()
{
	float CurrentTime = FPlatformTime::Seconds();
	
	// Remove expired notifications
	int32 RemovedCount = ActiveNotifications.RemoveAll([CurrentTime](const FLiveBPNotificationData& Notification)
	{
		return (CurrentTime - Notification.Timestamp) > Notification.Duration;
	});
	
	if (RemovedCount > 0)
	{
		UE_LOG(LogLiveBPCore, VeryVerbose, TEXT("Cleaned up %d expired notifications"), RemovedCount);
	}
	
	// Clear timer if no active notifications
	if (ActiveNotifications.Num() == 0 && CleanupTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CleanupTimerHandle);
		}
		CleanupTimerHandle.Invalidate();
	}
}

FString ULiveBPNotificationSystem::FormatNotificationMessage(const FString& Template, const FString& UserId, const FString& UserDisplayName, const FGuid& NodeId) const
{
	FString FormattedMessage = Template;
	
	// Replace placeholders
	FormattedMessage = FormattedMessage.Replace(TEXT("{UserId}"), *UserId);
	FormattedMessage = FormattedMessage.Replace(TEXT("{UserDisplayName}"), *UserDisplayName);
	FormattedMessage = FormattedMessage.Replace(TEXT("{NodeId}"), *NodeId.ToString());
	
	return FormattedMessage;
}
