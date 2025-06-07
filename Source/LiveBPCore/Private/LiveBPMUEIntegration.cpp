#include "LiveBPMUEIntegration.h"
#include "LiveBPCore.h"
#include "IConcertSyncClientModule.h"
#include "IConcertSyncClient.h"
#include "IConcertClientSession.h"
#include "ConcertMessages.h"
#include "ConcertSessionMessages.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Misc/DateTime.h"
// Additional headers required for UE 5.5
#include "Modules/ModuleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Templates/SharedPointer.h"

// Define the static channel constants
const FString ULiveBPMUEIntegration::LiveBPWirePreviewChannel = TEXT("LiveBP.WirePreview");
const FString ULiveBPMUEIntegration::LiveBPNodeOperationChannel = TEXT("LiveBP.NodeOperation");
const FString ULiveBPMUEIntegration::LiveBPLockRequestChannel = TEXT("LiveBP.LockRequest");

// Custom Concert event structure for LiveBP messages
USTRUCT()
struct FLiveBPConcertEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FString Channel;

	UPROPERTY()
	FLiveBPMessage Message;

	FLiveBPConcertEvent()
	{
	}

	FLiveBPConcertEvent(const FString& InChannel, const FLiveBPMessage& InMessage)
		: Channel(InChannel), Message(InMessage)
	{
	}
};

template<>
struct TStructOpsTypeTraits<FLiveBPConcertEvent> : public TStructOpsTypeTraitsBase2<FLiveBPConcertEvent>
{
	enum { WithSerializer = true };
};

ULiveBPMUEIntegration::ULiveBPMUEIntegration()
	: ConcertSyncClient(nullptr)
	, bIsInitialized(false)
	, CurrentUserId(TEXT(""))
{
}

void ULiveBPMUEIntegration::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	InitializeConcertIntegration();
}

void ULiveBPMUEIntegration::Deinitialize()
{
	ShutdownConcertIntegration();
	
	Super::Deinitialize();
}

bool ULiveBPMUEIntegration::InitializeConcertIntegration()
{
	UE_LOG(LogLiveBPCore, Log, TEXT("Initializing LiveBP Concert integration..."));

	// Get the Concert Sync Client Module - using correct API from UE 5.5
	IConcertSyncClientModule& ConcertSyncClientModule = FModuleManager::Get().LoadModuleChecked<IConcertSyncClientModule>("ConcertSyncClient");
	ConcertSyncClient = ConcertSyncClientModule.GetClient().Get();

	if (!ConcertSyncClient)
	{
		UE_LOG(LogLiveBPCore, Error, TEXT("Failed to get Concert Sync Client"));
		return false;
	}

	// Set up session event handlers
	ConcertSyncClient->OnSessionStartup().AddUObject(this, &ULiveBPMUEIntegration::OnSessionStartup);
	ConcertSyncClient->OnSessionShutdown().AddUObject(this, &ULiveBPMUEIntegration::OnSessionShutdown);

	// Check if we already have an active session
	if (TSharedPtr<IConcertClientSession> ExistingSession = ConcertSyncClient->GetCurrentSession())
	{
		OnSessionStartup(ExistingSession.ToSharedRef());
	}

	bIsInitialized = true;
	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Concert integration initialized successfully"));
	
	return true;
}

void ULiveBPMUEIntegration::ShutdownConcertIntegration()
{
	if (bIsInitialized && ConcertSyncClient)
	{
		// Unregister from session events
		ConcertSyncClient->OnSessionStartup().RemoveAll(this);
		ConcertSyncClient->OnSessionShutdown().RemoveAll(this);

		// Unregister custom event handlers if we have an active session
		if (ActiveSession.IsValid())
		{
			ActiveSession->UnregisterCustomEventHandler<FLiveBPConcertEvent>();
		}

		ActiveSession.Reset();
		ConcertSyncClient = nullptr;
		bIsInitialized = false;
		CurrentUserId.Empty();

		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP Concert integration shutdown"));
	}
}

void ULiveBPMUEIntegration::OnSessionStartup(TSharedRef<IConcertClientSession> InSession)
{
	ActiveSession = InSession;
	
	// Get current user ID from the session - using correct API
	const FConcertClientInfo& ClientInfo = InSession->GetLocalClientInfo();
	CurrentUserId = ClientInfo.UserName;
	
	if (CurrentUserId.IsEmpty())
	{
		CurrentUserId = FString::Printf(TEXT("User_%s"), *FGuid::NewGuid().ToString());
	}

	// Register custom event handler for LiveBP messages
	InSession->RegisterCustomEventHandler<FLiveBPConcertEvent>(this, &ULiveBPMUEIntegration::OnCustomEventReceived);

	UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP joined Concert session as user: %s"), *CurrentUserId);
}

