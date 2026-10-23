#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Enum/FaceExpression.h"
#include "EmployeeReactionComponent.generated.h"

class AOfficeworker;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

// 졸음 표시 단계 — 꾸벅(Z 글자만) / 늘어짐(콧방울 + Z 글자).
enum class EEmployeeDozeVisual : uint8 { None, Nodding, Sleeping };

// 빌보드 효과 칸의 종류. None = 빈 칸.
enum class EEmployeeReactionSprite : uint8 { None, Z, Drop, Steam, Dust };

struct FEmployeeReactionSpriteState
{
	EEmployeeReactionSprite Kind = EEmployeeReactionSprite::None;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	float Life = 1.f;
	float Phase = 0.f;
	bool bStill = false;
};

struct FEmployeeReactionHop
{
	double StartTime = 0.0;
	float Duration = 0.2f;
	float Height = 0.f;
};

// 끌어서 데려오기 단계. Reduced = 모션 줄이기 — 들지 않고 손을 떼면 잡기만 한다.
enum class EEmployeeCarryPhase : uint8 { None, Held, Falling, Reduced };

// 복귀 달리기 모양 — 탭으로 잡힘 / 끌어서 놓았는데 자리에서 멂(0.6초 달리기) / 자리 가까이(0.22초에 미끄러져 앉기).
enum class EEmployeeDashStyle : uint8 { Caught, DropRun, DropSnap };

// 끌어서 놓아 잡혔다(잡히기 직전 슬랙 단계) — 캐치 링이 탭 잡기와 같은 알림(소리·힌트)을 잇는다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEmployeeCarryCaught, EFatigueSlackPhase);

/**
 * 직원 몸 반응 연출 — 깨우기·자리 이탈 잡기 복귀·일하는 직원 찌르기·크리티컬·분야 달성·졸음 표시·끌어서 데려오기.
 * 바디 메시의 상대 변환(찌그러짐·점프·돌리기·기울기)과 얼굴 반응, 작은 빌보드 효과만 만진다.
 * 게임플레이(상태·점수·좌석·위치)는 바꾸지 않는다 — 끌기만 예외로 들린 동안 액터를 옮기고, 착지 때 기존 잡기(TryManualCatch)를 부른다.
 * 할 일이 없으면 틱이 꺼진다.
 */
