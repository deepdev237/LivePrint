#include "LiveBPCore.h"
#include "LiveBPMessageThrottler.h"
#include "LiveBPPerformanceMonitor.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogLiveBPCore);

void FLiveBPCoreModule::StartupModule()
{
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBPCore module starting up"));
	
	// Initialize global systems
	FLiveBPGlobalThrottler::Initialize();
	FLiveBPGlobalPerformanceMonitor::Initialize();
	
	// Start performance monitoring in development builds
#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
	FLiveBPGlobalPerformanceMonitor::Get().StartMonitoring();
#endif
}

void FLiveBPCoreModule::ShutdownModule()
{
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBPCore module shutting down"));
	
	// Shutdown global systems
	FLiveBPGlobalPerformanceMonitor::Shutdown();
	FLiveBPGlobalThrottler::Shutdown();
}

IMPLEMENT_MODULE(FLiveBPCoreModule, LiveBPCore)