void ULiveBPMUEIntegration::OnSessionShutdown(TSharedRef<IConcertClientSession> InSession)
{
	if (ActiveSession.IsValid() && ActiveSession.Get() == &InSession.Get())
	{
		// Unregister custom event handler
		InSession->UnregisterCustomEventHandler<FLiveBPConcertEvent>();
		
		ActiveSession.Reset();
		CurrentUserId.Empty();

		UE_LOG(LogLiveBPCore, Log, TEXT("LiveBP left Concert session"));
	}
}

void ULiveBPMUEIntegration::OnCustomEventReceived(const FConcertSessionContext& Context, const FLiveBPConcertEvent& Event)
{
	// Don't process our own messages
	if (Event.Message.UserId == CurrentUserId)
	{
		return;
	}

	UE_LOG(LogLiveBPCore, VeryVerbose, TEXT("Received LiveBP message of type %d from user %s on channel %s"), 
		static_cast<int32>(Event.Message.MessageType), *Event.Message.UserId, *Event.Channel);

	// Broadcast the received message to listeners
	OnMessageReceived.Broadcast(Event.Message);
}

bool ULiveBPMUEIntegration::SendCustomEvent(const FString& Channel, const TArray<uint8>& EventData)
{
	if (!IsConnected())
	{
		UE_LOG(LogLiveBPCore, Warning, TEXT("Cannot send custom event: not connected to Concert session"));
		return false;
	}

	// Create LiveBP message from the event data
	FLiveBPMessage Message;
	if (!DeserializeMessage(Channel, EventData, Message))
	{
		UE_LOG(LogLiveBPCore, Error, TEXT("Failed to deserialize message for channel: %s"), *Channel);
		return false;
	}

	// Create Concert event wrapper
	FLiveBPConcertEvent ConcertEvent(Channel, Message);

	// Send the event to all session participants
	ActiveSession->SendCustomEvent(ConcertEvent, ActiveSession->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);

	UE_LOG(LogLiveBPCore, Verbose, TEXT("Sent LiveBP message of type %d on channel %s"), 
		static_cast<int32>(Message.MessageType), *Channel);

	return true;
}

bool ULiveBPMUEIntegration::SendWirePreview(const FLiveBPWirePreview& WirePreview, const FGuid& BlueprintId, const FGuid& GraphId)
{
	if (!IsConnected())
	{
		UE_LOG(LogLiveBPCore, Warning, TEXT("Cannot send wire preview: not connected to Concert session"));
		return false;
	}

	// Create message
	FLiveBPMessage Message;
	Message.MessageType = ELiveBPMessageType::WirePreview;
	Message.BlueprintId = BlueprintId;
	Message.GraphId = GraphId;
	Message.UserId = CurrentUserId;
	Message.Timestamp = FPlatformTime::Seconds();
	Message.PayloadData = SerializeWirePreview(WirePreview);

	// Create Concert event
	FLiveBPConcertEvent ConcertEvent(LiveBPWirePreviewChannel, Message);

	// Send to all participants using the correct API
	TArray<FGuid> AllEndpoints;
	TArray<FConcertClientInfo> ClientInfos = ActiveSession->GetSessionClients();
	for (const FConcertClientInfo& ClientInfo : ClientInfos)
	{
		AllEndpoints.Add(ClientInfo.ClientEndpointId);
	}
	
	if (AllEndpoints.Num() > 0)
	{
		ActiveSession->SendCustomEvent(ConcertEvent, AllEndpoints, EConcertMessageFlags::ReliableOrdered);
	}

	UE_LOG(LogLiveBPCore, VeryVerbose, TEXT("Sent wire preview for Blueprint %s"), *BlueprintId.ToString());

	return true;
}

