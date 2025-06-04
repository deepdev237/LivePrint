
#include "LiveBPBlueprintEditorMode.h"
#include "LiveBPEditor.h"
#include "LiveBPEditorSubsystem.h"
#include "LiveBPSettings.h"
#include "LiveBPGraphEditor.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorTabs.h"
#include "BlueprintEditorModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "ToolMenus.h"

// Static member definitions for FLiveBPBlueprintEditorCustomization
FDelegateHandle FLiveBPBlueprintEditorCustomization::BlueprintEditorOpenedHandle;
TMap<TWeakPtr<FBlueprintEditor>, TSharedPtr<FLiveBPBlueprintEditorMode>> FLiveBPBlueprintEditorCustomization::ActiveModes;

FLiveBPBlueprintEditorMode::FLiveBPBlueprintEditorMode(TSharedPtr<FBlueprintEditor> InBlueprintEditor)
	: FBlueprintEditorApplicationMode(InBlueprintEditor, FBlueprintEditorApplicationModes::StandardBlueprintEditorMode)
	, BlueprintEditor(InBlueprintEditor)
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP Blueprint Editor Mode created"));
}

void FLiveBPBlueprintEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	if (!BlueprintEditorPtr.IsValid())
	{
		return;
	}
	
	// Call parent to register standard tabs
	FBlueprintEditorApplicationMode::RegisterTabFactories(InTabManager);
	
	// Register our custom graph editor tab factory
	BlueprintEditorPtr->DocumentManager->SetTabFactory(MakeShareable(new FLiveBPGraphEditorSummoner(BlueprintEditorPtr,
		FLiveBPGraphEditorSummoner::FOnCreateGraphEditorWidget::CreateSP(this, &FLiveBPBlueprintEditorMode::CreateGraphEditorWidget)
	)));
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP tab factories registered"));
}

void FLiveBPBlueprintEditorMode::PreDeactivateMode()
{
	// Clean up any active collaboration state
	for (auto& GraphEditorPair : GraphEditors)
	{
		if (GraphEditorPair.Value.IsValid())
		{
			// Notify other users that we're leaving
			// Implementation would send disconnect messages
		}
	}
	
	FBlueprintEditorApplicationMode::PreDeactivateMode();
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP mode deactivated"));
}

void FLiveBPBlueprintEditorMode::PostActivateMode()
{
	FBlueprintEditorApplicationMode::PostActivateMode();
	
	// Initialize collaboration state
	if (ULiveBPEditorSubsystem* EditorSubsystem = GEngine->GetEngineSubsystem<ULiveBPEditorSubsystem>())
	{
		EditorSubsystem->OnBlueprintEditorActivated(BlueprintEditor.Pin());
	}
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP mode activated"));
}

TSharedPtr<SLiveBPGraphEditor> FLiveBPBlueprintEditorMode::GetOrCreateGraphEditor(UEdGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}
	
	TWeakObjectPtr<UEdGraph> GraphPtr(Graph);
	if (TSharedPtr<SLiveBPGraphEditor>* ExistingEditor = GraphEditors.Find(GraphPtr))
	{
		if (ExistingEditor->IsValid())
		{
			return *ExistingEditor;
		}
		else
		{
			GraphEditors.Remove(GraphPtr);
		}
	}
	
	// Create new Live BP graph editor
	TSharedPtr<SLiveBPGraphEditor> NewGraphEditor;
	SAssignNew(NewGraphEditor, SLiveBPGraphEditor)
		.GraphToEdit(Graph)
		.IsEditable(true)
		.OnNodeDoubleClicked_Lambda([this](UEdGraphNode* Node) {
			if (BlueprintEditor.IsValid())
			{
				BlueprintEditor.Pin()->JumpToHyperlink(Node);
			}
		})
		.OnSelectionChanged_Lambda([this](const FGraphPanelSelectionSet& Selection) {
			// Handle collaborative selection
			if (ULiveBPEditorSubsystem* EditorSubsystem = GEngine->GetEngineSubsystem<ULiveBPEditorSubsystem>())
			{
				for (UObject* SelectedObject : Selection)
				{
					if (UEdGraphNode* Node = Cast<UEdGraphNode>(SelectedObject))
					{
						FGuid NodeId; // Would get actual node ID
						EditorSubsystem->BroadcastNodeSelection(NodeId, true);
					}
				}
			}
		});
	
	GraphEditors.Add(GraphPtr, NewGraphEditor);
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("Created LiveBP graph editor for graph: %s"), *Graph->GetName());
	
	return NewGraphEditor;
}

