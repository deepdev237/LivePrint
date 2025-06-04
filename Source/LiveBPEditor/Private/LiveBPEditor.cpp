#include "LiveBPEditor.h"
#include "LiveBPEditorSubsystem.h"
#include "LiveBPBlueprintEditorMode.h"
#include "LiveBPConsoleCommands.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

DEFINE_LOG_CATEGORY(LogLiveBPEditor);

void FLiveBPEditorModule::StartupModule()
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBPEditor module starting up"));

	// Register console commands
	FLiveBPConsoleCommands::RegisterConsoleCommands();

	// Initialize the editor subsystem
	if (ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>())
	{
		EditorSubsystem->Initialize();
	}

	// Initialize Blueprint editor customization
	FLiveBPBlueprintEditorCustomization::Initialize();

	RegisterMenuExtensions();
}

void FLiveBPEditorModule::ShutdownModule()
{
	UE_LOG(LogLiveBPEditor, Log, TEXT("LiveBPEditor module shutting down"));

	// Unregister console commands
	FLiveBPConsoleCommands::UnregisterConsoleCommands();

	UnregisterMenuExtensions();

	// Shutdown Blueprint editor customization
	FLiveBPBlueprintEditorCustomization::Shutdown();

	// Shutdown the editor subsystem
	if (ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>())
	{
		EditorSubsystem->Shutdown();
	}
}

void FLiveBPEditorModule::RegisterMenuExtensions()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(FLiveBPEditorModule::StaticClass());
		
		// Add menu item to Blueprint Editor menu
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
		{
			FToolMenuSection& Section = Menu->AddSection("LiveBlueprint", NSLOCTEXT("LiveBP", "LiveBlueprintSection", "Live Blueprint"));
			Section.AddMenuEntry("ToggleLiveBP",
				NSLOCTEXT("LiveBP", "ToggleLiveBP", "Toggle Live Blueprint Collaboration"),
				NSLOCTEXT("LiveBP", "ToggleLiveBPTooltip", "Enable or disable real-time Blueprint collaboration"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					if (ULiveBPEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<ULiveBPEditorSubsystem>())
					{
						EditorSubsystem->ToggleCollaboration();
					}
				}))
			);
		}
	}));
}

void FLiveBPEditorModule::UnregisterMenuExtensions()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(FLiveBPEditorModule::StaticClass());
}

IMPLEMENT_MODULE(FLiveBPEditorModule, LiveBPEditor)
