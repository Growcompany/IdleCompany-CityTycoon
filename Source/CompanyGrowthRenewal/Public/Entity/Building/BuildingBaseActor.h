// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Entity/InteractableBaseActor.h"
#include "Enum/BuildingType.h"
#include "Enum/CompanyType.h"

#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Table/ConstructionCost.h"

#include "Table/BuildingData.h"
#include "Data/BuildingEnhancementData.h"
#include "Data/EntitySaveData.h"
#include "BuildingBaseActor.generated.h"

class UBoxComponent;
class UNavModifierComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class AMainMapPlayerController;
class ATimeCycleManager;

// 버블 재평가가 필요한 상태 변화(업종 선택, 직원 배치 등)에서 BuildingIndex 와 함께 브로드캐스트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildingBubbleRefreshRequested, int32, BuildingIndex);

UCLASS()
class COMPANYGROWTHRENEWAL_API ABuildingBaseActor : public AInteractableBaseActor
{
	GENERATED_BODY()

private:
	EBuildingType BuildingType = EBuildingType::None;

	// EInputMode::Normal
	bool bIsInteracting = false;

public:	
	ABuildingBaseActor();

	virtual void BeginDestroy() override;
	virtual void SetInteractableInfo(const FInteractableInfo& InInfo) override;
	void InitializeFromSaveData(const FInteractableInfo& InInfo, const FBuildingEntitySaveData& LoadData);
	FBuildingEntitySaveData GetBuildingSaveData() const;

	virtual float Interact() override;
	bool IsRequireBuild() const;
	//void AddBuilder(AVillager* Builder);
	//void RemoveBuilder(AVillager* Builder);

	virtual void OnInteract_Implementation(APlayerController* PC) override;
	virtual void OnEndInteract_Implementation(APlayerController* PC) override;

	// 층수 올리기. bShouldSave=false 시 호출자가 마지막에 한 번만 저장 (UpgradeEnhancement bulk 호출 N+1 disk write 방지)
	void AddFloor(bool bShouldSave = true);

	// 빌딩 placement 떨어지는 시퀀스 — Whoosh 즉시 + 0.202s 후 Body thud (peak alignment) + Rumble. LandBuilding 진입 시 호출
	UFUNCTION(BlueprintCallable, Category = "Building")
	void PlayPlacementLandSequence();

	// 건물 완성/업그레이드 시 VFX 재생
	// SpawnHeight: VFX 스폰 높이 (0 = 건물 바닥, finalHeight = 새로 추가된 층 아래)
	void PlayBuildCompleteVFX(float SpawnHeight = 0.0f);

	// BoxCollider 높이 재계산
	virtual void ReCalcBoxExtent() const override;

	// 건물 스킨 적용
	UFUNCTION(BlueprintCallable, Category = "Building Skin")
	void ApplySkin(int32 SkinID, bool bShouldSave = true);

	// 현재 적용된 스킨 ID 반환
	UFUNCTION(BlueprintCallable, Category = "Building Skin")
	int32 GetAppliedSkinID() const { return AppliedSkinID; }

	// 건물 조명(Window_Emissive_Color) 적용
	UFUNCTION(BlueprintCallable, Category = "Building Light")
	void ApplyLight(int32 LightID, bool bShouldSave = true);

	UFUNCTION(BlueprintCallable, Category = "Building Light")
	int32 GetAppliedLightID() const { return AppliedLightID; }

	// 건물 종류 ID 조회 (DataTable RowName, 예: "Building_1")
	UFUNCTION(BlueprintCallable, Category = "Building")
	FName GetBuildingID() const { return BuildingID; }

	// 건물 인덱스 조회/설정
	UFUNCTION(BlueprintCallable, Category = "Building")
	int32 GetBuildingIndex() const { return BuildingIndex; }

	void SetBuildingIndex(int32 InIndex) { BuildingIndex = InIndex; }

	// 업종(회사 타입) 조회
	UFUNCTION(BlueprintCallable, Category = "Building")
	ECompanyType GetCompanyType() const { return CompanyType; }

	// 업종 설정 + 저장
	void SetCompanyType(ECompanyType NewType);

	// 이 건물이 소속된 도시 블록(부지) PlotId 조회/설정 (저장 연동은 별도 — 여기선 멤버만)
	UFUNCTION(BlueprintCallable, Category = "Building")
	FName GetOwningPlotId() const { return OwningPlotId; }

	void SetOwningPlotId(FName InPlotId) { OwningPlotId = InPlotId; }