void FLiveBPBlueprintEditorMode::RemoveGraphEditor(UEdGraph* Graph)
{
	if (Graph)
	{
		GraphEditors.Remove(TWeakObjectPtr<UEdGraph>(Graph));
		UE_LOG(LogLiveBPEditor, Log, TEXT("Removed LiveBP graph editor for graph: %s"), *Graph->GetName());
	}
}

TArray<TSharedPtr<SLiveBPGraphEditor>> FLiveBPBlueprintEditorMode::GetAllGraphEditors() const
{
	TArray<TSharedPtr<SLiveBPGraphEditor>> Result;
	for (const auto& GraphEditorPair : GraphEditors)
	{
		if (GraphEditorPair.Value.IsValid())
		{
			Result.Add(GraphEditorPair.Value);
		}
	}
	return Result;
}

TSharedRef<SGraphEditor> FLiveBPBlueprintEditorMode::CreateGraphEditorWidget(UEdGraph* Graph)
{
	TSharedPtr<SLiveBPGraphEditor> LiveBPEditor = GetOrCreateGraphEditor(Graph);
	if (LiveBPEditor.IsValid() && LiveBPEditor->GetGraphEditor().IsValid())
	{
		return LiveBPEditor->GetGraphEditor().ToSharedRef();
	}
	
	// Fallback to standard graph editor if LiveBP creation fails
	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
	return GraphEditorModule.CreateGraphEditor(Graph);
}

// FLiveBPGraphEditorSummoner implementation

FLiveBPGraphEditorSummoner::FLiveBPGraphEditorSummoner(TSharedPtr<FBlueprintEditor> InBlueprintEditor, FOnCreateGraphEditorWidget InOnCreateGraphEditorWidget)
	: FDocumentTabFactoryForObjects<UEdGraph>(FBlueprintEditorTabs::GraphEditorID, InBlueprintEditor)
	, BlueprintEditor(InBlueprintEditor)
	, OnCreateGraphEditorWidget(InOnCreateGraphEditorWidget)
{
}

void FLiveBPGraphEditorSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	BlueprintEditor.Pin()->OnGraphEditorFocused(GraphEditor);
}

void FLiveBPGraphEditorSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	BlueprintEditor.Pin()->OnGraphEditorBackgrounded(GraphEditor);
}

void FLiveBPGraphEditorSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	GraphEditor->NotifyGraphChanged();
}

void FLiveBPGraphEditorSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	
	FVector2D ViewLocation;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);
	
	UEdGraph* Graph = FTabPayload_UObject::CastChecked<UEdGraph>(Payload);
	BlueprintEditor.Pin()->GetBlueprintObj()->LastEditedDocuments.Add(FEditedDocumentInfo(Graph, ViewLocation, ZoomAmount));
}

TAttribute<FText> FLiveBPGraphEditorSummoner::ConstructTabNameForObject(UEdGraph* DocumentID) const
{
	return TAttribute<FText>(FText::FromString(DocumentID->GetName()));
}

TSharedRef<SWidget> FLiveBPGraphEditorSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return OnCreateGraphEditorWidget.ExecuteIfBound(DocumentID);
}

const FSlateBrush* FLiveBPGraphEditorSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return FAppStyle::GetBrush("GraphEditor.EventGraph_16x");
}

void FLiveBPGraphEditorSummoner::OnTabClosed(TSharedRef<SDockTab> Tab, UEdGraph* DocumentID) const
{
	// Notify collaboration system that tab was closed
	if (ULiveBPEditorSubsystem* EditorSubsystem = GEngine->GetEngineSubsystem<ULiveBPEditorSubsystem>())
	{
		EditorSubsystem->OnGraphEditorClosed(DocumentID);
	}
}

// FLiveBPBlueprintEditorCustomization implementation

