#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Notifications/NotificationManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLiveBPEditor, Log, All);

class FLiveBPEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
};
