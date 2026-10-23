#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "WorkstationDevFxComponent.generated.h"

class AOfficeworker;
class AWorkstationActorBase;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UEmployeeBehaviorComponent;
enum class EFatigueSlackPhase : uint8;

/**
 * 책상 개발 연출 에셋 묶음. 코드가 이 DA 하나만 문자열 경로로 열고, 나머지는 DA 의 하드 참조로 쿠킹된다.
 * 작업물 조각 글리프는 TileMaterial 의 아틀라스 칸 = EProductionDiscipline 순서(4×2 칸, 0~5 사용).
 */
UCLASS(BlueprintType)
class COMPANYGROWTHRENEWAL_API UWorkstationDevFxConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// 1m 정육면체(/Engine/BasicShapes/Cube) — 작업물 조각·키캡
	UPROPERTY(EditAnywhere, Category = "DevFx")
	TObjectPtr<UStaticMesh> TileMesh = nullptr;

	// 1m 평면(/Engine/BasicShapes/Plane) — 책상 빛 웅덩이·메모지
	UPROPERTY(EditAnywhere, Category = "DevFx")
	TObjectPtr<UStaticMesh> PlaneMesh = nullptr;

	// Unlit 불투명. TileColor(vector) GoldMix/GlyphCell/GlyphOn(scalar)
	UPROPERTY(EditAnywhere, Category = "DevFx")
	TObjectPtr<UMaterialInterface> TileMaterial = nullptr;

	// Unlit 가산. GlowColor(vector) Opacity(scalar) — 빛 웅덩이(평면)와 모니터 빛(빌보드)이 같이 쓴다
	UPROPERTY(EditAnywhere, Category = "DevFx")
	TObjectPtr<UMaterialInterface> GlowMaterial = nullptr;

	// Unlit 마스크. NoteColor(vector)
	UPROPERTY(EditAnywhere, Category = "DevFx")
	TObjectPtr<UMaterialInterface> NoteMaterial = nullptr;
};

// 작업물 조각 1장(날아가는 중이거나 더미에 쌓인 것)
USTRUCT()
struct FWorkstationDevFxTile
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;

	// 0 쉼 · 1 비행 · 2 더미 · 3 떠남(접힘/환호)
	uint8 Phase = 0;
	float Age = 0.f;
	float BaseScale = 1.f;
	bool bLanded = false;
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	FVector LeaveTo = FVector::ZeroVector;
	float LeaveDuration = 0.2f;
	float LeaveEndScale = 0.f;
};

// 키캡 튐 1개
USTRUCT()
struct FWorkstationDevFxKeycap
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;

	bool bAlive = false;
	float Age = 0.f;
	float Spin = 0.f;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

// 좌석 1개의 연출 상태. 위치는 전부 월드 좌표(개발 중엔 책상이 움직이지 않는다).
USTRUCT()
struct FWorkstationDevFxLane
{
	GENERATED_BODY()

	int32 SeatIndex = 0;
	int32 EmployeeID = INDEX_NONE;
	TWeakObjectPtr<AOfficeworker> Worker;
	TWeakObjectPtr<UEmployeeBehaviorComponent> BoundBehavior;
	FDelegateHandle SlackHandle;
	FDelegateHandle CritHandle;

	// 좌석 기준 축(Forward = 좌석→모니터)과 앵커
	float Scale = 1.f;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector ScreenTop = FVector::ZeroVector;
	FVector TileStart = FVector::ZeroVector;
	FVector GlowLocation = FVector::ZeroVector;
	FVector PoolCenter = FVector::ZeroVector;
	FVector StackBase = FVector::ZeroVector;
	FVector KeyboardTop = FVector::ZeroVector;
	FVector NoteAnchor = FVector::ZeroVector;
	float NoteSize = 24.f;
	float PoolWidth = 150.f;
	float PoolDepth = 95.f;

	int32 StepNumber = 1;
	int32 GlyphCell = 0;
	FLinearColor Color = FLinearColor::White;

