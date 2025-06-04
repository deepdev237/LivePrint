
#include "LiveBPCollaborationPanel.h"
#include "LiveBPEditor.h"
#include "LiveBPEditorSubsystem.h"
#include "LiveBPSettings.h"
#include "LiveBPPerformanceMonitor.h"
#include "LiveBPMessageThrottler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "DesktopPlatformModule.h"

void SLiveBPCollaborationPanel::Construct(const FArguments& InArgs)
{
	EditorSubsystem = GEngine->GetEngineSubsystem<ULiveBPEditorSubsystem>();
	LastUpdateTime = 0.0f;
	SessionStartTime = FPlatformTime::Seconds();
	
	// Initialize user list
	ActiveUsers.Empty();
	
	// Initialize performance metrics
	PerformanceMetrics.Empty();
	
	ChildSlot
	[
		SNew(SVerticalBox)
		
		// Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Live Blueprint Collaboration")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Refresh")))
					.OnClicked(this, &SLiveBPCollaborationPanel::OnRefreshClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Clear Stats")))
					.OnClicked(this, &SLiveBPCollaborationPanel::OnClearStatisticsClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Export Diagnostics")))
					.OnClicked(this, &SLiveBPCollaborationPanel::OnExportDiagnosticsClicked)
				]
			]
		]
		
		// Session Statistics
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Session Statistics")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5, 0, 0)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(5)
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Session Time:")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(SessionTimeText, STextBlock)
							.Text(FText::FromString(TEXT("00:00:00")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Messages Sent:")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(MessageCountText, STextBlock)
							.Text(FText::FromString(TEXT("0")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]
					]
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Errors:")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(ErrorCountText, STextBlock)
							.Text(FText::FromString(TEXT("0")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor::Red)
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Network Latency: ")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(5, 0)
					.VAlign(VAlign_Center)
					[
						SAssignNew(NetworkLatencyBar, SProgressBar)
						.Percent(0.0f)
						.FillColorAndOpacity(FLinearColor::Green)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(NetworkLatencyText, STextBlock)
						.Text(FText::FromString(TEXT("0ms")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
				]
			]
		]
		
		// Main content area
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			SNew(SHorizontalBox)
			
			// Active Users
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.Padding(0, 0, 2.5f, 0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Active Users")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 2, 0, 5)
					[
						SNew(SSeparator)
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(UsersListView, SListView<TSharedPtr<FLiveBPActiveUser>>)
						.ListItemsSource(&ActiveUsers)
						.OnGenerateRow(this, &SLiveBPCollaborationPanel::OnGenerateUserRow)
						.OnSelectionChanged(this, &SLiveBPCollaborationPanel::OnUserSelectionChanged)
						.SelectionMode(ESelectionMode::Single)
					]
				]
			]
			
			// Performance Metrics
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.Padding(2.5f, 0, 0, 0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Performance Metrics")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 2, 0, 5)
					[
						SNew(SSeparator)
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(MetricsListView, SListView<TSharedPtr<FLiveBPPerformanceMetric>>)
						.ListItemsSource(&PerformanceMetrics)
						.OnGenerateRow(this, &SLiveBPCollaborationPanel::OnGenerateMetricRow)
						.SelectionMode(ESelectionMode::None)
					]
				]
			]
		]
	];
	
	// Initial update
	UpdateCollaborationState();
	UpdatePerformanceMetrics();
}

void SLiveBPCollaborationPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	// Update every second
	if (InCurrentTime - LastUpdateTime > 1.0f)
	{
		UpdateCollaborationState();
		UpdatePerformanceMetrics();
		UpdateSessionStatistics();
		LastUpdateTime = InCurrentTime;
	}
}

