#include "LiveBPMUEIntegration.h"
#include "LiveBPCore.h"
#include "LiveBPMessageThrottler.h"
#include "LiveBPPerformanceMonitor.h"
#include "IConcertClient.h"
#include "IConcertSession.h"
#include "ConcertMessages.h"
#include "ConcertClientSettings.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Custom Concert event type for LiveBP messages
USTRUCT()
struct FLiveBPConcertEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FLiveBPMessage Message;
};

ULiveBPMUEIntegration::ULiveBPMUEIntegration()
	: bIsInitialized(false)
{
}

bool ULiveBPMUEIntegration::Initialize()
{
	if (bIsInitialized)
	{
		return true;
	}

	// Get the Concert client
	if (IConcertClientModule* ConcertClientModule = FModuleManager::GetModulePtr<IConcertClientModule>("ConcertClient"))
	{
		ConcertClient = ConcertClientModule->GetClient(TEXT("MultiUser"));
		if (!ConcertClient.IsValid())
		{
			UE_LOG(LogLiveBPCore, Warning, TEXT("Failed to get Concert client"));
			return false;
		}

		// Register for session events
		if (TSharedPtr<IConcertClientSession> Session = ConcertClient->GetCurrentSession())
		{
			// Register custom event handler for LiveBP messages
			Session->RegisterCustomEventHandler<FLiveBPConcertEvent>(this, 
				FOnConcertCustomEventReceived<FLiveBPConcertEvent>::CreateUObject(this, &ULiveBPMUEIntegration::HandleConcertMessage));

			// Get current user ID
			CurrentUserId = Session->GetSessionClientEndpointId().ToString();
			
			bIsInitialized = true;
			UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP MUE Integration initialized successfully"));
			return true;
		}
		else
		{
			UE_LOG(LogLiveBPCore, Warning, TEXT("No active Concert session found"));
		}
	}
	else
	{
		UE_LOG(LogLiveBPCore, Warning, TEXT("Concert client module not available"));
	}

	return false;
}

void ULiveBPMUEIntegration::Shutdown()
{
	if (bIsInitialized && ConcertClient.IsValid())
	{
		if (TSharedPtr<IConcertClientSession> Session = ConcertClient->GetCurrentSession())
		{
			Session->UnregisterCustomEventHandler<FLiveBPConcertEvent>(this);
		}
		ConcertClient.Reset();
		bIsInitialized = false;
	}
}

