// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/Officeworker/Officeworker.h"  // EEmployeeDepartment/Rank/Gender 전이 포함
#include "Enum/GachaTier.h"
#include "Enum/FaceExpression.h"
#include "Enum/FunnyPartSlot.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "Math/RandomStream.h"
#include "FunnyOfficeworker.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UAnimSequence;
class UTableManagerSubsystem;
struct FEmployeeInstance;
struct FFunnyWorkerCosmeticTable;
struct FFunnyExpressionTable;
struct FFunnyFaceStyleTable;
class UEmployeeReactionComponent;

/**
 * Funny 직원 — 단일 Mixamo 호환 SkeletalMesh(GetMesh()) + 슬롯 머티리얼/코스메틱 메시.
 * 부모 AOfficeworker(깨끗한 게임플레이 베이스: 배회/워크스테이션/버블/초상화/Behavior)를 상속.
 * 외형 진입점: ApplyWorkerCosmetics (DT_WorkerCosmetic 단일 진실).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AFunnyOfficeworker : public AOfficeworker
{
	GENERATED_BODY()

public:
	AFunnyOfficeworker();

	// 스폰 후 외부(GameMode)에서 호출: 외형 축 → DT lookup → 머티리얼 파라미터 + 안경 토글.
	// 몸 희귀도 Overlay는 bIsGachaReveal=true인 Premium 공개 연출에서만 추가한다.
	UFUNCTION(BlueprintCallable, Category = "FunnyWorker|Cosmetic")
	void ApplyWorkerCosmetics(EEmployeeDepartment Department, EEmployeeRank Rank,
		EGachaTier Tier, int32 InEmployeeID, EEmployeeGender InGender, bool bIsGachaReveal = false);

	UFUNCTION(BlueprintPure, Category = "FunnyWorker|Cosmetic")
	bool IsWearingGlasses() const { return bWearingGlasses; }

	// DT RowName 합성 (결정적): "D{dept}_R{rank}_T{tier}"
	static FName MakeCosmeticRowName(EEmployeeDepartment Department, EEmployeeRank Rank, EGachaTier Tier);

	// 몸 희귀도 Overlay는 Premium 가챠 공개 연출에서만 사용한다.
	static bool ShouldShowRarityOverlay(EGachaTier Tier, bool bIsGachaReveal);

	// 저장된 ELootBoxRarity(6단계) → EGachaTier(3단계) 압축. 골드 손은 Premium,
	// 골드 림은 Premium 가챠 공개 컨텍스트에서만 사용한다.
	// 스폰/뽑기/초상화 전 경로의 단일 진실 (OfficeGameMode/Recruitment/EmployeeManager 공용).
	static EGachaTier ResolveGachaTier(const FEmployeeInstance& Employee);

	// 실제 파츠 추첨과 같은 경로를 모아 비동기 로드한다. 핸들은 소비처가 표시/촬영 종료까지 보유한다.
	static void GatherAppearanceAssets(UGameInstance* Instance, int32 InEmployeeID,
		EEmployeeGender InGender, TArray<FSoftObjectPath>& OutPaths);
	static bool AreAppearanceAssetsReady(const TArray<FSoftObjectPath>& Paths);
	// 얼굴 판·눈·입 적용이 성공해 초상화에 사용할 수 있는지 확인한다.
	bool IsFaceSheetReady() const;
	FSoftObjectPath GetPortraitPoseAssetPath() const { return PortraitPoseAnim.ToSoftObjectPath(); }

	// 초상화 캡쳐용 외형 진입점: 코스메틱 적용 + 중립 표정 고정 + 메시 로드 델리게이트 수동 발화.
	// (메시가 이미 로드돼 있어 비동기 로드 대기 없이 이 동기 경로를 탄다.)
	void ApplyCosmeticsForPortrait(EEmployeeDepartment Department, EEmployeeRank Rank,
		EGachaTier Tier, int32 InEmployeeID, EEmployeeGender InGender);
	virtual void SetupFaceCapture() override;

	// 라이브 캡처(직원창 히어로)용 제자리 걷기 — PortraitPoseAnim(AS_Walk)을 SingleNode 무한 루프 재생.
	// 무대는 MOVE_None 이라 이동 없음, LeaderPose 코스메틱은 바디 자동 추종.
	void PlayWalkLoopForCapture();

	// 미리보기는 이 액터에만 적용하며 장착 데이터와 보유 외형을 변경하지 않는다.
	bool PreviewEquipmentAppearance(EEmployeeEquipmentSlot EquipmentSlot, EEquipmentAppearanceMode Mode, FName AppearanceId);
	void ClearEquipmentAppearancePreview();

	// 얼굴 표정 지정 — DT 행의 눈·입 칸과 볼터치를 얼굴 판 MID 에 넣는다(메시 교체 없음).
	UFUNCTION(BlueprintCallable, Category = "FunnyWorker|Face")
	void SetFaceExpression(EWorkerFaceExpression Expression);

	// 행동 상태 → 표정. 졸음/폭주처럼 "지금 무슨 일이 벌어지는가"를 얼굴이 직접 읽는다.
	// 버블(EWorkerBubbleType)은 개발 중 내내 Working 으로 고정이라 표정 구동원으로 못 쓴다.
	UFUNCTION(BlueprintPure, Category = "FunnyWorker|Face")
	EWorkerFaceExpression ResolveFaceExpression() const;

	// 반응용 임시 표정 — Duration 초(월드 시간) 동안 ResolveFaceExpression 보다 우선한다.
	void PlayFaceReaction(EWorkerFaceExpression Expression, float Duration);

	// 미리보기 치트 전용 고정 표정(반응·상태보다 우선). Max = 해제.
	void SetDebugFaceOverride(EWorkerFaceExpression Expression);

	virtual bool IsDozeVisualActive() const override;

	// ───────────────────────── 에디터 프리뷰 (런타임 무영향) ─────────────────────────
	// bEditorPreview ON → OnConstruction 이 실제 ApplyWorkerCosmetics 파이프라인으로 부서색/직급/희귀도/기분을
	// BP 뷰포트에 즉시 반영(몸 슬롯 머티리얼 포함). 게임 월드(스폰)에선 GameMode 가 권위 적용하므로 자동 무시.
	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Preview")
	bool bEditorPreview = true;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EEmployeeDepartment PreviewDepartment = EEmployeeDepartment::Development;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EEmployeeRank PreviewRank = EEmployeeRank::Manager;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EGachaTier PreviewTier = EGachaTier::Normal;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Preview", meta = (EditCondition = "bEditorPreview"))
	EWorkerFaceExpression PreviewExpression = EWorkerFaceExpression::Neutral;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Preview", meta = (EditCondition = "bEditorPreview", ClampMin = "0"))
	int32 PreviewEmployeeID = 1;

#if WITH_EDITOR
	// 뷰포트 버튼: EmployeeID 를 1 증가시켜 개인 변주(스킨톤/헤어색/옷 밝기) 다음 샘플을 즉시 확인.
	UFUNCTION(CallInEditor, Category = "FunnyWorker|Preview")
	void PreviewNextEmployee();
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* HeadwearComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* BackComponent = nullptr;

	// 신규 지오메트리 1: 안경 — Head 본 리지드 스킨(SK_Glasses, 스켈레톤 공유) + LeaderPose. 머리/넥타이와 동일.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* GlassesComponent = nullptr;

	// 수염 — 남성 한정, 개인 시드 확률. 색은 헤어색을 따른다(틴트 전용).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* MustacheComponent = nullptr;

	// 장갑 — 팩 20종 임포트만, 풀 Weight 0(스킨 예약). 행이 켜지면 자동 배정.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* GloveComponent = nullptr;

	// 코스메틱 슬롯 — 전부 SKEL_Funny 스켈레톤 공유 리지드 스킨. GetMesh() 부착 + LeaderPose 로 바디 추종.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* OuterwearComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* PantsComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* ShoeComponent = nullptr;

	// 얼굴 판(SK_FaceSheet) — 머리 앞에 얹은 얇은 판에 눈·입·볼 그림을 칸으로 고른다. 슬롯 이름은 옛 Eyebrow 를 잇는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* EyebrowComponent = nullptr;

	// 신규 지오메트리 7: 머리카락 캡 — Head 본에 리지드 스킨된 SkeletalMesh(SK_Cos_Hair). LeaderPose 로 바디 추종.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	USkeletalMeshComponent* HairComponent = nullptr;

	// 몸 반응 연출(깨우기·잡기 복귀·찌르기·크리·분야 달성·졸음 표시) — 반응 중에만 틱
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FunnyWorker")
	UEmployeeReactionComponent* ReactionComponent = nullptr;

	// 슬롯별 MID 캐시 — 매 프레임 재생성 방지. 인덱스 = (int32)EFunnyPartSlot.
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> SlotMIDs;

	// M_FunnyWorker (아틀라스 × TintColor + Metallic/Emissive)
	UPROPERTY()
	UMaterialInterface* FunnyBaseMat = nullptr;

	// 얼굴 판 MID 부모(M_FunnyFaceSheet). 에디터 스크립트가 나중에 만드는 에셋이라 생성자 하드 참조 대신 첫 사용 때 소프트 로드한다.
	UPROPERTY(Transient)
	UMaterialInterface* FunnyFaceBaseMat = nullptr;

	// 의상 존 전용 MIC(GarmentZonesEnabled=true). StaticSwitch 는 런타임 MID 에서 못 켜서 별도 부모가 필요하다.
	UPROPERTY()
	UMaterialInterface* FunnyGarmentMat = nullptr;

	// 몸 피부와 이너 셔츠 영역을 함께 그리는 전용 MIC. 생성 이후 첫 적용 때 로드한다.
	UPROPERTY(Transient)
	UMaterialInterface* FunnyBodyMat = nullptr;

	// 슬롯 컴포넌트를 enum 순서로 반환 — 파츠 적용 루프가 이 순서를 계약으로 삼는다.
	USkeletalMeshComponent* GetSlotComponent(EFunnyPartSlot PartSlot) const;

	// 초상화 정지 포즈 — 레퍼런스(T)포즈 대신 이 시퀀스의 한 프레임으로 굳혀 자연스러운 스탠스로 촬영.
	// 기본=AS_Walk 0초(걷는 첫 프레임). BP 에서 다른 포즈/시간으로 교체 가능.
	UPROPERTY(EditDefaultsOnly, Category = "FunnyWorker|Portrait")
	TSoftObjectPtr<UAnimSequence> PortraitPoseAnim;

	UPROPERTY(EditDefaultsOnly, Category = "FunnyWorker|Portrait", meta = (ClampMin = "0.0"))
	float PortraitPoseTime = 0.f;

	// 카메라 로컬 기준 광원 방향: -X=카메라 쪽, -Y=화면 왼쪽, +Z=위쪽.
	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Portrait")
	FVector PortraitLightDirection = FVector(-1.f, -0.55f, 0.75f);

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Portrait")
	FLinearColor PortraitShadowTint = FLinearColor(0.62f, 0.66f, 0.74f);

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Portrait", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float PortraitShadowThreshold = 0.28f;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Portrait", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PortraitShadowSoftness = 0.55f;

	UPROPERTY(EditAnywhere, Category = "FunnyWorker|Portrait", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float PortraitBrightness = 1.12f;

	// 희귀도 골드 림 전용 머티리얼(선택). 미지정 시 부모 SelectionOverlayMaterial 재사용.
	UPROPERTY(EditDefaultsOnly, Category = "FunnyWorker|Assets")
	UMaterialInterface* RimOverlayMaterial = nullptr;

	// 전체 액터 균일 스케일(캡슐 포함 동반) — 저폴리 스틱맨이 슬림한 구 직원보다 덩치가 커 보여 축소 튜닝용.
	// BP_FunnyOfficeworker 에서 값 조절 시 뷰포트에 즉시 반영(OnConstruction). 1.0=원본 182cm.
	UPROPERTY(EditDefaultsOnly, Category = "FunnyWorker", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BodyScale = 1.0f;

	// AOfficeworker PURE_VIRTUAL 구현 (컴파일 필수)
	virtual void LoadAssetsAsync() override;
	virtual void OnAssetsLoaded() override;
	virtual EEmployeeGender GetCharacterGender() const override { return EEmployeeGender::Male; }  // 유니섹스

	// BodyScale 을 액터 전체에 적용 (에디터 프리뷰 + 스폰 시점). BP 기본값 변경이 반영되는 진입점.
	virtual void OnConstruction(const FTransform& Transform) override;

	// 리지드 스킨 코스메틱에 LeaderPose(GetMesh()) 설정 — 바디 포즈 추종.
	// PostInitializeComponents(런타임 1회, 메시 셋업 후)에서 건다.
	// PostRegisterAllComponents 는 재등록마다 여러 번 호출돼 런타임 LeaderPose 바인딩이 꼬여 코스메틱이 분리됨(2026-06-11 회귀 수정).
	// 에디터 정적 프리뷰는 LeaderPose 안 걸리지만 애님/게임 확인엔 PMC·PIE 사용하므로 무관.
	virtual void PostInitializeComponents() override;

	// 행동 상태를 읽어 표정 동기화 + 깜빡임
	virtual void Tick(float DeltaTime) override;

private:
	static TSoftObjectPtr<USkeletalMesh> ResolveSlotMesh(const UTableManagerSubsystem* Tables,
		EFunnyPartSlot PartSlot, int32 InEmployeeID, EEmployeeGender InGender, bool* bOutHasSelection = nullptr);
	UMaterialInterface* GetBodyMaterial();
	UMaterialInterface* GetSlotMaterial(EFunnyPartSlot PartSlot);

	bool bWearingGlasses = false;
	bool bHasCosmeticContext = false;
	bool bEquipmentPreview = false;
	EEmployeeDepartment AppliedDepartment = EEmployeeDepartment::None;
	EEmployeeRank AppliedRank = EEmployeeRank::Intern;
	EGachaTier AppliedTier = EGachaTier::Normal;
	EEmployeeGender AppliedGender = EEmployeeGender::Male;
	bool bAppliedGachaReveal = false;
	EEmployeeEquipmentSlot PreviewEquipmentSlot = EEmployeeEquipmentSlot::Head;
	EEquipmentAppearanceMode PreviewEquipmentMode = EEquipmentAppearanceMode::UseEquipment;
	FName PreviewAppearanceId;
	FString AppliedEquipmentKey;
	bool bBackAppearanceVisible = false;
	FString GetEquipmentAppearanceKey() const;
	void ApplyEquipmentAppearance();
	void UpdateBackVisibility();
	void HandleEquipmentChanged();

	EWorkerFaceExpression LastFaceExpression = EWorkerFaceExpression::Max;   // Max = 미적용(첫 프레임 강제 반영)
	EWorkerFaceExpression DebugFaceOverride = EWorkerFaceExpression::Max;
	EWorkerFaceExpression FaceReactionExpression = EWorkerFaceExpression::Max;
	double FaceReactionEndTime = 0.0;

	// 깜빡임 — 현재 표정 행의 눈 칸을 기억해 감은 눈(칸 8)에서 되돌린다.
	int32 FaceEyesCell = 0;
	bool bFaceAllowsBlink = false;
	bool bFaceBlinking = false;
	double NextFaceBlinkTime = -1.0;   // 음수 = 아직 예약 전
	double FaceBlinkEndTime = 0.0;
	FRandomStream FaceBlinkStream;
	bool bFaceSheetReady = false;

	// 디버그 고정 > 반응 > 행동 상태 순.
	EWorkerFaceExpression GetEffectiveFaceExpression() const;

	// 얼굴 판 메시·MID 를 걸고 직원 시드 개인 차이를 넣는다. 에셋이 없으면 슬롯을 숨긴다.
	void ApplyFaceSheet(int32 InEmployeeID);
	static bool ResolveFaceStyle(UGameInstance* Instance, int32 InEmployeeID, FFunnyFaceStyleTable& OutStyle);
	UMaterialInterface* GetFaceSheetMaterial();
	UMaterialInstanceDynamic* GetFaceMID() const;
	void TickFaceBlink();

	// 표정 행 조회 — 런타임=TableManagerSubsystem 캐시, 에디터 프리뷰(게임인스턴스 없음)=DT 직접 로드.
	const FFunnyExpressionTable* FindFaceExpressionRow(EWorkerFaceExpression Expression) const;

	// 코스메틱 6종 소켓-상대 트랜스폼을 BP 튜닝값(*Xform)으로 적용. ApplyWorkerCosmetics / OnConstruction 에서 호출.
	void FitAllCosmetics();

	// 코스메틱 행 조회 — 런타임=TableManagerSubsystem 캐시, 에디터(게임인스턴스 없음)=DT_WorkerCosmetic 직접 로드 폴백.
	bool ResolveCosmeticRow(EEmployeeDepartment Department, EEmployeeRank Rank, EGachaTier Tier,
		FFunnyWorkerCosmeticTable& OutRow) const;

	// 슬롯 하나를 DT 풀에서 뽑아 메시+틴트를 적용한다. 풀이 비면 컴포넌트를 숨긴다.
	void ApplySlot(EFunnyPartSlot PartSlot, int32 InEmployeeID, EEmployeeGender InGender,
		const FLinearColor& Tint, float Metal, float Emissive);
};