void SLiveBPCollaborationPanel::UpdateCollaborationState()
{
	if (!EditorSubsystem.IsValid())
		return;
	
	// Update active users list
	// This would get the actual user list from the editor subsystem
	// For now, we'll simulate some users for demonstration
	
	// Example: Add test users if none exist
	if (ActiveUsers.Num() == 0)
	{
		ActiveUsers.Add(MakeShareable(new FLiveBPActiveUser(TEXT("User1"), TEXT("Alice"), FLinearColor::Blue)));
		ActiveUsers.Add(MakeShareable(new FLiveBPActiveUser(TEXT("User2"), TEXT("Bob"), FLinearColor::Green)));
		ActiveUsers.Add(MakeShareable(new FLiveBPActiveUser(TEXT("User3"), TEXT("Charlie"), FLinearColor::Red)));
	}
	
	// Update user activity and lock counts
	for (auto& User : ActiveUsers)
	{
		if (User.IsValid())
		{
			// This would get actual data from the collaboration system
			User->LastActivity = FPlatformTime::Seconds() - FMath::RandRange(0.0f, 30.0f);
			User->LockedNodes = FMath::RandRange(0, 3);
			User->bIsOnline = (FPlatformTime::Seconds() - User->LastActivity) < 60.0f;
		}
	}
	
	// Refresh the list view
	if (UsersListView.IsValid())
	{
		UsersListView->RequestListRefresh();
	}
}

void SLiveBPCollaborationPanel::UpdateActiveUser(const FString& UserId, const FString& DisplayName, const FLinearColor& Color, bool bIsOnline)
{
	// Find existing user or create new one
	TSharedPtr<FLiveBPActiveUser>* ExistingUser = ActiveUsers.FindByPredicate([&UserId](const TSharedPtr<FLiveBPActiveUser>& User) {
		return User.IsValid() && User->UserId == UserId;
	});
	
	if (ExistingUser && ExistingUser->IsValid())
	{
		(*ExistingUser)->DisplayName = DisplayName;
		(*ExistingUser)->UserColor = Color;
		(*ExistingUser)->bIsOnline = bIsOnline;
		(*ExistingUser)->LastActivity = FPlatformTime::Seconds();
	}
	else
	{
		ActiveUsers.Add(MakeShareable(new FLiveBPActiveUser(UserId, DisplayName, Color)));
	}
	
	if (UsersListView.IsValid())
	{
		UsersListView->RequestListRefresh();
	}
}

void SLiveBPCollaborationPanel::RemoveActiveUser(const FString& UserId)
{
	ActiveUsers.RemoveAll([&UserId](const TSharedPtr<FLiveBPActiveUser>& User) {
		return User.IsValid() && User->UserId == UserId;
	});
	
	if (UsersListView.IsValid())
	{
		UsersListView->RequestListRefresh();
	}
}