bool ULiveBPMUEIntegration::SendWirePreview(const FLiveBPWirePreview& WirePreview, const FGuid& BlueprintId, const FGuid& GraphId)
{
	LIVEBP_SCOPE_TIMER("SendWirePreview");
	
	if (!IsConnected())
	{
		LIVEBP_RECORD_ERROR("MUE not connected", true);
		return false;
	}

	// Check throttling
	float CurrentTime = FPlatformTime::Seconds();
	if (FLiveBPGlobalThrottler::Get().ShouldThrottleMessage(ELiveBPMessageType::WirePreview, CurrentUserId, CurrentTime))
	{
		return false; // Throttled, but not an error
	}

	FLiveBPMessage Message;
	Message.MessageType = ELiveBPMessageType::WirePreview;
	Message.BlueprintId = BlueprintId;
	Message.GraphId = GraphId;
	Message.UserId = CurrentUserId;
	Message.Timestamp = CurrentTime;
	Message.PayloadData = SerializeWirePreview(WirePreview);

	FLiveBPConcertEvent ConcertEvent;
	ConcertEvent.Message = Message;

	if (TSharedPtr<IConcertClientSession> Session = ConcertClient->GetCurrentSession())
	{
		Session->SendCustomEvent(ConcertEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		
		// Record message sent
		FLiveBPGlobalThrottler::Get().RecordMessageSent(ELiveBPMessageType::WirePreview, CurrentUserId, CurrentTime);
		LIVEBP_RECORD_MESSAGE_SENT(ELiveBPMessageType::WirePreview, Message.PayloadData.Num());
		
		return true;
	}

	LIVEBP_RECORD_ERROR("Failed to get Concert session", true);
	return false;
}

bool ULiveBPMUEIntegration::SendNodeOperation(const FLiveBPNodeOperationData& NodeOperation, const FGuid& BlueprintId, const FGuid& GraphId)
{
	LIVEBP_SCOPE_TIMER("SendNodeOperation");
	
	if (!IsConnected())
	{
		LIVEBP_RECORD_ERROR("MUE not connected", true);
		return false;
	}

	float CurrentTime = FPlatformTime::Seconds();
	
	FLiveBPMessage Message;
	Message.MessageType = ELiveBPMessageType::NodeOperation;
	Message.BlueprintId = BlueprintId;
	Message.GraphId = GraphId;
	Message.UserId = CurrentUserId;
	Message.Timestamp = CurrentTime;
	Message.PayloadData = SerializeNodeOperation(NodeOperation);

	FLiveBPConcertEvent ConcertEvent;
	ConcertEvent.Message = Message;

	if (TSharedPtr<IConcertClientSession> Session = ConcertClient->GetCurrentSession())
	{
		Session->SendCustomEvent(ConcertEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		
		// Record message sent (no throttling for structural changes)
		LIVEBP_RECORD_MESSAGE_SENT(ELiveBPMessageType::NodeOperation, Message.PayloadData.Num());
		
		return true;
	}

	LIVEBP_RECORD_ERROR("Failed to get Concert session", true);
	return false;
}

bool ULiveBPMUEIntegration::SendLockRequest(const FLiveBPNodeLock& LockRequest, const FGuid& BlueprintId, const FGuid& GraphId)
{
	LIVEBP_SCOPE_TIMER("SendLockRequest");
	
	if (!IsConnected())
	{
		LIVEBP_RECORD_ERROR("MUE not connected", true);
		return false;
	}

	float CurrentTime = FPlatformTime::Seconds();
	
	FLiveBPMessage Message;
	Message.MessageType = ELiveBPMessageType::LockRequest;
	Message.BlueprintId = BlueprintId;
	Message.GraphId = GraphId;
	Message.UserId = CurrentUserId;
	Message.Timestamp = CurrentTime;
	Message.PayloadData = SerializeLockRequest(LockRequest);

	FLiveBPConcertEvent ConcertEvent;
	ConcertEvent.Message = Message;

	if (TSharedPtr<IConcertClientSession> Session = ConcertClient->GetCurrentSession())
	{
		Session->SendCustomEvent(ConcertEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		
		// Record message sent (no throttling for locks)
		LIVEBP_RECORD_MESSAGE_SENT(ELiveBPMessageType::LockRequest, Message.PayloadData.Num());
		
		return true;
	}

	LIVEBP_RECORD_ERROR("Failed to get Concert session", true);
	return false;
}
}

void ULiveBPMUEIntegration::HandleConcertMessage(const FConcertSessionContext& Context, const FLiveBPConcertEvent& Event)
{
	LIVEBP_SCOPE_TIMER("HandleConcertMessage");
	
	// Don't process our own messages
	if (Event.Message.UserId == CurrentUserId)
	{
		return;
	}

	// Calculate latency if possible
	float CurrentTime = FPlatformTime::Seconds();
	float LatencyMs = (CurrentTime - Event.Message.Timestamp) * 1000.0f;
	
	// Record message received
	LIVEBP_RECORD_MESSAGE_RECEIVED(Event.Message.MessageType, Event.Message.PayloadData.Num(), LatencyMs);

	HandleLiveBPMessage(Event.Message, Context);
}

void ULiveBPMUEIntegration::HandleLiveBPMessage(const FLiveBPMessage& Message, const FConcertSessionContext& Context)
{
	UE_LOG(LogLiveBPCore, VeryVerbose, TEXT("Received LiveBP message of type %d from user %s"), 
		static_cast<int32>(Message.MessageType), *Message.UserId);

	OnMessageReceived.Broadcast(Message, Context);
}

bool ULiveBPMUEIntegration::IsConnected() const
{
	return bIsInitialized && ConcertClient.IsValid() && ConcertClient->GetCurrentSession().IsValid();
}

FString ULiveBPMUEIntegration::GetCurrentUserId() const
{
	return CurrentUserId;
}

TArray<uint8> ULiveBPMUEIntegration::SerializeWirePreview(const FLiveBPWirePreview& WirePreview) const
{
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	
	// Binary serialization for performance
	Writer << const_cast<FGuid&>(WirePreview.NodeId);
	Writer << const_cast<FString&>(WirePreview.PinName);
	Writer << const_cast<FVector2D&>(WirePreview.StartPosition);
	Writer << const_cast<FVector2D&>(WirePreview.EndPosition);
	Writer << const_cast<FString&>(WirePreview.UserId);
	Writer << const_cast<float&>(WirePreview.Timestamp);
	
	return Result;
}

TArray<uint8> ULiveBPMUEIntegration::SerializeNodeOperation(const FLiveBPNodeOperationData& NodeOperation) const
{
	// Use JSON for structural changes to maintain readability and debuggability
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	
	JsonObject->SetNumberField(TEXT("Operation"), static_cast<int32>(NodeOperation.Operation));
	JsonObject->SetStringField(TEXT("NodeId"), NodeOperation.NodeId.ToString());
	JsonObject->SetStringField(TEXT("TargetNodeId"), NodeOperation.TargetNodeId.ToString());
	JsonObject->SetStringField(TEXT("PinName"), NodeOperation.PinName);
	JsonObject->SetStringField(TEXT("TargetPinName"), NodeOperation.TargetPinName);
	JsonObject->SetNumberField(TEXT("PositionX"), NodeOperation.Position.X);
	JsonObject->SetNumberField(TEXT("PositionY"), NodeOperation.Position.Y);
	JsonObject->SetStringField(TEXT("NodeClass"), NodeOperation.NodeClass);
	JsonObject->SetStringField(TEXT("PropertyData"), NodeOperation.PropertyData);
	JsonObject->SetStringField(TEXT("UserId"), NodeOperation.UserId);
	JsonObject->SetNumberField(TEXT("Timestamp"), NodeOperation.Timestamp);
	
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	TArray<uint8> Result;
	Result.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*JsonString)), JsonString.Len());
	
	return Result;
}

TArray<uint8> ULiveBPMUEIntegration::SerializeLockRequest(const FLiveBPNodeLock& LockRequest) const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	
	JsonObject->SetStringField(TEXT("NodeId"), LockRequest.NodeId.ToString());
	JsonObject->SetNumberField(TEXT("LockState"), static_cast<int32>(LockRequest.LockState));
	JsonObject->SetStringField(TEXT("UserId"), LockRequest.UserId);
	JsonObject->SetNumberField(TEXT("LockTime"), LockRequest.LockTime);
	JsonObject->SetNumberField(TEXT("ExpiryTime"), LockRequest.ExpiryTime);
	
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	TArray<uint8> Result;
	Result.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*JsonString)), JsonString.Len());
	
	return Result;
}