bool ULiveBPMUEIntegration::SendNodeOperation(const FLiveBPNodeOperationData& NodeOperation, const FGuid& BlueprintId, const FGuid& GraphId)
{
	if (!IsConnected())
	{
		UE_LOG(LogLiveBPCore, Warning, TEXT("Cannot send node operation: not connected to Concert session"));
		return false;
	}

	// Create message
	FLiveBPMessage Message;
	Message.MessageType = ELiveBPMessageType::NodeOperation;
	Message.BlueprintId = BlueprintId;
	Message.GraphId = GraphId;
	Message.UserId = CurrentUserId;
	Message.Timestamp = FPlatformTime::Seconds();
	Message.PayloadData = SerializeNodeOperation(NodeOperation);

	// Create Concert event
	FLiveBPConcertEvent ConcertEvent(LiveBPNodeOperationChannel, Message);

	// Send to all participants using the correct API
	TArray<FGuid> AllEndpoints;
	TArray<FConcertClientInfo> ClientInfos = ActiveSession->GetSessionClients();
	for (const FConcertClientInfo& ClientInfo : ClientInfos)
	{
		AllEndpoints.Add(ClientInfo.ClientEndpointId);
	}
	
	if (AllEndpoints.Num() > 0)
	{
		ActiveSession->SendCustomEvent(ConcertEvent, AllEndpoints, EConcertMessageFlags::ReliableOrdered);
	}

	UE_LOG(LogLiveBPCore, Verbose, TEXT("Sent node operation %d for Blueprint %s"), 
		static_cast<int32>(NodeOperation.Operation), *BlueprintId.ToString());

	return true;
}

bool ULiveBPMUEIntegration::SendLockRequest(const FLiveBPNodeLock& LockRequest, const FGuid& BlueprintId, const FGuid& GraphId)
{
	if (!IsConnected())
	{
		UE_LOG(LogLiveBPCore, Warning, TEXT("Cannot send lock request: not connected to Concert session"));
		return false;
	}

	// Create message
	FLiveBPMessage Message;
	Message.MessageType = ELiveBPMessageType::LockRequest;
	Message.BlueprintId = BlueprintId;
	Message.GraphId = GraphId;
	Message.UserId = CurrentUserId;
	Message.Timestamp = FPlatformTime::Seconds();
	Message.PayloadData = SerializeLockRequest(LockRequest);

	// Create Concert event
	FLiveBPConcertEvent ConcertEvent(LiveBPLockRequestChannel, Message);

	// Send to all participants using the correct API
	TArray<FGuid> AllEndpoints;
	TArray<FConcertClientInfo> ClientInfos = ActiveSession->GetSessionClients();
	for (const FConcertClientInfo& ClientInfo : ClientInfos)
	{
		AllEndpoints.Add(ClientInfo.ClientEndpointId);
	}
	
	if (AllEndpoints.Num() > 0)
	{
		ActiveSession->SendCustomEvent(ConcertEvent, AllEndpoints, EConcertMessageFlags::ReliableOrdered);
	}

	UE_LOG(LogLiveBPCore, Verbose, TEXT("Sent lock request for node %s in Blueprint %s"), 
		*LockRequest.NodeId.ToString(), *BlueprintId.ToString());

	return true;
}

bool ULiveBPMUEIntegration::IsConnected() const
{
	return bIsInitialized && ConcertSyncClient && ActiveSession.IsValid();
}

bool ULiveBPMUEIntegration::HasActiveSession() const
{
	return ActiveSession.IsValid();
}

FString ULiveBPMUEIntegration::GetCurrentUserId() const
{
	return CurrentUserId;
}

TArray<FString> ULiveBPMUEIntegration::GetConnectedUsers() const
{
	TArray<FString> ConnectedUsers;
	
	if (!ActiveSession.IsValid())
	{
		return ConnectedUsers;
	}

	TArray<FConcertClientInfo> ClientInfos = ActiveSession->GetSessionClients();
	for (const FConcertClientInfo& ClientInfo : ClientInfos)
	{
		ConnectedUsers.Add(ClientInfo.UserName);
	}

	return ConnectedUsers;
}

TArray<uint8> ULiveBPMUEIntegration::SerializeWirePreview(const FLiveBPWirePreview& WirePreview) const
{
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	
	// Binary serialization for performance - wire previews are high frequency
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

bool ULiveBPMUEIntegration::DeserializeMessage(const FString& Channel, const TArray<uint8>& Data, FLiveBPMessage& OutMessage) const
{
	// For this implementation, we're creating the message structure directly in the Send methods
	// This function would be used if we were deserializing raw event data
	// For now, we'll return true as we're handling this differently
	return true;
}
