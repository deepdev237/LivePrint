#include "LiveBPUtils.h"
#include "LiveBPCore.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

// Message throttle intervals (in seconds)
static const float WIRE_PREVIEW_THROTTLE = 0.1f;  // 10Hz
static const float NODE_OPERATION_THROTTLE = 0.0f; // No throttling for structural changes
static const float LOCK_MESSAGE_THROTTLE = 0.0f;   // No throttling for locks

TArray<uint8> FLiveBPUtils::SerializeToJson(const FLiveBPNodeOperationData& NodeOperation)
{
	TSharedPtr<FJsonObject> JsonObject = NodeOperationToJson(NodeOperation);
	
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	TArray<uint8> Result;
	Result.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*JsonString)), JsonString.Len());
	return Result;
}

TArray<uint8> FLiveBPUtils::SerializeToJson(const FLiveBPNodeLock& NodeLock)
{
	TSharedPtr<FJsonObject> JsonObject = NodeLockToJson(NodeLock);
	
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	TArray<uint8> Result;
	Result.Append(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*JsonString)), JsonString.Len());
	return Result;
}

bool FLiveBPUtils::DeserializeFromJson(const TArray<uint8>& Data, FLiveBPNodeOperationData& OutNodeOperation)
{
	FString JsonString;
	JsonString.AppendChars(reinterpret_cast<const TCHAR*>(Data.GetData()), Data.Num() / sizeof(TCHAR));
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		return JsonToNodeOperation(JsonObject, OutNodeOperation);
	}
	
	return false;
}

bool FLiveBPUtils::DeserializeFromJson(const TArray<uint8>& Data, FLiveBPNodeLock& OutNodeLock)
{
	FString JsonString;
	JsonString.AppendChars(reinterpret_cast<const TCHAR*>(Data.GetData()), Data.Num() / sizeof(TCHAR));
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		return JsonToNodeLock(JsonObject, OutNodeLock);
	}
	
	return false;
}

TArray<uint8> FLiveBPUtils::SerializeToBinary(const FLiveBPWirePreview& WirePreview)
{
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	
	Writer << const_cast<FGuid&>(WirePreview.NodeId);
	Writer << const_cast<FString&>(WirePreview.PinName);
	Writer << const_cast<FVector2D&>(WirePreview.StartPosition);
	Writer << const_cast<FVector2D&>(WirePreview.EndPosition);
	Writer << const_cast<FString&>(WirePreview.UserId);
	Writer << const_cast<float&>(WirePreview.Timestamp);
	
	return Result;
}

bool FLiveBPUtils::DeserializeFromBinary(const TArray<uint8>& Data, FLiveBPWirePreview& OutWirePreview)
{
	if (Data.Num() == 0)
	{
		return false;
	}
	
	FMemoryReader Reader(Data);
	
	try
	{
		Reader << OutWirePreview.NodeId;
		Reader << OutWirePreview.PinName;
		Reader << OutWirePreview.StartPosition;
		Reader << OutWirePreview.EndPosition;
		Reader << OutWirePreview.UserId;
		Reader << OutWirePreview.Timestamp;
		return true;
	}
	catch (...)
	{
		UE_LOG(LogLiveBPCore, Warning, TEXT("Failed to deserialize wire preview data"));
		return false;
	}
}

bool FLiveBPUtils::IsValidMessage(const FLiveBPMessage& Message)
{
	// Basic validation
	if (!Message.BlueprintId.IsValid() || !Message.GraphId.IsValid())
	{
		return false;
	}
	
	if (Message.UserId.IsEmpty() || Message.Timestamp <= 0.0f)
	{
		return false;
	}
	
	// Message type specific validation
	switch (Message.MessageType)
	{
	case ELiveBPMessageType::WirePreview:
		return Message.PayloadData.Num() > 0;
	case ELiveBPMessageType::NodeOperation:
		return Message.PayloadData.Num() > 0;
	case ELiveBPMessageType::LockRequest:
	case ELiveBPMessageType::LockRelease:
		return Message.PayloadData.Num() > 0;
	case ELiveBPMessageType::Heartbeat:
		return true; // Heartbeat doesn't need payload
	default:
		return false;
	}
}