	// 모니터 켜짐 · 몰입 단계
	double OnTime = -1.0;
	bool bOn = false;
	bool bPlayTickOnTurnOn = false;
	double FlashTime = -9.0;
	float PoolOpacity = 0.f;
	int32 Streak = 0;
	int32 Tier = 0;
	double FocusFaceEndTime = 0.0;
	double NextKeycapTime = 0.0;
	bool bDebugFlow = false;

	// 타자 재생 속도(몰입 2단계). 반응 컴포넌트가 틱하는 동안은 손대지 않는다.
	bool bRateApplied = false;
	bool bTrueRateKnown = false;
	float TrueRate = 1.f;

	// 더미
	TArray<int32> StackOrder;
	double BumpTime = -9.0;

	// 메모지 — 0 없음 · 1 접힘/비행 · 2 붙음
	uint8 NotePhase = 0;
	float NoteAge = 0.f;
	FVector NoteFrom = FVector::ZeroVector;
	FVector NoteControl = FVector::ZeroVector;

	// 의자(완료 환호)
	TWeakObjectPtr<UStaticMeshComponent> Chair;
	bool bChairCaptured = false;
	FVector ChairBaseRelLocation = FVector::ZeroVector;
	FRotator ChairBaseRelRotation = FRotator::ZeroRotator;
	FVector ChairBaseWorldLocation = FVector::ZeroVector;
	FQuat ChairBaseWorldRotation = FQuat::Identity;
	FVector ChairPivot = FVector::ZeroVector;
	float ChairPush = 0.f;
	double SpinStartTime = -1.0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Pool = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PoolMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialBillboardComponent> Glow = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Note = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NoteMID = nullptr;

	UPROPERTY(Transient)
	TArray<FWorkstationDevFxTile> Tiles;

	UPROPERTY(Transient)
	TArray<FWorkstationDevFxKeycap> Keycaps;
};

/**
 * 책상 개발 연출(설계 2026-10-17 4·5단계) — 모니터 켜짐 물결, 책상 빛 웅덩이·모니터 빛(몰입 단계별 밝기),
 * 기여마다 모니터에서 더미로 날아가는 작업물 조각, 분야 달성 메모지, 몰입 키캡, 완료 환호 때 의자 밀림·회전.
 * 점수·타이머·도착 흐름은 바꾸지 않는다. 개발 중에만 틱하고, 끝나면 만든 컴포넌트를 전부 지운다.
 */