	// 버블 재평가가 필요한 상태 변화 발생 시 브로드캐스트 (InGameLayerWidget 구독)
	UPROPERTY(BlueprintAssignable, Category = "Building|Bubble")
	FOnBuildingBubbleRefreshRequested OnBubbleRefreshRequested;

	// ManagePanel 열기 (OnEndInteract에서 분리)
	void OpenManagePanel(AMainMapPlayerController* MainPC);

	// 버블 클릭 등 외부에서 건물 UI 열기 (bIsInteracting 체크 없음) — 항상 ManagePanel
	// (산업은 건설 시점에 베이크되므로 None 분기 없음)
	void OpenBuildingUI(AMainMapPlayerController* MainPC);

	// 창문 조명 토글 (프로젝트 유무에 따라 호출)
	void SetWindowLightActive(bool bActive);

	// [치트] 전역 강제 점등 — true 면 SetWindowLightActive(false) 요청을 무시하고 모든 건물 창문을 항상 켠 상태로 유지.
	// (조명 off 경로가 SetWindowLightActive 단일 창구뿐이라, 여기서 클램프하면 운영 여부/버블 재평가와 무관하게 고정된다.)
	static void SetForceWindowLightsOn(bool bForce) { bForceWindowLightsOn = bForce; }
	static bool IsForceWindowLightsOn() { return bForceWindowLightsOn; }

	// 클릭 같은거 했을 때 건물 흔들리는 효과
	virtual void PlayWobble() override;
	virtual void Wooble_TimelineFinished() override;
	virtual void Wooble_TimelineUpdate(float scaleValue) override;

	// 건물 배치/재배치 시 떠오르는 효과
	// bImmediate가 true면 애니메이션 없이 바로 떠있는 상태로 시작
	UFUNCTION(BlueprintCallable, Category = "Building|Animation")
	void StartFloating(float Height = 50.0f, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category = "Building|Animation")
	void StopFloating();

	// 건물 착지: 내려오는 애니메이션 + 완료 시 VFX 재생
	UFUNCTION(BlueprintCallable, Category = "Building|Animation")
	void LandBuilding();

	bool IsFloating() const { return bIsFloating; }

	// TopModule위치만 계산해주는 함수
	float CalculateTopModuleHeight() const;

	// 건물 전체 높이 계산 (Top Module 높이 포함)
	float GetTotalBuildingHeight() const;

	// 버블/UI 앵커용 월드 위치 (메시 수평 중심 + 건물 꼭대기)
	FVector GetBubbleAnchorPosition() const;

	// 최상단 모듈(지붕) 월드 바운드 / 중심 — 선택 마커 앵커
	FBox GetRoofBounds() const;
	FVector GetRoofAnchorPosition() const;

	// 옥상 footprint/높이에 맞춰 항공장애등 인스턴스를 재배치(높이 미달이면 0개)
	void UpdateRooftopBeacons();

	// 게임 데이터 즉시 저장 (건물 변경 시 사용)
	void SaveGameData();

	// 건물 완성 처리 (배치 완료 시 호출)
	void BuildSuccess();

protected:
	// 에디터에서 호출되는 함수
	virtual void OnConstruction(const FTransform& Transform) override;

	// 건물 외형을 구성하는 함수
	void ConstructVisuals(const FBuildingData* BuildingData);
	float RebuildBodyModules();
	void UpdateBuildingInfo(const float TotalHeight);

	// 건물 종류 식별 ID (DataTable RowName, 에디터 + 런타임 공용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Config")
	FName BuildingID;

