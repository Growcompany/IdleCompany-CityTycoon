// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Office/WorkstationTypes.h"
#include "WorkstationTable.generated.h"

/**
 * 업무공간 아이템 스킨 데이터 (통합)
 * 의자, 노트북, 모니터색상, 키보드마우스 등 스킨 정보
 */
USTRUCT(BlueprintType)
struct FWorkstationItemSkinData : public FTableRowBase
{
	GENERATED_BODY()

	// 스킨 ID (DataTable Row Name)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FName SkinID;

	// 대상 슬롯 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	EWorkstationSlot TargetSlot = EWorkstationSlot::Chair;

	// 표시 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FText DisplayName;

	// 적용할 메시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	TSoftObjectPtr<UStaticMesh> Mesh;

	// 스킨 비용 (0이면 기본 제공)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	int32 Cost = 0;

	// 해금 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	int32 UnlockLevel = 1;

	// 프리미엄 스킨 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	bool bIsPremium = false;

	// 모니터 전용: 모니터 타입 (Flat, Curved, Vertical)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Monitor")
	EMonitorType MonitorType = EMonitorType::None;

	FWorkstationItemSkinData()
		: TargetSlot(EWorkstationSlot::Chair)
		, Cost(0)
		, UnlockLevel(1)
		, bIsPremium(false)
		, MonitorType(EMonitorType::None)
	{}
};

/** 책상 레벨의 표시, 밸런스, 활성 장비와 메시를 모두 소유하는 단일 데이터 행 */
USTRUCT(BlueprintType)
struct FComputerSetupLevelData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	EComputerSetupLevel Level = EComputerSetupLevel::Level1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FText DisplayName;

	/** 이 행으로 진입할 때 지불하는 Diamond 비용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 UpgradeCost = 0;

	/** 최대 레벨 외형 변경 1회 비용. 승급 비용과 별개라 UpgradeCost 를 재활용하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 VariantSwapCost = 0;

	/** 누적 작업 속도 증가율. 0.05는 5% 빠른 점수 Orb 주기를 뜻한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float WorkSpeedBonusRate = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	ELaptopPlacement LaptopPlacement = ELaptopPlacement::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	EPrimaryMonitorType PrimaryMonitorType = EPrimaryMonitorType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	ESecondMonitorType SecondMonitorType = ESecondMonitorType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	bool bKeyboardActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	bool bMouseActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	bool bComputerActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> LaptopMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> PrimaryMonitorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> VerticalMonitorMesh;

	/** SecondMonitorType == Curved 일 때 좌·우 두 대에 공통으로 쓰는 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> DualMonitorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> KeyboardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> MouseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> ComputerMesh;

	bool IsValidConfiguration(FString* OutError = nullptr) const
	{
		auto Reject = [OutError](const TCHAR* Message)
		{
			if (OutError)
			{
				*OutError = Message;
			}
			return false;
		};

		if (static_cast<uint8>(Level) > static_cast<uint8>(EComputerSetupLevel::Level6Twin))
		{
			return Reject(TEXT("Invalid setup level"));
		}
		if (DisplayName.IsEmpty())
		{
			return Reject(TEXT("DisplayName is required"));
		}
		if (UpgradeCost < 0 || !FMath::IsFinite(WorkSpeedBonusRate) || WorkSpeedBonusRate < 0.0f)
		{
			return Reject(TEXT("Cost and work-speed rate must be non-negative"));
		}
		// 미파싱 enum 컬럼은 None 으로 떨어진다 — 전 장비 비활성 행을 통과시키면 결제 후 빈 책상이 된다
		if (LaptopPlacement == ELaptopPlacement::None
			&& PrimaryMonitorType == EPrimaryMonitorType::None
			&& SecondMonitorType == ESecondMonitorType::None
			&& !bKeyboardActive && !bMouseActive && !bComputerActive)
		{
			return Reject(TEXT("A setup level must activate at least one equipment item"));
		}
		if (LaptopPlacement != ELaptopPlacement::None && LaptopMesh.IsNull())
		{
			return Reject(TEXT("Active laptop requires a mesh"));
		}
		if (PrimaryMonitorType != EPrimaryMonitorType::None && PrimaryMonitorMesh.IsNull())
		{
			return Reject(TEXT("Active primary monitor requires a mesh"));
		}
		if (SecondMonitorType == ESecondMonitorType::Vertical && VerticalMonitorMesh.IsNull())
		{
			return Reject(TEXT("Active vertical monitor requires a mesh"));
		}
		if (SecondMonitorType == ESecondMonitorType::Curved && DualMonitorMesh.IsNull())
		{
			return Reject(TEXT("Curved twin monitors require a mesh"));
		}
		if (bKeyboardActive && KeyboardMesh.IsNull())
		{
			return Reject(TEXT("Active keyboard requires a mesh"));
		}
		if (bMouseActive && MouseMesh.IsNull())
		{
			return Reject(TEXT("Active mouse requires a mesh"));
		}
		if (bComputerActive && ComputerMesh.IsNull())
		{
			return Reject(TEXT("Active computer requires a mesh"));
		}

		return true;
	}
};
