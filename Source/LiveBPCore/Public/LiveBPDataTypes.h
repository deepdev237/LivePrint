#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "LiveBPDataTypes.generated.h"

UENUM(BlueprintType)
enum class ELiveBPMessageType : uint8
{
	WirePreview,
	NodeOperation,
	LockRequest,
	LockRelease,
	Heartbeat
};

UENUM(BlueprintType)
enum class ELiveBPNodeOperation : uint8
{
	Add,
	Delete,
	Move,
	PinConnect,
	PinDisconnect,
	PropertyChange
};

UENUM(BlueprintType)
enum class ELiveBPLockState : uint8
{
	Unlocked,
	Locked,
	Pending
};

USTRUCT(BlueprintType)
struct LIVEBPCORE_API FLiveBPWirePreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString PinName;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FVector2D StartPosition;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FVector2D EndPosition;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString UserId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float Timestamp;

	FLiveBPWirePreview()
		: StartPosition(FVector2D::ZeroVector)
		, EndPosition(FVector2D::ZeroVector)
		, Timestamp(0.0f)
	{
	}
};

USTRUCT(BlueprintType)
struct LIVEBPCORE_API FLiveBPNodeOperationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	ELiveBPNodeOperation Operation;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid TargetNodeId; // For connections

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString PinName;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString TargetPinName;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FVector2D Position;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString NodeClass;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString PropertyData; // JSON serialized properties

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString UserId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float Timestamp;

	FLiveBPNodeOperationData()
		: Operation(ELiveBPNodeOperation::Add)
		, Position(FVector2D::ZeroVector)
		, Timestamp(0.0f)
	{
	}
};

USTRUCT(BlueprintType)
struct LIVEBPCORE_API FLiveBPNodeLock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	ELiveBPLockState LockState;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString UserId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float LockTime;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float ExpiryTime;

	FLiveBPNodeLock()
		: LockState(ELiveBPLockState::Unlocked)
		, LockTime(0.0f)
		, ExpiryTime(0.0f)
	{
	}
};

USTRUCT(BlueprintType)
struct LIVEBPCORE_API FLiveBPMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	ELiveBPMessageType MessageType;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid BlueprintId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FGuid GraphId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	FString UserId;

	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	float Timestamp;

	// Serialized payload data
	UPROPERTY(BlueprintReadWrite, Category = "LiveBP")
	TArray<uint8> PayloadData;

	FLiveBPMessage()
		: MessageType(ELiveBPMessageType::Heartbeat)
		, Timestamp(0.0f)
	{
	}
};