	/* 베이스 모듈 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UInstancedStaticMeshComponent* Body_Module;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UInstancedStaticMeshComponent* Top_Module;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UInstancedStaticMeshComponent* Top_Empty_Module;

	// 옥상 항공장애등(야간 비콘). 엔진 Sphere + M_AviationBeacon. Top_Module ISM 패턴 미러.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Beacon")
	UInstancedStaticMeshComponent* BeaconISM;

	// 키스톤 모뉴먼트 영향권 반투명 돔(엔진 Sphere 의 위쪽 반구만 보임 — 아래 반구는 불투명 지면에 가림). 비-모뉴먼트는 항상 숨김.
	UPROPERTY(VisibleAnywhere, Category = "Building|Keystone")
	UStaticMeshComponent* KeystoneDomeMesh = nullptr;

	// 키스톤 영향권 멤버일 때 건물을 감싸는 파란 Candrop 박스(엔진 Cube + M_Placeable_BuffBlue). 건물 메시는 안 건드림. 기본 숨김.
	UPROPERTY(VisibleAnywhere, Category = "Building|Keystone")
	UStaticMeshComponent* AuraBoxSMC = nullptr;

	// AuraBoxSMC 의 동적 머티리얼 — 건물 높이에 맞춰 FadeHeight 주입(건물마다 일관된 페이드). 1회 생성 후 재사용.
	UPROPERTY()
	UMaterialInstanceDynamic* AuraBoxMID = nullptr;

	// 가림 고스트용 골조 박스(엔진 Cube + M_BuildingGhost). AuraBoxSMC 와 동시 활성될 수 있어 별도 인스턴스.
	UPROPERTY(VisibleAnywhere, Category = "Building|Ghost")
	UStaticMeshComponent* GhostBoxSMC = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* GhostBoxMID = nullptr;

	bool bOccluderGhosted = false;

	// 고스트 복원 시 되살릴 상태의 출처. 스냅샷이 아니라 출처를 들고 재계산해야
	// 고스트 중 낮/밤이 바뀌어도 복원이 틀리지 않는다.
	bool bAuraHighlightOn = false;
	bool bKeystoneRingOn = false;

	// 이 높이(cm) 미만 건물은 비콘 미점등(인스턴스 0).
	// **0 = 게이팅 OFF(모든 건물 점등) — 2026-07-23 사용자 육안 확인으로 정식 채택.**
	// 구 주석의 "[임시 진단]…적정값(예: 6000)으로 복구" 는 폐기 — 6000 은 검증된 값이 아니라 예시였고,
	// 실제 화면에서는 전 건물 점등이 의도한 그림이다. 낮은 건물의 비콘을 걷어내고 싶어지면 그때 PIE 로 실측해 올릴 것.
	UPROPERTY(EditAnywhere, Category = "Building|Beacon")
	float BeaconMinHeight = 0.f;

	// 옥상 한 변이 이 값(cm) 이상이면 4모서리, 미만이면 중앙 1개. PIE 보정.
	UPROPERTY(EditAnywhere, Category = "Building|Beacon")
	float BeaconFourCornerMinExtent = 1500.f;

	// 큰 비콘("L_Beacon" 소켓 / 자동 1~2개) 베이스 스케일. 최종 = Sphere 100cm × 이 값 × 소켓 Scale.
	UPROPERTY(EditAnywhere, Category = "Building|Beacon")
	float BeaconScaleLarge = 1.5f;

	// 보통 비콘("Beacon" 소켓 / 자동 4모서리) 베이스 스케일. 소켓 Scale로 소켓별 미세조정(리빌드 불필요).
	UPROPERTY(EditAnywhere, Category = "Building|Beacon")
	float BeaconScaleNormal = 0.75f;

	// 4모서리 배치 시 모서리에서 안쪽으로 들이는 거리(cm).
	UPROPERTY(EditAnywhere, Category = "Building|Beacon")
	float BeaconCornerInset = 250.f;

	// 옥상면에서 살짝 위로 띄우는 오프셋(cm).
	UPROPERTY(EditAnywhere, Category = "Building|Beacon")
	float BeaconZLift = 60.f;

	// Variable

	/* Material Instance 2 */
	UPROPERTY(EditAnywhere, Category = "Building Parameters")
	UMaterialInterface* Material_Main = nullptr;

	UPROPERTY(EditAnywhere, Category = "Building Parameters")
	UMaterialInterface* Material_RoofElements = nullptr;

	UPROPERTY(EditAnywhere, Category = "Building Parameters", meta = (ClampMin = "0", ClampMax = "50"))
	int32 Body_Module_Copies = 0;

	// Material Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Parameters", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float UV_Layout_Selection = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Parameters", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Walls_Between_Windows_Switch = 0;

	/* 바디 모듈 층(높이) 스케일 / 기본 바닥 모듈을 구성하는 스케일 */
	UPROPERTY(EditAnywhere, Category = "Building Parameters", meta = (ClampMin = "0.62", ClampMax = "1"))
	float Floor_Height_BodyModuleScale = 1;
	
	// 일반적으로 활용되는 층 높이 4m
	UPROPERTY(EditAnywhere, Category = "Building Parameters")
	float Standard_FloorHeight = 4;

	// 정보 표시 변수

	/* 건물 전체 높이 (m 단위, 소수점 포함) */
	UPROPERTY(VisibleAnywhere, Category = "Building Info")
	FText Building_Height;

	/* 지붕 요소를 제외한 건물 전체 높이 (m 단위, 소수점 포함) */
	UPROPERTY(VisibleAnywhere, Category = "Building Info")
	FText Building_Height_Without_RoofElements;

	/* 바디 모듈을 포함한 층수(Stories) */
	UPROPERTY(VisibleAnywhere, Category = "Building Info")
	FText NumberStories;

