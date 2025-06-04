#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditorMode.h"
#include "BlueprintEditorModule.h"
#include "LiveBPGraphEditor.h"

class FBlueprintEditor;
class SLiveBPGraphEditor;

/**
 * Custom Blueprint editor mode that integrates Live Blueprint collaboration features
 */
class LIVEBPEDITOR_API FLiveBPBlueprintEditorMode : public FBlueprintEditorApplicationMode
{
public:
	/** Mode identifier */
	static const FName LiveBPModeID;
	
	FLiveBPBlueprintEditorMode(TSharedPtr<FBlueprintEditor> InBlueprintEditor);
	
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

protected:
	/** Blueprint editor we're extending */
	TWeakPtr<FBlueprintEditor> BlueprintEditor;
	
	/** Custom graph editor instances */
	TMap<TWeakObjectPtr<UEdGraph>, TSharedPtr<SLiveBPGraphEditor>> GraphEditors;
	
public:
	/** Get or create a Live BP graph editor for the given graph */
	TSharedPtr<SLiveBPGraphEditor> GetOrCreateGraphEditor(UEdGraph* Graph);
	
	/** Remove graph editor for a graph */
	void RemoveGraphEditor(UEdGraph* Graph);
	
	/** Get all active graph editors */
	TArray<TSharedPtr<SLiveBPGraphEditor>> GetAllGraphEditors() const;

private:
	/** Create a graph editor widget for the given graph */
	TSharedRef<SGraphEditor> CreateGraphEditorWidget(UEdGraph* Graph);
};

/**
 * Tab factory for Live Blueprint graph editor tabs
 */
class FLiveBPGraphEditorSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SGraphEditor>, FOnCreateGraphEditorWidget, UEdGraph*);

public:
	FLiveBPGraphEditorSummoner(TSharedPtr<FBlueprintEditor> InBlueprintEditor, FOnCreateGraphEditorWidget InOnCreateGraphEditorWidget);

	// FDocumentTabFactoryForObjects interface
	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabRefreshed(TSharedPtr<SDockTab> Tab) const override;
	virtual void SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const override;

protected:
	// FDocumentTabFactoryForObjects interface
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override;
	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual void OnTabClosed(TSharedRef<SDockTab> Tab, UEdGraph* DocumentID) const override;

private:
	/** Weak reference to the Blueprint editor */
	TWeakPtr<FBlueprintEditor> BlueprintEditor;
	
	/** Delegate to create graph editor widgets */
	FOnCreateGraphEditorWidget OnCreateGraphEditorWidget;
	
	/** Live BP editor mode */
	TSharedPtr<FLiveBPBlueprintEditorMode> LiveBPMode;
};

/**
 * Blueprint editor customization that replaces standard graph editors with Live BP versions
 */
class LIVEBPEDITOR_API FLiveBPBlueprintEditorCustomization
{
public:
	/** Initialize the customization system */
	static void Initialize();
	
	/** Shutdown the customization system */
	static void Shutdown();
	
	/** Check if a Blueprint editor supports Live BP collaboration */
	static bool IsLiveBPEnabled(TSharedPtr<FBlueprintEditor> BlueprintEditor);
	
	/** Enable Live BP collaboration for a Blueprint editor */
	static void EnableLiveBP(TSharedPtr<FBlueprintEditor> BlueprintEditor);
	
	/** Disable Live BP collaboration for a Blueprint editor */
	static void DisableLiveBP(TSharedPtr<FBlueprintEditor> BlueprintEditor);

private:
	/** Delegate handle for Blueprint editor creation */
	static FDelegateHandle BlueprintEditorOpenedHandle;
	
	/** Active Live BP editor modes */
	static TMap<TWeakPtr<FBlueprintEditor>, TSharedPtr<FLiveBPBlueprintEditorMode>> ActiveModes;
	
	/** Handle Blueprint editor opened */
	static void OnBlueprintEditorOpened(EBlueprintType BlueprintType, TSharedPtr<FBlueprintEditor> BlueprintEditor);
	
	/** Handle Blueprint editor closed */
	static void OnBlueprintEditorClosed(TSharedPtr<FBlueprintEditor> BlueprintEditor);
};