void SLiveBPCollaborationPanel::UpdatePerformanceMetrics()
{
	PerformanceMetrics.Empty();
	
	auto& Monitor = FLiveBPPerformanceMonitor::Get();
	auto& Throttler = FLiveBPMessageThrottler::Get();
	
	// Wire Preview Latency
	auto WireLatencyStats = Monitor.GetLatencyStats(ELiveBPMessageType::WirePreview);
	FString WireLatency = FormatPerformanceValue(WireLatencyStats.Average * 1000.0f, TEXT("ms"));
	FString WireLatencyAvg = FormatPerformanceValue(WireLatencyStats.Average * 1000.0f, TEXT("ms"));
	FLinearColor WireLatencyColor = GetMetricStatusColor(TEXT("Latency"), WireLatencyStats.Average * 1000.0f);
	PerformanceMetrics.Add(MakeShareable(new FLiveBPPerformanceMetric(
		TEXT("Wire Preview Latency"), WireLatency, WireLatencyAvg, TEXT("Good"), WireLatencyColor)));
	
	// Node Operation Latency
	auto NodeLatencyStats = Monitor.GetLatencyStats(ELiveBPMessageType::NodeOperation);
	FString NodeLatency = FormatPerformanceValue(NodeLatencyStats.Average * 1000.0f, TEXT("ms"));
	FString NodeLatencyAvg = FormatPerformanceValue(NodeLatencyStats.Average * 1000.0f, TEXT("ms"));
	FLinearColor NodeLatencyColor = GetMetricStatusColor(TEXT("Latency"), NodeLatencyStats.Average * 1000.0f);
	PerformanceMetrics.Add(MakeShareable(new FLiveBPPerformanceMetric(
		TEXT("Node Operation Latency"), NodeLatency, NodeLatencyAvg, TEXT("Good"), NodeLatencyColor)));
	
	// Throughput
	auto ThroughputStats = Monitor.GetThroughputStats(ELiveBPMessageType::WirePreview);
	FString Throughput = FormatPerformanceValue(ThroughputStats.Average, TEXT("msg/s"));
	FString ThroughputAvg = FormatPerformanceValue(ThroughputStats.Average, TEXT("msg/s"));
	PerformanceMetrics.Add(MakeShareable(new FLiveBPPerformanceMetric(
		TEXT("Message Throughput"), Throughput, ThroughputAvg, TEXT("Normal"), FLinearColor::Green)));
	
	// Memory Usage
	FLiveBPPerformanceMonitor::FMemoryStats MemoryStats = Monitor.GetMemoryStats();
	FString MemoryUsage = FormatPerformanceValue(MemoryStats.CurrentUsage / (1024.0f * 1024.0f), TEXT("MB"));
	FString MemoryPeak = FormatPerformanceValue(MemoryStats.PeakUsage / (1024.0f * 1024.0f), TEXT("MB"));
	FLinearColor MemoryColor = GetMetricStatusColor(TEXT("Memory"), MemoryStats.CurrentUsage / (1024.0f * 1024.0f));
	PerformanceMetrics.Add(MakeShareable(new FLiveBPPerformanceMetric(
		TEXT("Memory Usage"), MemoryUsage, MemoryPeak, TEXT("Normal"), MemoryColor)));
	
	// Error Rate
	auto ErrorStats = Monitor.GetErrorStats(ELiveBPMessageType::WirePreview);
	float ErrorRate = ThroughputStats.SampleCount > 0 ? (float)ErrorStats.TotalErrors / ThroughputStats.SampleCount * 100.0f : 0.0f;
	FString ErrorRateStr = FormatPerformanceValue(ErrorRate, TEXT("%"));
	FLinearColor ErrorColor = ErrorRate > 5.0f ? FLinearColor::Red : (ErrorRate > 1.0f ? FLinearColor::Yellow : FLinearColor::Green);
	PerformanceMetrics.Add(MakeShareable(new FLiveBPPerformanceMetric(
		TEXT("Error Rate"), ErrorRateStr, ErrorRateStr, TEXT("Low"), ErrorColor)));
	
	// Message Queue Size
	// This would get actual queue size from the system
	int32 QueueSize = 0; // Placeholder
	FString QueueSizeStr = FString::Printf(TEXT("%d"), QueueSize);
	FLinearColor QueueColor = QueueSize > 100 ? FLinearColor::Red : (QueueSize > 50 ? FLinearColor::Yellow : FLinearColor::Green);
	PerformanceMetrics.Add(MakeShareable(new FLiveBPPerformanceMetric(
		TEXT("Message Queue Size"), QueueSizeStr, QueueSizeStr, TEXT("Normal"), QueueColor)));
	
	if (MetricsListView.IsValid())
	{
		MetricsListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SLiveBPCollaborationPanel::OnGenerateUserRow(TSharedPtr<FLiveBPActiveUser> User, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FLiveBPActiveUser>>, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 5, 0)
				[
					SNew(SBox)
					.WidthOverride(16)
					.HeightOverride(16)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
						.BorderBackgroundColor(User->UserColor)
						.Padding(0)
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(User->DisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(User->bIsOnline ? FLinearColor::White : FLinearColor::Gray)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("Locks: %d | %s"), 
							User->LockedNodes, User->bIsOnline ? TEXT("Online") : TEXT("Offline"))))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FLinearColor::Gray)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FormatDuration(FPlatformTime::Seconds() - User->LastActivity)))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(FLinearColor::Gray)
				]
			]
		];
}

TSharedRef<ITableRow> SLiveBPCollaborationPanel::OnGenerateMetricRow(TSharedPtr<FLiveBPPerformanceMetric> Metric, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FLiveBPPerformanceMetric>>, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Metric->MetricName))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("Current: %s"), *Metric->CurrentValue)))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(Metric->StatusColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10, 0, 0, 0)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("Avg: %s"), *Metric->AverageValue)))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.ColorAndOpacity(FLinearColor::Gray)
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Metric->Status))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(Metric->StatusColor)
				]
			]
		];
}

void SLiveBPCollaborationPanel::OnUserSelectionChanged(TSharedPtr<FLiveBPActiveUser> SelectedUser, ESelectInfo::Type SelectInfo)
{
	if (SelectedUser.IsValid())
	{
		// Could highlight this user's cursor/activities in the graph editor
		UE_LOG(LogLiveBPEditor, Log, TEXT("Selected user: %s"), *SelectedUser->DisplayName);
	}
}