UCLASS(ClassGroup = (Custom))
class COMPANYGROWTHRENEWAL_API UWorkstationDevFxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorkstationDevFxComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 책상 판단(RecalcBoxExtent)에서 빼야 하는 연출 메시 표식
	static const FName FxComponentTag;

	// 개발 시작 — 모니터가 화면 왼쪽부터 0.09초 간격으로 켜지고 첫 화면에서 「톡」 한 번.
	static void NotifyDevelopmentStart(UWorld* InWorld);

	// 세이브 복원·컨티뉴처럼 시작 연출 없이 개발에 들어온 경로 — 꺼진 책상만 바로 켠다.
	static void EnsureDevelopmentActive(UWorld* InWorld);

	// 개발 점수 구슬 요청 — 기여한 직원 책상을 찾아 구슬 출발점을 모니터 위로 옮긴다. bNewContribution 이면 조각을 날리고 몰입을 센다.
	static bool HandleScoreOrb(UWorld* InWorld, const FVector& HeadWorldPos, int32 StepNumber, bool bCritical, bool bNewContribution, FVector& OutOrbStart);

	// 분야 달성 — 그 분야가 주 직능인 직원 책상의 조각 더미가 메모지 한 장으로 접혀 모니터 뒷면에 붙는다.
	static int32 NotifyDisciplineMilestone(UWorld* InWorld, int32 DisciplineSlot, int32 StepNumber);

	// B 이상 기립 환호 — 의자 밀림·회전, 두 번 폴짝·웃음, 메모지 팔랑, 조각 정리.
	static void NotifyLaunchCheer(UWorld* InWorld);

	// 개발 정리(결과 창 닫힘·새 개발) — 조각·메모지·빛을 거두고 의자·타자 속도를 되돌린다.
	static void NotifyDevelopmentReset(UWorld* InWorld);

	// 검증 콘솔 명령 진입점. start|tile|crit|mile|flow|climax|reset, WorkerIndex = 직원ID 순번(음수 = 전원)
	static bool PlayDebug(UWorld* InWorld, const FString& Kind, int32 WorkerIndex);

	// 좌석 연출을 새로 짓고 모니터 켜짐을 예약한다. 연출을 맡은 좌석 수를 돌려준다.
	int32 BeginDevelopment(float WaveDelay, bool bPlayTick, bool bImmediate);
	void ResetDevelopment();
	bool IsDevelopmentActive() const { return bActive; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AWorkstationActorBase* GetDesk() const;
	bool LoadConfig();
	void StartTicking();

	void BuildLanes();
	bool ResolveLaneAnchors(FWorkstationDevFxLane& Lane) const;
	void ResolveLaneDiscipline(FWorkstationDevFxLane& Lane) const;
	void BindLaneWorker(int32 LaneIndex);
	void UnbindLaneWorker(FWorkstationDevFxLane& Lane);
	int32 FindLaneForWorker(const AOfficeworker* InWorker) const;

	void HandleSlackPhaseChanged(EFatigueSlackPhase OldPhase, EFatigueSlackPhase NewPhase, bool bManualCatch, const FVector& PreviousLocation, int32 LaneIndex);
	void HandleCriticalContribution(int32 LaneIndex);

	bool HandleContribution(const AOfficeworker& InWorker, int32 InStepNumber, bool bCritical, bool bNewContribution, FVector& OutOrbStart);
	void PlayMilestoneNote(int32 LaneIndex, int32 InStepNumber);
	void PlayCheer(float SpinDelay);
	void SetDebugFlow(int32 LaneIndex);

	// 연출 메시
	UStaticMeshComponent* CreateFxMesh(UStaticMesh* Mesh, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutMID);
	void EnsureLaneVisuals(FWorkstationDevFxLane& Lane);
	int32 AcquireTile(FWorkstationDevFxLane& Lane);
	void ReleaseTile(FWorkstationDevFxTile& Tile);
	void SpawnTile(FWorkstationDevFxLane& Lane, int32 InStepNumber, int32 InGlyphCell, bool bCritical, bool bReduced);
	void LayoutStack(FWorkstationDevFxLane& Lane, float Bump);
	void SpawnKeycap(FWorkstationDevFxLane& Lane);
	void DestroyLaneVisuals(FWorkstationDevFxLane& Lane);

	// 좌석별 틱 — 아직 움직이는 게 있으면 true
	bool TickLane(FWorkstationDevFxLane& Lane, int32 LaneIndex, double Now, float Dt, bool bReduced, bool bRunning);
	void TickTiles(FWorkstationDevFxLane& Lane, double Now, float Dt);
	void TickNote(FWorkstationDevFxLane& Lane, int32 LaneIndex, double Now, float Dt);
	bool TickChair(FWorkstationDevFxLane& Lane, double Now, float Dt, bool bReduced);
	bool TickKeycaps(FWorkstationDevFxLane& Lane, float Dt);
	void ApplyFlowRate(FWorkstationDevFxLane& Lane, bool bWantFlowRate);
	void RestoreFlowRate(FWorkstationDevFxLane& Lane, bool bForce = false);
	void CaptureChair(FWorkstationDevFxLane& Lane);
	void RestoreChair(FWorkstationDevFxLane& Lane);

	FQuat GetLaneQuat(const FWorkstationDevFxLane& Lane) const;

	bool bActive = false;
	bool bDebugRunning = false;
	bool bPendingRestore = false;
	double PendingRestoreDeadline = 0.0;
	double CheerStartTime = -1.0;
	int32 DeskOrder = 0;

	UPROPERTY(Transient)
	TArray<FWorkstationDevFxLane> Lanes;

	UPROPERTY(Transient)
	TObjectPtr<UWorkstationDevFxConfig> FxConfig = nullptr;

	bool bConfigTried = false;
};