UCLASS(ClassGroup = (Custom))
class COMPANYGROWTHRENEWAL_API UEmployeeReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEmployeeReactionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 탭으로 깨움 — 놀람·튀어 오름 → 허둥 도리도리·땀 → 불끈·김 + 타자 빨리. bPopBubble = 콧방울을 터뜨린다.
	void PlayWakeReaction(bool bPopBubble);

	// 자리 이탈 중 잡힘 — 「!」 + 보이는 몸만 0.6초 달려 자리로 온다(좌석·점수 복귀는 기존 경로 그대로 즉시).
	// bDropped = 끌어서 놓아 잡힘 — 착지가 얼굴·눌림을 이미 걸었으니 놀람·「!」·점프 없이 삐친 채 돌아온다.
	void PlayBoltReturn(const FVector& FromLocation, bool bDropped = false);

	// 끌기 시작 문턱(UI 단위, 2560×1440 캔버스 기준) — 이보다 덜 움직이고 떼면 지금처럼 탭 잡기다.
	static constexpr float CarryDragThresholdUnits = 10.f;

	// 끌어서 데려오기 — 개발 중 폭주(배회) 직원만, 패널이 열린(UI 모드) 동안은 안 된다.
	bool CanBeginCarry() const;

	// 들어 올려 손가락(포인터)을 따라가게 한다. 모션 줄이기면 들지 않고 놓을 때 잡기만 한다.
	// bFollowPointer=false 는 검증 명령의 대본 이동 — 위치를 코드가 직접 정한다.
	bool BeginCarry(bool bFollowPointer = true);

	// 손을 뗐다 — 떨어져 착지하면 잡기로 처리한다.
	void ReleaseCarry();

	bool IsCarryActive() const { return CarryPhase != EEmployeeCarryPhase::None; }

	// 몸이 들렸거나 떨어지는 중 — 캐치 링은 그리지 않는다.
	bool IsCarryLifted() const { return CarryPhase == EEmployeeCarryPhase::Held || CarryPhase == EEmployeeCarryPhase::Falling; }

	FOnEmployeeCarryCaught OnCarryCaught;

	// 개발 중 일하는 직원 찌르기 — 윙크 + 살짝 점프 + 카메라 쪽 의자 돌리기. 일하는 중이 아니면 false.
	bool PlayWorkPoke(bool bIgnoreWorkState = false);

	void PlayCritReaction();
	void PlayMilestoneReaction();

	// 검증용 — 슬랙 단계 없이 늘어짐 표시(콧방울 + Z)만 Seconds 동안 띄운다.
	void PreviewDoze(float Seconds);

	// 검증 콘솔 명령 진입점. wake|bolt|poke|crit|mile|doze|drag (drag 의 Option "far" = 자리에서 먼 곳에 놓기)
	bool PlayDebugReaction(const FString& Kind, const FString& Option = FString());

	bool IsDozeVisualActive() const { return DozeVisual != EEmployeeDozeVisual::None; }

	// 분야 달성 — 그 분야가 주 직능인 일하는 직원에게 반응을 건다. 반응한 직원 수를 돌려준다.
	static int32 NotifyDisciplineMilestone(UWorld* World, int32 DisciplineSlot);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleSlackPhaseChanged(EFatigueSlackPhase OldPhase, EFatigueSlackPhase NewPhase, bool bManualCatch, const FVector& PreviousLocation);
	void HandleCriticalContribution();

	AOfficeworker* GetWorker() const;
	USkeletalMeshComponent* GetBody() const;
	bool IsMotionReduced() const;
	bool IsBodyActBusy() const;
	void StartTicking();
	bool HasPendingWork(double Now) const;

	void PlayFace(EWorkerFaceExpression Expression, float Duration);
	void ShowExclamation(float Seconds);
	void AddHop(double StartTime, float Duration, float Height);
	void KickSquash(float Value);

	// 바디 메시 기준 변환 — BP 값을 처음 반응 때 한 번 읽고, 쉬면 그 값으로 정확히 되돌린다.
	void CaptureBaseTransforms();
	// Swing = 캡슐 공간 회전(축 = 캡슐 중심) — 들린 몸의 흔들림.
	void ApplyBodyPose(const FVector& Offset, float YawDeg, float LeanDeg, float Stretch, const FVector& OverheadOffset, const FQuat& Swing = FQuat::Identity);
	void RestoreBodyPose(bool bResetAnimRate);
	void SetRunOverride(float RunSpeed);

	void EvaluateDash(float Alpha, FVector& OutLocation, FVector& OutDirection) const;
	void FinishDash(bool bArrived);

	// 끌어서 데려오기
	void UpdateCarry(float DeltaTime, double Now, FVector& InOutBodyOffset, float& InOutYawDeg, float& OutStretch, float& OutRunSpeed);
	FQuat UpdateCarrySwing(float DeltaTime);
	bool ResolveCarryPointerTarget(float PlaneZ, FVector& OutTarget);
	FVector ClampCarryTarget(const FVector& Target) const;
	void LandCarry();
	void CatchFromCarry(bool bDropped);
	void AbortCarry();
	void PauseWorkerForCarry();
	void ResumeWorkerAfterCarry();
	float GetCapsuleHalfHeight() const;
	bool StartDebugDrag(bool bFar);
	void UpdateDebugDrag(double Now);

	void SetDozeVisual(EEmployeeDozeVisual NewVisual, bool bPreview = false);
	void UpdateDoze(double Now, bool bReduced);
	FVector BubbleWorldScale(float Radius) const;
	bool ShowSnotBubble();
	void HideSnotBubble();
	bool IsSnotBubbleShown() const;
	void StartBubblePop(bool bWithDrops);
	void UpdateBubblePop(double Now);
	bool ResolveHeadReference();

	bool LoadFxAssets();
	int32 AcquireSprite();
	void SpawnSprite(EEmployeeReactionSprite Kind, const FVector& Location, const FVector& Velocity, float Life, bool bStill);
	void SpawnDrops(const FVector& Origin, int32 Count, float Side);
	void SpawnSteamPuffs(bool bStill);
	void SpawnZ(bool bStill);
	void UpdateSprites(float DeltaTime, double Now);
	bool HasActiveSprites() const;
	void SpawnDust(const FVector& FloorLocation, int32 Count, float Speed);

	FVector GetHeadLocation() const;
	float GetFxScale() const;
	void GetCameraAxes(FVector& OutRight, FVector& OutForwardFlat) const;
	FVector GetCameraForward() const;

	// 기준 변환
	bool bBaseCaptured = false;
	bool bPoseApplied = false;
	FVector BaseBodyLocation = FVector::ZeroVector;
	FQuat BaseBodyRotation = FQuat::Identity;
	FVector BaseBodyScale = FVector::OneVector;
	float BaseAnimRateScale = 1.f;
	FVector BaseOverheadLocation = FVector::ZeroVector;
	bool bRunOverrideApplied = false;

	// 몸 — 부피 보존 스프링(1 = 원래 키)
	float Squash = 1.f;
	float SquashVelocity = 0.f;
	TArray<FEmployeeReactionHop> Hops;

	double WakeStartTime = -1.0;
	bool bWakePanicShown = false;
	bool bWakeSweatShown = false;
	double TypingBoostEndTime = -1.0;

	double DashStartTime = -1.0;
	float DashDuration = 0.f;
	float DashLength = 0.f;
	float DashRunSpeed = 0.f;
	bool bDashMoving = false;
	bool bDashPanicShown = false;
	double NextDustTime = 0.0;
	FVector DashFrom = FVector::ZeroVector;
	FVector DashVia = FVector::ZeroVector;
	FVector DashTo = FVector::ZeroVector;
	EEmployeeDashStyle DashStyle = EEmployeeDashStyle::Caught;

	// 끌어서 데려오기 — 들린 동안 배회 AI·이동·행동 틱을 멈춰 두고 착지 때 되돌린다
	EEmployeeCarryPhase CarryPhase = EEmployeeCarryPhase::None;
	bool bCarryFollowPointer = false;
	bool bCarryPaused = false;
	bool bCarryMoveTickWasEnabled = true;
	bool bCarryBehaviorTickWasEnabled = true;
	bool bCarryDropped = false;
	float CarryFloorZ = 0.f;
	float CarryFallSpeed = 0.f;
	float CarryDropHeight = 0.f;
	float CarryTilt = 0.f;
	float CarryTiltVelocity = 0.f;
	FVector CarryTarget = FVector::ZeroVector;
	FVector CarryLastLocation = FVector::ZeroVector;
	FVector CarryVelocity = FVector::ZeroVector;
	double NextCarrySweatTime = 0.0;
	double NextCarryFaceTime = 0.0;
	double AfterDizzyTime = -1.0;

	// 검증 명령 drag 대본(시안 DIRECTOR.dragHome)
	double DebugDragArmTime = -1.0;
	double DebugDragStartTime = -1.0;
	bool bDebugDragFar = false;
	FVector DebugDragFrom = FVector::ZeroVector;
	FVector DebugDragMid = FVector::ZeroVector;
	FVector DebugDragTo = FVector::ZeroVector;

	double PokeStartTime = -1.0;
	float PokeYawTarget = 0.f;

	EEmployeeDozeVisual DozeVisual = EEmployeeDozeVisual::None;
	bool bDozePreview = false;
	double DozePreviewEndTime = 0.0;
	double NextZTime = 0.0;
	double BubblePopStartTime = -1.0;
	bool bHeadReferenceResolved = false;
	FTransform HeadReferencePose = FTransform::Identity;

	bool bFxAssetsTried = false;
	TArray<FEmployeeReactionSpriteState> SpriteStates;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialBillboardComponent>> SpritePool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SpriteMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SpriteMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SpriteTextures;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SnotBubble;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SnotBubbleMID;
};