void SLiveBPCollaborationPanel::UpdateSessionStatistics()
{
	// Update session time
	float SessionDuration = FPlatformTime::Seconds() - SessionStartTime;
	if (SessionTimeText.IsValid())
	{
		SessionTimeText->SetText(FText::FromString(FormatDuration(SessionDuration)));
	}
	
	// Update message count
	auto& Monitor = FLiveBPPerformanceMonitor::Get();
	auto ThroughputStats = Monitor.GetThroughputStats(ELiveBPMessageType::WirePreview);
	if (MessageCountText.IsValid())
	{
		MessageCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), ThroughputStats.SampleCount)));
	}
	
	// Update error count
	auto ErrorStats = Monitor.GetErrorStats(ELiveBPMessageType::WirePreview);
	if (ErrorCountText.IsValid())
	{
		ErrorCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), ErrorStats.TotalErrors)));
		ErrorCountText->SetColorAndOpacity(ErrorStats.TotalErrors > 0 ? FLinearColor::Red : FLinearColor::Green);
	}
	
	// Update network latency
	auto LatencyStats = Monitor.GetLatencyStats(ELiveBPMessageType::WirePreview);
	float LatencyMs = LatencyStats.Average * 1000.0f;
	if (NetworkLatencyBar.IsValid() && NetworkLatencyText.IsValid())
	{
		float LatencyPercent = FMath::Clamp(LatencyMs / 200.0f, 0.0f, 1.0f); // Scale to 200ms max
		NetworkLatencyBar->SetPercent(LatencyPercent);
		NetworkLatencyText->SetText(FText::FromString(FString::Printf(TEXT("%.1fms"), LatencyMs)));
		
		FLinearColor LatencyColor = LatencyMs > 100.0f ? FLinearColor::Red : 
									(LatencyMs > 50.0f ? FLinearColor::Yellow : FLinearColor::Green);
		NetworkLatencyBar->SetFillColorAndOpacity(LatencyColor);
	}
}

FString SLiveBPCollaborationPanel::FormatDuration(float Seconds) const
{
	int32 Hours = (int32)(Seconds / 3600.0f);
	int32 Minutes = (int32)((Seconds - Hours * 3600.0f) / 60.0f);
	int32 Secs = (int32)(Seconds - Hours * 3600.0f - Minutes * 60.0f);
	
	return FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Minutes, Secs);
}

FString SLiveBPCollaborationPanel::FormatPerformanceValue(float Value, const FString& Unit) const
{
	if (Value < 0.01f)
	{
		return FString::Printf(TEXT("0.00%s"), *Unit);
	}
	else if (Value < 1.0f)
	{
		return FString::Printf(TEXT("%.2f%s"), Value, *Unit);
	}
	else if (Value < 100.0f)
	{
		return FString::Printf(TEXT("%.1f%s"), Value, *Unit);
	}
	else
	{
		return FString::Printf(TEXT("%.0f%s"), Value, *Unit);
	}
}

FLinearColor SLiveBPCollaborationPanel::GetMetricStatusColor(const FString& MetricName, float Value) const
{
	if (MetricName == TEXT("Latency"))
	{
		if (Value > 100.0f) return FLinearColor::Red;
		if (Value > 50.0f) return FLinearColor::Yellow;
		return FLinearColor::Green;
	}
	else if (MetricName == TEXT("Memory"))
	{
		if (Value > 500.0f) return FLinearColor::Red;
		if (Value > 200.0f) return FLinearColor::Yellow;
		return FLinearColor::Green;
	}
	
	return FLinearColor::Green;
}

FReply SLiveBPCollaborationPanel::OnRefreshClicked()
{
	UpdateCollaborationState();
	UpdatePerformanceMetrics();
	UpdateSessionStatistics();
	
	FNotificationInfo Info(FText::FromString(TEXT("Collaboration panel refreshed")));
	Info.ExpireDuration = 2.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
	
	return FReply::Handled();
}

FReply SLiveBPCollaborationPanel::OnClearStatisticsClicked()
{
	auto& Monitor = FLiveBPPerformanceMonitor::Get();
	Monitor.ClearAllStats();
	
	SessionStartTime = FPlatformTime::Seconds();
	UpdatePerformanceMetrics();
	UpdateSessionStatistics();
	
	FNotificationInfo Info(FText::FromString(TEXT("Statistics cleared")));
	Info.ExpireDuration = 2.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
	
	return FReply::Handled();
}