bool FLiveBPUtils::IsValidNodeOperation(const FLiveBPNodeOperationData& NodeOperation)
{
	if (!NodeOperation.NodeId.IsValid() || NodeOperation.UserId.IsEmpty())
	{
		return false;
	}
	
	// Operation-specific validation
	switch (NodeOperation.Operation)
	{
	case ELiveBPNodeOperation::Add:
		return !NodeOperation.NodeClass.IsEmpty();
	case ELiveBPNodeOperation::Delete:
		return true; // Just need valid NodeId
	case ELiveBPNodeOperation::Move:
		return true; // Position can be anywhere
	case ELiveBPNodeOperation::PinConnect:
	case ELiveBPNodeOperation::PinDisconnect:
		return NodeOperation.TargetNodeId.IsValid() && !NodeOperation.PinName.IsEmpty();
	case ELiveBPNodeOperation::PropertyChange:
		return !NodeOperation.PropertyData.IsEmpty();
	default:
		return false;
	}
}

bool FLiveBPUtils::IsValidWirePreview(const FLiveBPWirePreview& WirePreview)
{
	return WirePreview.NodeId.IsValid() && 
		   !WirePreview.UserId.IsEmpty() && 
		   !WirePreview.PinName.IsEmpty() &&
		   WirePreview.Timestamp > 0.0f;
}

bool FLiveBPUtils::IsValidNodeLock(const FLiveBPNodeLock& NodeLock)
{
	return NodeLock.NodeId.IsValid() && 
		   !NodeLock.UserId.IsEmpty() &&
		   NodeLock.LockTime > 0.0f &&
		   NodeLock.ExpiryTime > NodeLock.LockTime;
}

bool FLiveBPUtils::ShouldThrottleMessage(ELiveBPMessageType MessageType, float LastSentTime, float CurrentTime)
{
	float ThrottleInterval = GetThrottleInterval(MessageType);
	if (ThrottleInterval <= 0.0f)
	{
		return false; // No throttling
	}
	
	return (CurrentTime - LastSentTime) < ThrottleInterval;
}

float FLiveBPUtils::GetThrottleInterval(ELiveBPMessageType MessageType)
{
	switch (MessageType)
	{
	case ELiveBPMessageType::WirePreview:
		return WIRE_PREVIEW_THROTTLE;
	case ELiveBPMessageType::NodeOperation:
		return NODE_OPERATION_THROTTLE;
	case ELiveBPMessageType::LockRequest:
	case ELiveBPMessageType::LockRelease:
		return LOCK_MESSAGE_THROTTLE;
	case ELiveBPMessageType::Heartbeat:
		return 1.0f; // 1 second heartbeat
	default:
		return 0.0f;
	}
}

FString FLiveBPUtils::NodeOperationToString(ELiveBPNodeOperation Operation)
{
	switch (Operation)
	{
	case ELiveBPNodeOperation::Add: return TEXT("Add");
	case ELiveBPNodeOperation::Delete: return TEXT("Delete");
	case ELiveBPNodeOperation::Move: return TEXT("Move");
	case ELiveBPNodeOperation::PinConnect: return TEXT("PinConnect");
	case ELiveBPNodeOperation::PinDisconnect: return TEXT("PinDisconnect");
	case ELiveBPNodeOperation::PropertyChange: return TEXT("PropertyChange");
	default: return TEXT("Unknown");
	}
}

FString FLiveBPUtils::LockStateToString(ELiveBPLockState LockState)
{
	switch (LockState)
	{
	case ELiveBPLockState::Unlocked: return TEXT("Unlocked");
	case ELiveBPLockState::Locked: return TEXT("Locked");
	case ELiveBPLockState::Pending: return TEXT("Pending");
	default: return TEXT("Unknown");
	}
}

FString FLiveBPUtils::MessageTypeToString(ELiveBPMessageType MessageType)
{
	switch (MessageType)
	{
	case ELiveBPMessageType::WirePreview: return TEXT("WirePreview");
	case ELiveBPMessageType::NodeOperation: return TEXT("NodeOperation");
	case ELiveBPMessageType::LockRequest: return TEXT("LockRequest");
	case ELiveBPMessageType::LockRelease: return TEXT("LockRelease");
	case ELiveBPMessageType::Heartbeat: return TEXT("Heartbeat");
	default: return TEXT("Unknown");
	}
}

