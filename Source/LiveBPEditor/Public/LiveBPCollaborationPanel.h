
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "LiveBPDataTypes.h"
#include "LiveBPPerformanceMonitor.h"

class SScrollBox;
class STextBlock;
class SProgressBar;
class ULiveBPEditorSubsystem;

/**
 * Data model for active user display
 */
struct FLiveBPActiveUser
{
	FString UserId;
	FString DisplayName;
	FLinearColor UserColor;
	FVector2D LastCursorPosition;
	bool bIsOnline;
	float LastActivity;
	int32 LockedNodes;
	
	FLiveBPActiveUser()
		: UserId(), DisplayName(), UserColor(FLinearColor::White)
		, LastCursorPosition(FVector2D::ZeroVector), bIsOnline(false)
		, LastActivity(0.0f), LockedNodes(0) {}
		
	FLiveBPActiveUser(const FString& InUserId, const FString& InDisplayName, const FLinearColor& InColor)
		: UserId(InUserId), DisplayName(InDisplayName), UserColor(InColor)
		, LastCursorPosition(FVector2D::ZeroVector), bIsOnline(true)
		, LastActivity(FPlatformTime::Seconds()), LockedNodes(0) {}
};

/**
 * Data model for performance metrics display
 */
struct FLiveBPPerformanceMetric
{
	FString MetricName;
	FString CurrentValue;
	FString AverageValue;
	FString Status;
	FLinearColor StatusColor;
	
	FLiveBPPerformanceMetric()
		: MetricName(), CurrentValue(), AverageValue(), Status()
		, StatusColor(FLinearColor::Green) {}
		
	FLiveBPPerformanceMetric(const FString& InName, const FString& InCurrent, const FString& InAverage, const FString& InStatus, const FLinearColor& InColor)
		: MetricName(InName), CurrentValue(InCurrent), AverageValue(InAverage), Status(InStatus), StatusColor(InColor) {}
};

/**
 * Collaboration panel widget showing active users, performance metrics, and session statistics
 */
class LIVEBPEDITOR_API SLiveBPCollaborationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveBPCollaborationPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	/** Update the panel with current collaboration state */
	void UpdateCollaborationState();
	
	/** Add or update an active user */
	void UpdateActiveUser(const FString& UserId, const FString& DisplayName, const FLinearColor& Color, bool bIsOnline);
	
	/** Remove a user from the active list */
	void RemoveActiveUser(const FString& UserId);
	
	/** Update performance metrics */
	void UpdatePerformanceMetrics();

protected:
	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Editor subsystem reference */
	TWeakObjectPtr<ULiveBPEditorSubsystem> EditorSubsystem;
	
	/** Active users list */
	TArray<TSharedPtr<FLiveBPActiveUser>> ActiveUsers;
	TSharedPtr<SListView<TSharedPtr<FLiveBPActiveUser>>> UsersListView;
	
	/** Performance metrics list */
	TArray<TSharedPtr<FLiveBPPerformanceMetric>> PerformanceMetrics;
	TSharedPtr<SListView<TSharedPtr<FLiveBPPerformanceMetric>>> MetricsListView;
	
	/** Session statistics */
	TSharedPtr<STextBlock> SessionTimeText;
	TSharedPtr<STextBlock> MessageCountText;
	TSharedPtr<STextBlock> ErrorCountText;
	TSharedPtr<SProgressBar> NetworkLatencyBar;
	TSharedPtr<STextBlock> NetworkLatencyText;
	
	/** Update timers */
	float LastUpdateTime;
	float SessionStartTime;
	
	/** Generate user list widget */
	TSharedRef<ITableRow> OnGenerateUserRow(TSharedPtr<FLiveBPActiveUser> User, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** Generate performance metric widget */
	TSharedRef<ITableRow> OnGenerateMetricRow(TSharedPtr<FLiveBPPerformanceMetric> Metric, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** Handle user selection in list */
	void OnUserSelectionChanged(TSharedPtr<FLiveBPActiveUser> SelectedUser, ESelectInfo::Type SelectInfo);
	
	/** Get session statistics */
	void UpdateSessionStatistics();
	
	/** Format time duration */
	FString FormatDuration(float Seconds) const;
	
	/** Format performance value */
	FString FormatPerformanceValue(float Value, const FString& Unit) const;
	
	/** Get status color for metric */
	FLinearColor GetMetricStatusColor(const FString& MetricName, float Value) const;
	
	/** Handle refresh button clicked */
	FReply OnRefreshClicked();
	
	/** Handle clear statistics button clicked */
	FReply OnClearStatisticsClicked();
	
	/** Handle export diagnostics button clicked */
	FReply OnExportDiagnosticsClicked();
};

/**
 * Factory for creating collaboration panel tabs
 */
class FLiveBPCollaborationPanelSummoner : public FWorkflowTabFactory
{
public:
	FLiveBPCollaborationPanelSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);
	
	// FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	/** Weak reference to the hosting app */
	TWeakPtr<class FAssetEditorToolkit> HostingApp;
};

/**
 * Live Blueprint collaboration diagnostics and monitoring window
 */
class LIVEBPEDITOR_API SLiveBPDiagnosticsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveBPDiagnosticsWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	/** Show the diagnostics window */
	static void ShowDiagnosticsWindow();

private:
	/** Main collaboration panel */
	TSharedPtr<SLiveBPCollaborationPanel> CollaborationPanel;
	
	/** Message log scroll box */
	TSharedPtr<SScrollBox> MessageLogScrollBox;
	
	/** Performance graph (placeholder for future advanced visualization) */
	TSharedPtr<SWidget> PerformanceGraph;
	
	/** Add log message */
	void AddLogMessage(const FString& Message, const FLinearColor& Color = FLinearColor::White);
	
	/** Clear log messages */
	FReply OnClearLogClicked();
	
	/** Export log to file */
	FReply OnExportLogClicked();
	
	/** Update message log with recent collaboration events */
	void UpdateMessageLog();
	
	/** Last log update time */
	float LastLogUpdateTime;
	
	/** Maximum number of log messages to keep */
	static constexpr int32 MaxLogMessages = 1000;
};
