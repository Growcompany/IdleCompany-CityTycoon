// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Office/WorkstationTypes.h"
#include "Office/DecorationTypes.h"
#include "Data/RecruitmentData.h"
#include "Data/StageProgressData.h"
#include "Data/OperationData.h"
#include "Data/EmployeeTypes.h"
#include "Data/ProjectReportData.h"
#include "Data/ProjectBoardData.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/BacklogProductData.h"
#include "BuildingSaveData.generated.h"

/**
 * 데코레이션 저장 데이터 (간략화된 버전)
 */
USTRUCT(BlueprintType)
struct FDecorationSaveData
{
	GENERATED_BODY()

	// DecorationCardTable의 RowName
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Decoration")
	FName RowName;

	// 배치 위치 및 회전
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Decoration")
	FTransform Transform;

	// 카테고리 (빠른 필터링용)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Decoration")
	EDecorationCategory Category = EDecorationCategory::None;

	FDecorationSaveData()
		: RowName(NAME_None)
		, Transform(FTransform::Identity)
		, Category(EDecorationCategory::None)
	{
	}

	FDecorationSaveData(FName InRowName, const FTransform& InTransform, EDecorationCategory InCategory)
		: RowName(InRowName)
		, Transform(InTransform)
		, Category(InCategory)
	{
	}
};

/**
 * 오피스 전용 저장 데이터 (BuildingID별 별도 관리)
 */
USTRUCT(BlueprintType)
struct FOfficeSaveData
{
	GENERATED_BODY()

	// 오피스 X축 방향 타일 개수 (초기값 1)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Expansion")
	int32 TileCountX = 1;

	// 오피스 Y축 방향 타일 개수 (초기값 2)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Expansion")
	int32 TileCountY = 2;

	// 현재 적용된 바닥 타일의 DecorationData RowName
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|FloorTile")
	FName CurrentFloorTileRowName;

	// 보유한 바닥 타일 RowName 목록
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|FloorTile")
	TArray<FName> OwnedFloorTileRowNames;

	// 배치된 장식품 목록
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Decoration")
	TArray<FDecorationSaveData> PlacedDecorations;

	// 오피스 내 배치된 업무공간 목록
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Workstation")
	TArray<FWorkstationSaveData> OfficeWorkstations;

	// ========== 수익 저장 (오프라인 누적) ==========

	// 저장된 수익 (오프라인/다른 맵에서 누적된 금액)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Revenue")
	float StoredRevenue = 0.0f;

	// ========== 스테이지 진행 시스템 ==========

	// 현재 프로젝트 진행 상태
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|StageProgress")
	FStageProgressData StageProgress;

	// ========== 운영 시스템 ==========

	// 현재 운영 중인 프로젝트 (Building당 하나)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Operation")
	FOperationData CurrentOperation;

	// 운영 중인지 여부
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Operation")
	bool bHasActiveOperation = false;

	// ========== 직원 시스템 (건물별 관리) ==========

	// 이 건물에 배정된 직원 목록
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Employee")
	TArray<FEmployeeInstance> EmployeeList;

	// 직원 외모 데이터 (EmployeeID -> Appearance)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Employee")
	TMap<int32, FCharacterAppearance> EmployeeAppearances;

	// ========== 프로젝트 결산서 (Pending) ==========

	// 미확인 결산서 여부
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Report")
	bool bHasPendingReport = false;

	// 미확인 결산서 데이터
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Report")
	FProjectReportData PendingReport;

	// ========== 프로젝트 티어 진행 (수주 숙련도 / 자체개발 해금) ==========
	// OfficeStageProgressManager(WorldSubsystem) 의 TierProgress 가 맵 전환 시 휘발되는 문제 해결.
	// WorldMap 의 TradeOrderManager 도 이 값을 빌딩별로 순회해 "가중 랜덤 티어 샘플링" 에 사용.
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|StageProgress")
	FProjectTierProgress TierProgress;

	// ========== 건물 특성 (BUILDING_TRAIT_SYSTEM v1.1) ==========

	// 이 건물에 장착된 특성 슬롯 (FName 키, 길이 3, NAME_None = 빈 슬롯)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Trait")
	FBuildingTraitSlotData TraitSlots;

	// ========== 스타터 프리셋 (OFFICE_STARTER_PRESET) ==========

	// 건설 시 예약된 프리셋 행 키 — 첫 오피스 입장 적용 후 None (None = 적용 완료/대상 아님)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|StarterPreset")
	FName PendingStarterPreset;

	// 시작 타일 수 — 확장 비용 지수의 기준점 (프리셋이 부여한 공짜 타일은 비용에 미포함)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|StarterPreset")
	int32 StarterTileCountX = 1;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|StarterPreset")
	int32 StarterTileCountY = 2;

	// ========== 백로그 (클리어->자동화). 운영 생애주기는 후속 태스크에서 제거 ==========
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Backlog")
	TArray<FBacklogProductEntry> BacklogProducts;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Office|Backlog")
	FOfflineVault OfflineVault;
};

/**
 * 건물 저장 데이터
 * - 층수, 파라미터, 적용된 스킨 정보
 * - 건물 강화 시스템
 */
USTRUCT(BlueprintType)
struct FBuildingSaveData
{
	GENERATED_BODY()

	// 층수 (바디 모듈 복사본 수)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	int32 Body_Module_Copies = 0;

	// 층 높이 스케일
	UPROPERTY(SaveGame, BlueprintReadWrite)
	float Floor_Height_BodyModuleScale = 1.0f;

	// UV 레이아웃 선택
	UPROPERTY(SaveGame, BlueprintReadWrite)
	float UV_Layout_Selection = 1.0f;

	// 창문 사이 벽 스위치
	UPROPERTY(SaveGame, BlueprintReadWrite)
	float Walls_Between_Windows_Switch = 0.0f;

	// 적용된 스킨 ID
	UPROPERTY(SaveGame, BlueprintReadWrite)
	int32 AppliedSkinID = 0;

	// 적용된 조명 ID (Window_Emissive_Color)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	int32 AppliedLightID = 0;

	// 회사 타입 (산업 분야) - None이면 최초 진입 시 선택 필요
	UPROPERTY(SaveGame, BlueprintReadWrite)
	ECompanyType CompanyType = ECompanyType::None;

	// ========== 건물 강화 시스템 (22종 enum → 레벨 매핑) ==========
	// 타입별 개별 UPROPERTY 대신 TMap 단일 저장.
	// 신규 강화 추가 시 구조체 변경 없이 DT + enum 추가만으로 확장 가능.
	// BuildingFloor는 여기 포함하지 않음 (Body_Module_Copies가 층수 정본).
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Enhancement")
	TMap<EBuildingEnhancementType, int32> EnhancementLevels;

	// ========== 채용 시스템 ==========

	// [DEPRECATED] 구 카드 시스템용 — 세이브 역호환 목적 유지, 런타임 무시
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Recruitment")
	FBuildingRecruitmentData RecruitmentData;
};