	// BuildingDataTable
	UPROPERTY(VisibleAnywhere, Category = "Building Data")
	UDataTable * BuildingDataTable = nullptr;

	// 건물 완성/업그레이드 시 재생할 먼지 VFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Building|VFX")
	class UNiagaraSystem* BuildCompleteVFX;

	// ========== 창문 조명 색상 ==========

	// 현재 창문 발광 색상
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Window Light")
	FLinearColor Window_Emissive_Color;

	// 창문 색상 초기화 (랜덤 선택)
	void InitializeWindowLightColor();

	// 창문 색상을 머티리얼에 적용
	void ApplyWindowLightColor();

	// 프로젝트 유무에 따른 창문 조명 활성/비활성
	bool bWindowLightActive = true;

	// [치트] 전역 강제 점등 플래그(모든 인스턴스 공유). true 면 SetWindowLightActive 가 꺼짐 요청을 무시.
	static bool bForceWindowLightsOn;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitializeStaticMesh(const FName& name) override;
	virtual void RegisterWithEntityManager() override;
	virtual void UnregisterWithEntityManager() override;

	// day/night 전환 시 옥상 비콘 ISM 가시성만 토글(낮=숨김=렌더0, 밤=표시) — 인스턴스는 보존.
	UFUNCTION()
	void HandleSunRise();
	UFUNCTION()
	void HandleSunSet();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bGlowVisible = true; // 시계 못 찾으면 표시 유지(밤 비콘 회귀 방지)

private:
	// 건물 고유 인덱스 (저장/로드 및 Map 키용)
	UPROPERTY()
	int32 BuildingIndex = INDEX_NONE;

	// 적용된 스킨 ID (기본: 100 = Common 기본 스킨)
	UPROPERTY()
	int32 AppliedSkinID = 100;

	// 적용된 조명 ID (기본: 100 = Common 따뜻한 노랑)
	UPROPERTY()
	int32 AppliedLightID = 100;

	// 회사 타입 (Office에서 선택)
	UPROPERTY()
	ECompanyType CompanyType = ECompanyType::None;

	// 이 건물이 소속된 도시 블록(부지) PlotId. 세이브 연동은 Task 4에서 처리.
	UPROPERTY()
	FName OwningPlotId = NAME_None;

	void ApplyMobileFacadeMaterialTuning();

	// 떠오르는 효과 관련
	bool bIsFloating = false;
	float OriginalZ = 0.0f;
	float FloatingHeight = 0.0f;
	float FloatingAlpha = 0.0f;
	FTimerHandle FloatingTimerHandle;
	bool bFloatingUp = true;
	bool bShouldPlayLandVFX = false;  // 착지 시 VFX 재생 여부

	void UpdateFloatingAnimation();

	// ===== 증축(AddFloor) 새 층 밀어올림 연출 — 최상단 Body 인스턴스 1개만 아래→제자리로 =====
	// 통짜 floating(배치용)과 별개 채널: "층이 하나 쌓였다"가 읽히려면 새 층만 움직여야 함
	void StartNewFloorRiseAnimation();
	void UpdateNewFloorRiseAnimation();
	FTimerHandle NewFloorRiseTimerHandle;
	float NewFloorRiseElapsed = 0.0f;   // 경과 시간(초)
	float NewFloorRiseDrop = 0.0f;      // 시작 낙차(cm) — 층 높이 기반
	int32 NewFloorRiseInstanceIndex = INDEX_NONE;
	float NewFloorRiseFinalZ = 0.0f;    // 제자리 Z(로컬)

	// ========== 건물 강화 시스템 (22종 enum — TMap 통합) ==========
	// 타입별 개별 필드 대신 TMap으로 저장. 신규 강화 추가 시 필드 추가 불필요.
	// BuildingFloor는 Body_Module_Copies가 정본이므로 이 맵에 포함하지 않음.
	UPROPERTY()
	TMap<EBuildingEnhancementType, int32> EnhancementLevels;

	// ========== 키스톤 모뉴먼트(특수빌딩) ==========

	// 키스톤 모뉴먼트 여부(ConstructVisuals 시 FBuildingData에서 캐시).
	UPROPERTY()
	bool bIsKeystoneMonument = false;

	// footprint 칸수(가로x세로). ConstructVisuals 시 FBuildingData에서 캐시 — 증축 비용이 매 홀드(10Hz) 참조한다.
	// 0 = 아직 캐시 안 됨(센티널). 1 을 기본값으로 두면 "1x1 건물"과 "캐시 실패"가 구분되지 않아
	// 3x3 이 1/9 가격에 증축되는 걸 아무도 못 잡는다 — 가격은 fail-open 하면 안 되는 축이다.
	UPROPERTY()
	int32 CachedFootprintCells = 0;

public:
	// 증축 비용 배수의 단일 출처 — 표시(패널)와 청구(액터)가 반드시 같은 값을 써야 한다.
	// 캐시가 비어 있으면 DT 를 직접 조회하고, 그마저 실패하면 Warning 을 남긴다.
	int32 GetFootprintCells() const;

