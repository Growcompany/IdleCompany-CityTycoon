#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ItemType.h"
#include "GachaBoxVisualTable.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EGachaBoxType : uint8
{
	EquipmentNormal,
	EquipmentAdvanced,
	EquipmentPremium,
	TraitNormal,
	TraitAdvanced,
	SkinNormal,
	SkinAdvanced,
};

/** 뽑기 상자 한 종류의 구운 레이어(DT_GachaBoxVisual). 시안을 같은 캡처 영역으로 구웠으므로 모든 레이어를 LayerSize 한 칸에 겹치면 시안 그림이 된다. */
USTRUCT(BlueprintType)
struct FGachaBoxVisualRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	EGachaBoxType BoxType = EGachaBoxType::EquipmentNormal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	TSoftObjectPtr<UTexture2D> Body;

	// 검은 불투명 바탕이라 더하기 합성으로만 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	TSoftObjectPtr<UTexture2D> Glow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	TSoftObjectPtr<UTexture2D> Icon;

	// 날개 회전을 t = i/10 로 표본한 11장, 0 = 닫힘
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	TArray<TSoftObjectPtr<UTexture2D>> FlapFront;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	TArray<TSoftObjectPtr<UTexture2D>> FlapBack;

	// 이음매·기준점은 레이어 이미지 기준 0~1 비율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	FVector2D SeamL = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	FVector2D SeamR = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	FVector2D SeamTop = FVector2D::ZeroVector;

	// 던지기·눌림·흔들림 변형의 축
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	FVector2D BoxOrigin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	FVector2D LayerSize = FVector2D::ZeroVector;

	// 층 합성은 위 가장자리 1px 테가 빠지므로 닫힌 동안은 이 한 장으로 그린다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	TSoftObjectPtr<UTexture2D> Closed;

	// linear (시안 sRGB 를 변환해 넣는다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GachaBox")
	FLinearColor TapeColor = FLinearColor::White;
};

inline EGachaBoxType GachaBoxTypeFromTicket(EItemType Ticket)
{
	switch (Ticket)
	{
	case EItemType::EmployeeEquipmentTicketNormal:   return EGachaBoxType::EquipmentNormal;
	case EItemType::EmployeeEquipmentTicketAdvanced: return EGachaBoxType::EquipmentAdvanced;
	case EItemType::EmployeeEquipmentTicketPremium:  return EGachaBoxType::EquipmentPremium;
	case EItemType::BuildingTraitTicketNormal:       return EGachaBoxType::TraitNormal;
	case EItemType::BuildingTraitTicketAdvanced:     return EGachaBoxType::TraitAdvanced;
	// 신화 특성 뽑기권은 전용 상자를 굽지 않아 고급 특성 상자로 연다
	case EItemType::BuildingTraitTicketMythic:       return EGachaBoxType::TraitAdvanced;
	case EItemType::SkinTicketNormal:                return EGachaBoxType::SkinNormal;
	case EItemType::SkinTicketAdvanced:              return EGachaBoxType::SkinAdvanced;
	default:
		UE_LOG(LogTemp, Warning, TEXT("[GachaBoxTicket] %s 에 맞는 상자 종류가 없어 일반 장비 상자로 그립니다"), *UEnum::GetValueAsString(Ticket));
		return EGachaBoxType::EquipmentNormal;
	}
}