uint32 FLiveBPUtils::GetNodeOperationHash(const FLiveBPNodeOperationData& NodeOperation)
{
	uint32 Hash = GetTypeHash(NodeOperation.NodeId);
	Hash = HashCombine(Hash, GetTypeHash(static_cast<int32>(NodeOperation.Operation)));
	Hash = HashCombine(Hash, GetTypeHash(NodeOperation.UserId));
	return Hash;
}

uint32 FLiveBPUtils::GetWirePreviewHash(const FLiveBPWirePreview& WirePreview)
{
	uint32 Hash = GetTypeHash(WirePreview.NodeId);
	Hash = HashCombine(Hash, GetTypeHash(WirePreview.PinName));
	Hash = HashCombine(Hash, GetTypeHash(WirePreview.UserId));
	return Hash;
}

float FLiveBPUtils::CalculateDistance2D(const FVector2D& A, const FVector2D& B)
{
	return FVector2D::Distance(A, B);
}

bool FLiveBPUtils::ArePositionsNearby(const FVector2D& A, const FVector2D& B, float Threshold)
{
	return CalculateDistance2D(A, B) <= Threshold;
}

bool FLiveBPUtils::IsTimestampRecent(float Timestamp, float MaxAge)
{
	return (GetCurrentTimestamp() - Timestamp) <= MaxAge;
}

// Private helper implementations
TSharedPtr<FJsonObject> FLiveBPUtils::NodeOperationToJson(const FLiveBPNodeOperationData& NodeOperation)
{
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
	
	return JsonObject;
}

TSharedPtr<FJsonObject> FLiveBPUtils::NodeLockToJson(const FLiveBPNodeLock& NodeLock)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	
	JsonObject->SetStringField(TEXT("NodeId"), NodeLock.NodeId.ToString());
	JsonObject->SetNumberField(TEXT("LockState"), static_cast<int32>(NodeLock.LockState));
	JsonObject->SetStringField(TEXT("UserId"), NodeLock.UserId);
	JsonObject->SetNumberField(TEXT("LockTime"), NodeLock.LockTime);
	JsonObject->SetNumberField(TEXT("ExpiryTime"), NodeLock.ExpiryTime);
	
	return JsonObject;
}

bool FLiveBPUtils::JsonToNodeOperation(const TSharedPtr<FJsonObject>& JsonObject, FLiveBPNodeOperationData& OutNodeOperation)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}
	
	OutNodeOperation.Operation = static_cast<ELiveBPNodeOperation>(JsonObject->GetIntegerField(TEXT("Operation")));
	FGuid::Parse(JsonObject->GetStringField(TEXT("NodeId")), OutNodeOperation.NodeId);
	FGuid::Parse(JsonObject->GetStringField(TEXT("TargetNodeId")), OutNodeOperation.TargetNodeId);
	OutNodeOperation.PinName = JsonObject->GetStringField(TEXT("PinName"));
	OutNodeOperation.TargetPinName = JsonObject->GetStringField(TEXT("TargetPinName"));
	OutNodeOperation.Position.X = JsonObject->GetNumberField(TEXT("PositionX"));
	OutNodeOperation.Position.Y = JsonObject->GetNumberField(TEXT("PositionY"));
	OutNodeOperation.NodeClass = JsonObject->GetStringField(TEXT("NodeClass"));
	OutNodeOperation.PropertyData = JsonObject->GetStringField(TEXT("PropertyData"));
	OutNodeOperation.UserId = JsonObject->GetStringField(TEXT("UserId"));
	OutNodeOperation.Timestamp = JsonObject->GetNumberField(TEXT("Timestamp"));
	
	return true;
}

bool FLiveBPUtils::JsonToNodeLock(const TSharedPtr<FJsonObject>& JsonObject, FLiveBPNodeLock& OutNodeLock)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}
	
	FGuid::Parse(JsonObject->GetStringField(TEXT("NodeId")), OutNodeLock.NodeId);
	OutNodeLock.LockState = static_cast<ELiveBPLockState>(JsonObject->GetIntegerField(TEXT("LockState")));
	OutNodeLock.UserId = JsonObject->GetStringField(TEXT("UserId"));
	OutNodeLock.LockTime = JsonObject->GetNumberField(TEXT("LockTime"));
	OutNodeLock.ExpiryTime = JsonObject->GetNumberField(TEXT("ExpiryTime"));
	
	return true;
}