	// ========== 키스톤 모뉴먼트(특수빌딩) ==========

	bool IsKeystoneMonument() const { return bIsKeystoneMonument; }
	// 모뉴먼트 레벨은 층수 강화(BuildingFloor)에서 파생 — 층을 올리면 영향권/패시브가 함께 성장
	int32 GetMonumentLevel() const { return 1 + GetEnhancementLevel(EBuildingEnhancementType::BuildingFloor); }
	// 현재 레벨 기준 영향권(돔) 반경(cm). 비-모뉴먼트/글로벌은 0. UpdateKeystoneRing 의 반경식과 동일.
	float GetKeystoneAuraRadiusCm() const;
	// 현재 레벨 기준 자체 패시브 산출(/tick). FBuildingData.KeystoneAura에서 산출.
	int64 GetMonumentPassiveOutput() const;

	// 영향권 시각화: 모뉴먼트면 반경에 맞춰 바닥 링 표시, 아니면 숨김
	void UpdateKeystoneRing();

	// 영향권 링 표시 토글 — 관리 패널이 열려 선택된 모뉴먼트일 때만 보이게
	void SetKeystoneRingVisible(bool bVisible);

	// 건물 본체 메시(MainMesh + Body/Top/TopEmpty ISM)의 월드 바운드 합산.
	// 고스트 박스/아우라 박스가 자기 바운드로 부풀지 않도록 박스 컴포넌트는 제외한다.
	FBox GetBuildingWorldBounds() const;

	// 이 빌딩이 받는 버프(키스톤 영향권 멤버) 표시: 건물을 감싸는 파란 Candrop 박스(M_Placeable_BuffBlue) 토글. 건물 본체는 안 건드림.
	void SetAuraHighlight(bool bOn);

	// 가림 고스트 — 본체 메시를 숨기고 반투명 골조 박스로 대체한다(카메라 포커스 가림 해소).
	void SetOccluderGhost(bool bOn);

	// 고스트 박스 불투명도만 갱신(페이드용). 고스트 상태가 아니면 무시된다.
	void SetGhostOpacity(float Opacity);

	bool IsOccluderGhosted() const { return bOccluderGhosted; }

	// ========== 건물 강화 시스템 함수들 ==========

	/**
	 * 건물 강화 스탯 업그레이드
	 * @param EnhancementType 강화할 스탯 타입
	 * @param Count 업그레이드할 횟수 (기본 1)
	 * @return 업그레이드 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	bool UpgradeEnhancement(EBuildingEnhancementType EnhancementType, int32 Count = 1);

	// 실제 구매 권위 판정. 튜토리얼 중에는 빌드업만, 완료 후에는 빌딩 티어/키스톤 규칙을 적용한다.
	UFUNCTION(BlueprintPure, Category = "Building Enhancement")
	bool IsEnhancementPurchaseAllowed(EBuildingEnhancementType EnhancementType) const;

	/**
	 * 특정 강화 스탯의 현재 레벨 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	int32 GetEnhancementLevel(EBuildingEnhancementType EnhancementType) const;

	/**
	 * 특정 강화 스탯의 효과 배율 반환
	 * @return 효과 배율 (예: 1.5 = 150%)
	 */
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	float GetEnhancementMultiplier(EBuildingEnhancementType EnhancementType) const;

	/**
	 * 다음 레벨 업그레이드 비용 계산
	 */
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	int64 GetUpgradeCost(EBuildingEnhancementType EnhancementType) const;

	/**
	 * 대량 업그레이드 비용 계산
	 */
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	int64 GetBulkUpgradeCost(EBuildingEnhancementType EnhancementType, int32 Count) const;

	// 금고 절대 하한 및 매니저 불가 시 폴백, 원. T1 첫 프로젝트 200/sec × Lv.0 5분.
	static constexpr float BaseVaultCapacity = 60000.0f;

	/**
	 * 금고 용량 = 티어 BandStart 기준 수익률 × VaultSeconds(액터의 현재 VaultCapacity 강화 레벨).
	 * OperationManager를 사용할 수 없으면 BaseVaultCapacity를 반환한다.
	 * @return 금고 용량 (원)
	 */
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	float GetVaultCapacity() const;
};
