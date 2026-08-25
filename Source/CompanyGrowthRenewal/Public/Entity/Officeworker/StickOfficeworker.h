// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/Officeworker/Officeworker.h"  // EEmployeeDepartment/Rank/Gender 전이 포함
#include "Enum/GachaTier.h"
#include "Enum/FaceExpression.h"
#include "StickOfficeworker.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UAnimSequence;
struct FEmployeeInstance;
struct FWorkerCosmeticTable;

/**
 * 스틱맨 직원 — 단일 Mixamo 호환 SkeletalMesh(GetMesh()) + 슬롯 머티리얼/코스메틱 메시.
 * 부모 AOfficeworker(깨끗한 게임플레이 베이스: 배회/워크스테이션/버블/초상화/Behavior)를 상속.
 * 구 모듈러 외형 파이프라인은 AModularOfficeworker(Male/Female 전용)로 분리되어 이 클래스와 무관.
 * 외형 진입점: ApplyWorkerCosmetics (DT_WorkerCosmetic 단일 진실).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AStickOfficeworker : public AOfficeworker
{
	GENERATED_BODY()

public:
	AStickOfficeworker();

	// 스폰 후 외부(GameMode)에서 호출: 외형 축 → DT lookup → 머티리얼 파라미터 + 안경 토글.
	// 몸 희귀도 Overlay는 bIsGachaReveal=true인 Premium 공개 연출에서만 추가한다.
	UFUNCTION(BlueprintCallable, Category = "StickWorker|Cosmetic")
	void ApplyWorkerCosmetics(EEmployeeDepartment Department, EEmployeeRank Rank,
		EGachaTier Tier, int32 InEmployeeID, bool bIsGachaReveal = false);

	UFUNCTION(BlueprintPure, Category = "StickWorker|Cosmetic")
	bool IsWearingGlasses() const { return bWearingGlasses; }

	// DT RowName 합성 (결정적): "D{dept}_R{rank}_T{tier}"
	static FName MakeCosmeticRowName(EEmployeeDepartment Department, EEmployeeRank Rank, EGachaTier Tier);

	// 몸 희귀도 Overlay는 Premium 가챠 공개 연출에서만 사용한다.
	static bool ShouldShowRarityOverlay(EGachaTier Tier, bool bIsGachaReveal);

	// 저장된 ELootBoxRarity(6단계) → EGachaTier(3단계) 압축. 골드 손은 Premium,
	// 골드 림은 Premium 가챠 공개 컨텍스트에서만 사용한다.
	// 스폰/뽑기/초상화 전 경로의 단일 진실 (OfficeGameMode/Recruitment/EmployeeManager 공용).
	static EGachaTier ResolveGachaTier(const FEmployeeInstance& Employee);

	// 초상화 캡쳐용 외형 진입점: 코스메틱 적용 + 중립 표정 고정 + 메시 로드 델리게이트 수동 발화.
	// (스틱은 메시가 이미 로드돼 있어 부모의 비동기 ApplyAppearanceAndNotify 대신 이 동기 경로를 탄다.)
	void ApplyCosmeticsForPortrait(EEmployeeDepartment Department, EEmployeeRank Rank,
		EGachaTier Tier, int32 InEmployeeID);

	// 라이브 캡처(직원창 히어로)용 제자리 걷기 — PortraitPoseAnim(AS_Walk)을 SingleNode 무한 루프 재생.
	// 무대는 MOVE_None 이라 이동 없음, LeaderPose 코스메틱은 바디 자동 추종.
	void PlayWalkLoopForCapture();

	// 얼굴 표정 지정 — 아틀라스 UV 오프셋만 바꾼다(텍스처 스왑 없음)
	UFUNCTION(BlueprintCallable, Category = "StickWorker|Face")
	void SetFaceExpression(EWorkerFaceExpression Expression);

	// 행동 상태 → 표정. 졸음/폭주처럼 "지금 무슨 일이 벌어지는가"를 얼굴이 직접 읽는다.
	// 버블(EWorkerBubbleType)은 개발 중 내내 Working 으로 고정이라 표정 구동원으로 못 쓴다.
	UFUNCTION(BlueprintPure, Category = "StickWorker|Face")
	EWorkerFaceExpression ResolveFaceExpression() const;

	// ───────────────────────── 에디터 프리뷰 (런타임 무영향) ─────────────────────────
	// bEditorPreview ON → OnConstruction 이 실제 ApplyWorkerCosmetics 파이프라인으로 부서색/직급/희귀도/기분을
	// BP 뷰포트에 즉시 반영(몸 슬롯 머티리얼 포함). 게임 월드(스폰)에선 GameMode 가 권위 적용하므로 자동 무시.
	UPROPERTY(EditAnywhere, Category = "StickWorker|Preview")
	bool bEditorPreview = true;

	UPROPERTY(EditAnywhere, Category = "StickWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EEmployeeDepartment PreviewDepartment = EEmployeeDepartment::Development;

	UPROPERTY(EditAnywhere, Category = "StickWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EEmployeeRank PreviewRank = EEmployeeRank::Manager;

	UPROPERTY(EditAnywhere, Category = "StickWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EGachaTier PreviewTier = EGachaTier::Normal;

	UPROPERTY(EditAnywhere, Category = "StickWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EWorkerFaceExpression PreviewExpression = EWorkerFaceExpression::Neutral;

	UPROPERTY(EditAnywhere, Category = "StickWorker|Preview", meta = (EditCondition = "bEditorPreview", ClampMin = "0"))
	int32 PreviewEmployeeID = 1;

#if WITH_EDITOR
	// 뷰포트 버튼: EmployeeID 를 1 증가시켜 개인 변주(스킨톤/헤어색/옷 밝기) 다음 샘플을 즉시 확인.
	UFUNCTION(CallInEditor, Category = "StickWorker|Preview")
	void PreviewNextEmployee();
#endif

protected:
	// 신규 지오메트리 1: 안경 — Head 본 리지드 스킨(SK_Glasses, 스켈레톤 공유) + LeaderPose. 머리/넥타이와 동일.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StickWorker")
	USkeletalMeshComponent* GlassesComponent = nullptr;

	// 코스메틱 5종 (넥타이/클로그L·R/벨트/카라) — 본에 리지드 스킨된 SkeletalMesh(SK_Cos_*).
	// GetMesh() 에 부착 + LeaderPose → 바디 포즈/스케일 자동 추종. 위치/회전/스케일 수동 보정 불필요.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StickWorker")
	USkeletalMeshComponent* NecktieComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StickWorker")
	USkeletalMeshComponent* BeltComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StickWorker")
	USkeletalMeshComponent* CollarComponent = nullptr;

	// 신규 지오메트리 7: 머리카락 캡 — Head 본에 리지드 스킨된 SkeletalMesh(SK_Cos_Hair). LeaderPose 로 바디 추종.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StickWorker")
	USkeletalMeshComponent* HairComponent = nullptr;

	// 안경 스켈레탈 폴백 (GlassesVariants 비었을 때 사용)
	UPROPERTY(EditDefaultsOnly, Category = "StickWorker|Assets")
	TSoftObjectPtr<USkeletalMesh> DefaultGlassesMesh;

	// 안경 변주 풀 — EmployeeID 결정적 픽(직원별 고정·전체 다양). 리지드 스킨 SkeletalMesh(LeaderPose). 스킨/머리색과 동일 패턴.
	UPROPERTY(EditAnywhere, Category = "StickWorker|Assets")
	TArray<TSoftObjectPtr<USkeletalMesh>> GlassesVariants;

	// 안경 착용 확률(0~1) — 등급 무관 완전 랜덤, EmployeeID 결정적 게이트. 0=아무도, 1=전원.
	UPROPERTY(EditAnywhere, Category = "StickWorker|Assets", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlassesChance = 0.5f;   // 약 절반 착용

	// 초상화 정지 포즈 — 레퍼런스(T)포즈 대신 이 시퀀스의 한 프레임으로 굳혀 자연스러운 스탠스로 촬영.
	// 기본=AS_Walk 0초(걷는 첫 프레임). BP 에서 다른 포즈/시간으로 교체 가능.
	UPROPERTY(EditDefaultsOnly, Category = "StickWorker|Portrait")
	TSoftObjectPtr<UAnimSequence> PortraitPoseAnim;

	UPROPERTY(EditDefaultsOnly, Category = "StickWorker|Portrait", meta = (ClampMin = "0.0"))
	float PortraitPoseTime = 0.f;

	// 희귀도 골드 림 전용 머티리얼(선택). 미지정 시 부모 SelectionOverlayMaterial 재사용.
	UPROPERTY(EditDefaultsOnly, Category = "StickWorker|Assets")
	UMaterialInterface* RimOverlayMaterial = nullptr;

	// ── 슬롯 머티리얼 베이스 (생성자 ConstructorHelpers 로드 → 런타임 슬롯별 SetMaterial) ──
	// 표정은 더 이상 떠 있는 FacePlane 이 아니라, 머리 앞면 "Face" 슬롯(M_StickFaceSlot)의
	// FaceTex 를 스왑해서 곡면 머리에 직접 입힌다. 색 영역은 메시 슬롯 분리(M_StickRegion).
	UPROPERTY()
	UMaterialInterface* RegionBaseMat = nullptr;   // M_StickRegion (RegionColor/Metallic)

	UPROPERTY()
	UMaterialInterface* FaceBaseMat = nullptr;     // M_StickFaceSlot (FaceTex/SkinTone)

	// 전체 액터 균일 스케일(캡슐 포함 동반) — 저폴리 스틱맨이 슬림한 구 직원보다 덩치가 커 보여 축소 튜닝용.
	// BP_StickOfficeworker 에서 값 조절 시 뷰포트에 즉시 반영(OnConstruction). 1.0=원본 182cm.
	UPROPERTY(EditDefaultsOnly, Category = "StickWorker", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BodyScale = 1.0f;

	// AOfficeworker PURE_VIRTUAL 구현 (컴파일 필수)
	virtual void LoadAssetsAsync() override;
	virtual void OnAssetsLoaded() override;
	virtual EEmployeeGender GetCharacterGender() const override { return EEmployeeGender::Male; }  // 유니섹스

	// BodyScale 을 액터 전체에 적용 (에디터 프리뷰 + 스폰 시점). BP 기본값 변경이 반영되는 진입점.
	virtual void OnConstruction(const FTransform& Transform) override;

	// 리지드 스킨 코스메틱에 LeaderPose(GetMesh()) 설정 — 바디 포즈 추종.
	// 구 ModularOfficeworker 와 동일하게 PostInitializeComponents(런타임 1회, 메시 셋업 후)에서 건다.
	// PostRegisterAllComponents 는 재등록마다 여러 번 호출돼 런타임 LeaderPose 바인딩이 꼬여 코스메틱이 분리됨(2026-06-11 회귀 수정).
	// 에디터 정적 프리뷰는 LeaderPose 안 걸리지만 애님/게임 확인엔 PMC·PIE 사용하므로 무관.
	virtual void PostInitializeComponents() override;

	// 부모 1초 mood 평가(CurrentBubbleType)를 읽어 표정 동기화
	virtual void Tick(float DeltaTime) override;

private:
	bool bWearingGlasses = false;

	UPROPERTY()
	UMaterialInstanceDynamic* FaceMID = nullptr;

	EWorkerFaceExpression LastFaceExpression = EWorkerFaceExpression::Max;   // Max = 미적용(첫 프레임 강제 반영)

	// 얼굴 아틀라스 레이아웃 — T_FaceAtlas. 정사각 그리드 + (정체성*표정수 + 표정) 선형 인덱스.
	// 정사각인 이유: 구 8x32 세로 스트립은 긴 변이 4096이라 모바일 텍스처 그룹 상한(1024)에서
	// 밉 2장이 강제로 깎여 셀이 32px까지 무너졌다. 텍셀 수가 같아도 정사각이면 1장만 깎인다.
	// 생성기(Tools/FaceAtlas/facegen.py atlas)가 아틀라스를 구울 때 이 헤더를 읽어 불일치를 경고한다.
	static constexpr int32 FaceAtlasColumns    = 16;   // 그리드 열 (표정 수가 아님)
	static constexpr int32 FaceAtlasRows       = 16;   // 그리드 행 (남는 슬롯은 X 마커)
	static constexpr int32 FaceExpressionCount = static_cast<int32>(EWorkerFaceExpression::Max);
	static constexpr int32 FaceIdentityCount   = 26;   // 실제 사용 정체성

	// 외형 축별 결정적 해시. 축마다 다른 솔트를 써야 축끼리 상관이 사라져 조합 공간이 온전히 열린다.
	static uint32 AppearanceHash(int32 EmployeeID, uint32 Salt);

	// 코스메틱 6종 소켓-상대 트랜스폼을 BP 튜닝값(*Xform)으로 적용. ApplyWorkerCosmetics / OnConstruction 에서 호출.
	void FitAllCosmetics();

	// 코스메틱 행 조회 — 런타임=TableManagerSubsystem 캐시, 에디터(게임인스턴스 없음)=DT_WorkerCosmetic 직접 로드 폴백.
	bool ResolveCosmeticRow(EEmployeeDepartment Department, EEmployeeRank Rank, EGachaTier Tier,
		FWorkerCosmeticTable& OutRow) const;

	// 슬롯별 베이스 머티리얼(M_StickRegion/M_StickFaceSlot) + MID 셋업(멱등). 얼굴 슬롯 MID 를 FaceMID 에 캐시.
	void SetupSlotMaterials();
};