void FLiveBPBlueprintEditorCustomization::Initialize()
{
	// Hook into Blueprint editor creation
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("KismetCompiler");
	BlueprintEditorOpenedHandle = BlueprintEditorModule.OnBlueprintEditorOpened().AddStatic(&FLiveBPBlueprintEditorCustomization::OnBlueprintEditorOpened);
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP Blueprint Editor Customization initialized"));
}

void FLiveBPBlueprintEditorCustomization::Shutdown()
{
	// Clean up all active modes
	for (auto& ModePair : ActiveModes)
	{
		if (ModePair.Value.IsValid())
		{
			// Mode cleanup will be handled by Blueprint editor shutdown
		}
	}
	ActiveModes.Empty();
	
	// Unhook from Blueprint editor creation
	if (BlueprintEditorOpenedHandle.IsValid())
	{
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("KismetCompiler");
		BlueprintEditorModule.OnBlueprintEditorOpened().Remove(BlueprintEditorOpenedHandle);
		BlueprintEditorOpenedHandle.Reset();
	}
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP Blueprint Editor Customization shutdown"));
}

bool FLiveBPBlueprintEditorCustomization::IsLiveBPEnabled(TSharedPtr<FBlueprintEditor> BlueprintEditor)
{
	return ActiveModes.Contains(BlueprintEditor);
}

void FLiveBPBlueprintEditorCustomization::EnableLiveBP(TSharedPtr<FBlueprintEditor> BlueprintEditor)
{
	if (!BlueprintEditor.IsValid() || IsLiveBPEnabled(BlueprintEditor))
	{
		return;
	}
	
	// Check if Live BP is enabled in settings
	const ULiveBPSettings* Settings = GetDefault<ULiveBPSettings>();
	if (!Settings || !Settings->bEnableCollaboration)
	{
		UE_LOG(LogLiveBPEditor, Warning, TEXT("LiveBP collaboration is disabled in settings"));
		return;
	}
	
	// Create and register Live BP mode
	TSharedPtr<FLiveBPBlueprintEditorMode> LiveBPMode = MakeShareable(new FLiveBPBlueprintEditorMode(BlueprintEditor));
	ActiveModes.Add(BlueprintEditor, LiveBPMode);
	
	// Add the mode to the Blueprint editor
	BlueprintEditor->AddApplicationMode(
		FLiveBPBlueprintEditorMode::LiveBPModeID, 
		LiveBPMode.ToSharedRef()
	);
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP enabled for Blueprint editor"));
}

void FLiveBPBlueprintEditorCustomization::DisableLiveBP(TSharedPtr<FBlueprintEditor> BlueprintEditor)
{
	if (!BlueprintEditor.IsValid() || !IsLiveBPEnabled(BlueprintEditor))
	{
		return;
	}
	
	// Remove the mode from the Blueprint editor
	BlueprintEditor->RemoveApplicationMode(FLiveBPBlueprintEditorMode::LiveBPModeID);
	
	// Clean up our tracking
	ActiveModes.Remove(BlueprintEditor);
	
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBP disabled for Blueprint editor"));
}

void FLiveBPBlueprintEditorCustomization::OnBlueprintEditorOpened(EBlueprintType BlueprintType, TSharedPtr<FBlueprintEditor> BlueprintEditor)
{
	if (!BlueprintEditor.IsValid())
	{
		return;
	}
	
	// Check if this Blueprint should have Live BP enabled
	const ULiveBPSettings* Settings = GetDefault<ULiveBPSettings>();
	if (Settings && Settings->bEnableCollaboration && Settings->bAutoEnableForNewBlueprints)
	{
		// Enable Live BP for this editor
		EnableLiveBP(BlueprintEditor);
		
		// Set up collaboration session if MUE is available
		if (ULiveBPEditorSubsystem* EditorSubsystem = GEngine->GetEngineSubsystem<ULiveBPEditorSubsystem>())
		{
			EditorSubsystem->OnBlueprintEditorActivated(BlueprintEditor);
		}
	}
}

void FLiveBPBlueprintEditorCustomization::OnBlueprintEditorClosed(TSharedPtr<FBlueprintEditor> BlueprintEditor)
{
	if (IsLiveBPEnabled(BlueprintEditor))
	{
		DisableLiveBP(BlueprintEditor);
	}
}

// Add mode ID constant
const FName FLiveBPBlueprintEditorMode::LiveBPModeID("LiveBPMode");