FReply SLiveBPCollaborationPanel::OnExportDiagnosticsClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}
	
	TArray<FString> SaveFilenames;
	bool bSaved = DesktopPlatform->SaveFileDialog(
		nullptr,
		TEXT("Export LiveBP Diagnostics"),
		TEXT(""),
		TEXT("LiveBP_Diagnostics.json"),
		TEXT("JSON Files (*.json)|*.json"),
		EFileDialogFlags::None,
		SaveFilenames
	);
	
	if (bSaved && SaveFilenames.Num() > 0)
	{
		// Export diagnostics to JSON file
		// Implementation would serialize all metrics and session data
		FString DiagnosticsData = TEXT("{\n  \"session\": {\n    \"duration\": \"");
		DiagnosticsData += FormatDuration(FPlatformTime::Seconds() - SessionStartTime);
		DiagnosticsData += TEXT("\",\n    \"timestamp\": \"");
		DiagnosticsData += FDateTime::Now().ToString();
		DiagnosticsData += TEXT("\"\n  }\n}");
		
		FFileHelper::SaveStringToFile(DiagnosticsData, *SaveFilenames[0]);
		
		FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Diagnostics exported to %s"), *SaveFilenames[0])));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	
	return FReply::Handled();
}

// SLiveBPDiagnosticsWindow implementation

void SLiveBPDiagnosticsWindow::Construct(const FArguments& InArgs)
{
	LastLogUpdateTime = 0.0f;
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(0.7f)
		[
			SAssignNew(CollaborationPanel, SLiveBPCollaborationPanel)
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.3f)
		.Padding(0, 5, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Collaboration Log")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Clear")))
						.OnClicked(this, &SLiveBPDiagnosticsWindow::OnClearLogClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Export")))
						.OnClicked(this, &SLiveBPDiagnosticsWindow::OnExportLogClicked)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0, 5, 0, 0)
				[
					SAssignNew(MessageLogScrollBox, SScrollBox)
					.Orientation(Orient_Vertical)
					.ScrollBarAlwaysVisible(true)
				]
			]
		]
	];
	
	// Add initial log message
	AddLogMessage(TEXT("LiveBP Diagnostics Window opened"), FLinearColor::Green);
}

void SLiveBPDiagnosticsWindow::ShowDiagnosticsWindow()
{
	TSharedRef<SWindow> DiagnosticsWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("LiveBP Collaboration Diagnostics")))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(800, 600))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		[
			SNew(SLiveBPDiagnosticsWindow)
		];
	
	FSlateApplication::Get().AddWindow(DiagnosticsWindow);
}

void SLiveBPDiagnosticsWindow::AddLogMessage(const FString& Message, const FLinearColor& Color)
{
	if (!MessageLogScrollBox.IsValid())
		return;
	
	FString Timestamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
	FString FormattedMessage = FString::Printf(TEXT("[%s] %s"), *Timestamp, *Message);
	
	MessageLogScrollBox->AddSlot()
	.Padding(2)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FormattedMessage))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		.ColorAndOpacity(Color)
		.AutoWrapText(true)
	];
	
	// Limit number of messages
	if (MessageLogScrollBox->GetChildren()->Num() > MaxLogMessages)
	{
		MessageLogScrollBox->RemoveSlot(MessageLogScrollBox->GetChildren()->GetChildAt(0));
	}
	
	// Scroll to bottom
	MessageLogScrollBox->ScrollToEnd();
}

FReply SLiveBPDiagnosticsWindow::OnClearLogClicked()
{
	if (MessageLogScrollBox.IsValid())
	{
		MessageLogScrollBox->ClearChildren();
		AddLogMessage(TEXT("Log cleared"), FLinearColor::Yellow);
	}
	return FReply::Handled();
}

FReply SLiveBPDiagnosticsWindow::OnExportLogClicked()
{
	// Implementation would export log messages to file
	AddLogMessage(TEXT("Log export not yet implemented"), FLinearColor::Yellow);
	return FReply::Handled();
}

void SLiveBPDiagnosticsWindow::UpdateMessageLog()
{
	// This would be called to add new collaboration events to the log
	// Implementation would monitor the collaboration system for new events
}
