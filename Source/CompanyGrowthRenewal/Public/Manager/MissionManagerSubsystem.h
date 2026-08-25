#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Table/MissionTable.h"
#include "Manager/EmployeeManager.h"
// FGuideExplainCopy 가 EGuideTooltipDir 를 값으로 들기 때문에 전방선언으로는 부족하다
#include "UI/HUD/GuideTooltipPlacement.h"
#include "UI/Element/Common/GestureHintTypes.h"
#include "MissionManagerSubsystem.generated.h"

class UBuildOpenWidget;
class UFactoryPanelWidget;
class UCommonActivatableWidget;
class UMissionTrackerWidget;
class UMissionGuideOverlayWidget;
class UBuildingManagePanelWidget;
class ABuildingBaseActor;
class UOfficeMainWidget;
class UOfficeLayerWidget;
class UOfficeRecruitmentPanelWidget;
class UEmployeeGachaPresentationWidget;
class UWorkstationInfoWidget;
class URecruitmentManagerSubsystem;
class UPitchBoardWidget;
class ULaunchConfirmWidget;
class UBuildingTraitGachaPanelWidget;
class UEmployeeWindowWidget;
class UEnhanceStarforceModalWidget;
class UCityAcquisitionManager;
struct FGachaResultData;
struct FBuildingTraitGachaResult;
class UWidget;
enum class EFactoryUpgradeType : uint8;

// 오피스 배치 종류 (M4 완료 판정 / 벽장식 = 미션판 G7 신호)
enum class EOfficePlacementKind : uint8 { Desk, WallDecoration };

// 새 미션 활성화 (트래커 표시 갱신)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionActivated, const FMissionTable&);
// 미션 완료 — 보상 지급 직후 (트래커 완료 연출)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMissionCompleted, FName, const FMissionTable&);
// 가이드 페이즈 변경 (멘토 라인/하이라이트 타겟 갱신)
DECLARE_MULTICAST_DELEGATE(FOnGuidePhaseChanged);
// 누적형 조건 진행도 변경 (트래커 "현재/목표" 갱신)
DECLARE_MULTICAST_DELEGATE(FOnMissionProgressChanged);
// 조건 달성 → 클레임 대기 진입 (트래커 "완료! 탭" 상태 전환 — 자동 진행 없음)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionReadyToClaim, const FMissionTable&);
// 미션 조건 관련 게임플레이 신호 — GoalBoardSubsystem 이 구독해 판정 (스펙 2026-08-02 §5.1)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnConditionSignal, EMissionConditionType);

/**
 * 미션판 안내의 "지금 할 한 걸음" (파생값 — 저장하지 않는다).
 * Index = DT_Goal.StepLines 인덱스. **0 번은 항상 장소 이동**(사무실 들어가기 / 도시로 나가기).
 * Anchor(링)와 Spot(월드 스포트라이트)은 배타적 — 둘 다 없으면 문구만 안내한다.
 */
struct FGoalGuideStep
{
	int32 Index = INDEX_NONE;
	class UWidget* Anchor = nullptr;
	class AActor* Spot = nullptr;
};

/** 설명 페이즈 툴팁 1장의 내용 + 꼬리 방향. */
struct FGuideExplainCopy
{
	FText Eyebrow;
	FText Body;
	EGuideTooltipDir Dir = EGuideTooltipDir::Up;
};

/**
 * 미션 체인 관리자 (GDD_PROGRESSION Phase 4 미션 시스템 v1).
 * 오프닝(CORE_REDESIGN ★B안)을 미션으로 표현: 깨면 보상 → 다음 미션. DT_Mission 단일 진실.
 * 트래커(UIE_MissionTracker)는 항상 현재 미션 1개만 노출(목표 깔때기 원칙),
 * 가이드 오버레이(UI_MissionGuideOverlay)가 현 페이즈의 타겟 위젯에 펄스 링을 그린다(소프트 유도 — 입력 차단 없음).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMissionManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// MainMap StartPlay(정상 모드)에서 호출 — 진행 중 미션이 있으면 트래커/가이드 구성
	void TryStartMissionChain();

	// 개발용 — 임의 미션으로 즉시 점프(인세션). 미션 활성 + 선행상태 시드 + 가이드/트래커 재구축.
	// 콘솔 치트 SkipToMission 의 백엔드. (Shipping 영향 없음 — 치트 매니저가 Shipping 비활성)
	void DevJumpToMission(FName MissionID);

	// ===== 진행 신호 (위젯/시스템이 호출) =====
	void RegisterBuildOpenWidget(UBuildOpenWidget* Widget);
	void UnregisterBuildOpenWidget(UBuildOpenWidget* Widget);
	void NotifyBuildModalOpened(UCommonActivatableWidget* Modal);
	void NotifyBuildingPlaced(ABuildingBaseActor* Building);
	// 건물 관리 패널 열림 (M3 EnterOffice: 빌딩 클릭 → 패널 → [입장] 페이즈 전이 + [입장] 버튼 하이라이트 타겟 등록)
	void NotifyManagePanelOpened(UBuildingManagePanelWidget* Panel);

	// ===== M5 RecruitEmployees =====
	// OfficeMain 등록 — p0 [채용] 버튼 하이라이트 타겟
	void RegisterOfficeMainWidget(UOfficeMainWidget* Widget);
	void UnregisterOfficeMainWidget(UOfficeMainWidget* Widget);
	// 채용 패널 열림 (OfficeMain [채용] → 패널) — p0→p1 전이 + [뽑기] 버튼 하이라이트용 패널 등록
	void NotifyRecruitPanelOpened(UOfficeRecruitmentPanelWidget* Panel);
	// 가챠 연출 위젯 열림 — [확인] 버튼 하이라이트용 등록 (p2)
	void RegisterGachaPresentation(UEmployeeGachaPresentationWidget* Widget);
	void UnregisterGachaPresentation(UEmployeeGachaPresentationWidget* Widget);
	// 채용 확정(ConfirmGachaHire 성공) — M5 PressConfirm→PressCloseRecruit
	void NotifyHireConfirmed(const FGachaResultData& Result);
	// 채용 패널 닫힘 — M5 직원 수 도달 조건 재평가
	void NotifyRecruitPanelClosed();

	// OfficeLayer 등록 — M5 책상 버블/액터 하이라이트 타겟 조회용
	void RegisterOfficeLayer(class UOfficeLayerWidget* Widget);
	void UnregisterOfficeLayer(class UOfficeLayerWidget* Widget);

	// ===== M5 좌석 배정 =====
	// 책상 패널 열림 (WorkstationInfo) — p0→p1 전이용 패널 등록
	void NotifyWorkstationPanelOpened(UWorkstationInfoWidget* Panel);
	// 좌석 배정(자동 착석 or 수동) — M5 조건 재평가 보조 신호
	void NotifyEmployeeSeated();
	// 공장 패널 등록 (강화 넛지 타겟 조회용 — BuildOpen 패턴)
	void RegisterFactoryPanel(UFactoryPanelWidget* Panel);
	void UnregisterFactoryPanel(UFactoryPanelWidget* Panel);

	// 배치 바(건물/책상 공용) — M2/M4 확정 페이즈의 [배치] 링 타겟
	void RegisterBuildPlacementPanel(class UBuildPlacementPanelWidget* Panel);
	void UnregisterBuildPlacementPanel(class UBuildPlacementPanelWidget* Panel);

	// ===== M6 기획 보드/출시 =====
	void RegisterPitchBoard(class UPitchBoardWidget* Widget);
	void UnregisterPitchBoard(class UPitchBoardWidget* Widget);
	void RegisterLaunchConfirm(class ULaunchConfirmWidget* Widget);
	void UnregisterLaunchConfirm(class ULaunchConfirmWidget* Widget);

	// ===== M4 PlaceDesks / 미션판 G7 PlaceFirstPainting =====
	void NotifyPlacementModeOpened(EOfficePlacementKind Kind);      // M4 OpenPlacement→Confirm
	void NotifyOfficePlacementCompleted(EOfficePlacementKind Kind, int32 Count = 1); // M4 완료 / 벽장식은 G7 신호

	// ===== 미션판 G9 EnhanceStat =====
	void NotifyEmployeeWindowOpened(class UEmployeeWindowWidget* Window);   // 직원창 열림 = 등록

	// ===== M6 LaunchFirstInHouseProject =====
	void NotifyInHouseProjectSelected();      // M6 PickInHouse→WatchStrip
	void NotifyProjectLaunched();             // M6 [출시] 즉시 완료(운영은 백그라운드) + 상시 수집 안내
	void NotifyProjectOperationCompleted();   // (구 완료 훅 — 현재 출시 즉시 완료라 M6엔 미사용)

	// ===== M7 CollectFirstRevenue =====
	// 수익 수집 완료(ProjectOperationManager->OnRevenueCollected, dynamic) — M7 완료 판정
	UFUNCTION()
	void HandleRevenueCollected(int64 Amount);

	// ===== 미션판 G10 EquipTrait =====
	// 아래 특성 훅들은 페이즈 전진이 사라진 뒤에도 위젯이 호출하는 진입점이라 유지한다 (신호원/등록용)
	void NotifyTraitEquipped();   // 특성 장착 신호 (판정은 GoalBoard)
	void RegisterTraitGachaPanel(class UBuildingTraitGachaPanelWidget* Widget);
	void UnregisterTraitGachaPanel(class UBuildingTraitGachaPanelWidget* Widget);
	void RegisterGachaReveal(class UGachaRevealPresentationWidget* Widget);
	void UnregisterGachaReveal(class UGachaRevealPresentationWidget* Widget);
	void NotifyGachaRevealConfirmed();
	void NotifyTraitTabOpened();

	// ===== 미션판 G8 EquipFirstSkin =====
	void NotifySkinEquipped();    // 스킨 장착 신호 (판정은 GoalBoard)

	// ===== 미션판 G1 RaiseBuildingFloor =====
	void NotifyBuildingFloorUpgraded(); // 빌드업 1회 강화 신호 (판정은 GoalBoard)
	// ===== 미션판 G2 HQLevel =====
	// SaveLoadManager::TryLevelUpHQ 성공 시 호출 — 본사 레벨 도달 신호 (판정은 GoalBoard)
	void NotifyHQLevelChanged();
	// ===== 강화(스타포스) 모달 등록 =====
	void RegisterStarforceModal(UEnhanceStarforceModalWidget* InModal);
	void UnregisterStarforceModal(UEnhanceStarforceModalWidget* InModal);

	// ===== 미션판 G4 AcquireCompany =====
	// CityAcquisitionManager::Acquire 성공 직후 호출 — 인수 신호 (판정은 GoalBoard)
	void NotifyCompanyAcquired(int32 Key);

	// ===== 미션판 G11 AcquireFirstPlot =====
	// ACityPlotActor 부지 결제 성공 직후 호출 — 부지 인수 신호 (판정은 GoalBoard)
	void NotifyPlotAcquired();

	// 운영시간 튜토리얼 단축 판정용
	EMissionConditionType GetActiveConditionType() const
	{
		return ActiveMissionID.IsNone() ? EMissionConditionType::None : ActiveMissionRow.ConditionType;
	}

	// ===== 조회 (트래커/오버레이) =====
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool HasActiveMission() const;

	// M7 피날레 수령까지 끝난 명시적 완료 상태. GoalBoard 언락을 proxy로 쓰지 않는다.
	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsTutorialCompleted() const { return bTutorialCompleted; }

	// 현재 활성 트래커(임베드 or 뷰포트 폴백) — 어느 레이어 소유든 해석. 없으면 임베드 재탐색.
	UMissionTrackerWidget* GetActiveTracker() const;

	UFUNCTION(BlueprintCallable, Category = "Mission")
	FName GetActiveMissionID() const { return ActiveMissionID; }

	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool GetActiveMission(FMissionTable& OutMission) const;

	UFUNCTION(BlueprintCallable, Category = "Mission")
	FText GetCurrentMentorLine() const;

	// 현 가이드 페이즈 진입 후 경과 초 — 오버레이가 소프트 존 지연 힌트 점등 판단에 사용
	double GetPhaseElapsedSeconds() const;

	// 누적형 조건(CollectBricks)의 현재/목표. 진행도 없는 미션이면 false
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool GetMissionProgress(int64& OutCurrent, int64& OutTarget) const;

	// 현 가이드 페이즈에서 펄스 링을 그릴 대상 (nullptr = 링 생략)
	UWidget* GetCurrentHighlightTarget() const;

	// 같은 페이즈에서 딤 구멍만 하나 더 뚫을 대상들 (주 타겟 제외, 링/셰브론 없음).
	// 대부분의 페이즈는 비운다 — "설명은 한 곳" 이 기본이고 여기 담는 건 예외다.
	void GetCurrentHighlightExtraTargets(TArray<UWidget*>& OutExtras) const;

	// 월드 액터 스포트라이트 대상 (CollectBricks '공장 탭' 페이즈 — 배경 딤 + 라운드 컷아웃). nullptr = 스포트라이트 없음
	AActor* GetCurrentSpotlightActor() const;

	// 제스처 힌트 채널 (스펙 2026-08-22 §3) — 손 모양 / 손끝을 둘 액터 / 딤 배율(0=딤 없음). 미션 없음·클레임이면 None/null/1
	EGestureHintKind GetCurrentGesture() const;
	AActor* GetCurrentGestureAnchorActor() const;
	float GetGuideDimScale() const;

	// ===== M7 설명 페이즈 (수입 칩 · 운영 진행 게이지를 순서대로 설명) =====
	// 지금이 설명 페이즈인가 — 타겟이 아직 안 모여도 true. 오버레이가 타임아웃을 셀 근거이자
	// "아무 데나 탭" 을 수령 경로와 가르는 판정.
	bool IsExplainPhaseActive() const;

	/** 설명 페이즈 안에서 지금 몇 번째 설명인가(0-based). 세션 내 상태 ― 세이브하지 않는다. */
	int32 GetExplainStep() const { return ExplainStep; }

	/** 설명 대상 개수. 스텝 상한의 단일 출처. */
	static constexpr int32 ExplainStepCount = 2;

	/** 해당 스텝의 대상 하나만 해결한다. 나머지 스텝이 못 뜨는 상태여도 이 스텝은 뜬다. */
	bool GetExplainTargetForStep(int32 Step, class UWidget*& OutTarget, FGuideExplainCopy& OutCopy) const;

	// 설명 페이즈의 카메라 글라이드가 끝났는가. 카메라를 못 찾으면 true(대기하지 않는다 ― 갇히지 않는 쪽)
	bool IsExplainCameraSettled() const;

	// 설명 화면 탭 — 다음 설명으로, 마지막이면 다음 페이즈로
	void AdvanceExplainPhase(bool bWaitForNextTarget = true);
	// 다음 타겟 준비 전에는 현재 설명을 유지하고, 준비된 프레임에만 스텝을 커밋한다.
	void ResolvePendingExplainAdvance(float TimeoutSeconds);

	// ===== 미션판 안내 (체인 종료 후) =====
	// 추적 미션의 "지금 할 한 걸음". **상태에서 매번 도출한다 — 인덱스를 저장하지 않는다.**
	// 체인처럼 신호로 전진시키면 플레이어 이탈 시 어긋나 후퇴 로직이 필요해진다(2026-08-08 구 M13 딤 사고의 원인).
	// 근거 = docs/superpowers/specs/2026-08-08-goal-guide-next-step-design.md §2
	FGoalGuideStep GetTrackedGoalStep() const;
	// 현재 걸음의 안내 문구 (DT_Goal.StepLines[Index]). 범위 밖이면 빈 FText
	FText GetTrackedGoalStepLine() const;

	// 안내 중인 미션의 링 타겟. 없으면 nullptr(다른 맵이거나 위젯 미배치 — 아무것도 안 그린다)
	UWidget* GetTrackedGoalHighlightTarget() const;
	// 안내 중인 미션의 월드 액터 타겟(위젯 타겟보다 우선). 도시 회사/부지는 대상 조회 API 가 없어 미배선
	AActor* GetTrackedGoalSpotlightActor() const;
	// 안내 중인 미션을 이 맵에서 할 수 없으면 갈 곳("사무실에서"/"도시에서"), 여기서 되면 빈 FText.
	// 멘토 밴드는 활성 미션 전용이라 미션판 추적 중엔 안 뜬다 — 이 공백을 미션판 행의 태그 칩이 대신 말한다
	FText GetTrackedGoalPlaceHint() const;

	// 튜토리얼 입력 하드게이트 — 현재 가이드 타겟만 클릭 허용(나머지 UI 잠금). 가이드 오버레이가 구멍 밖을 차단.
	// false = 자유 조작: 미션 없음 / 클레임 대기 / 드래그-배치 확정 서브페이즈(건물·책상·그림 배치).
	bool IsInputGated() const;

	// 가이드 타겟이 실제로 "누를 수 있는" 상태인지. false 면 IsInputGated 가 게이트를 풀어 소프트락을 막는다.
	// OutReason = null / Collapsed / Hidden / Disabled / ZeroSize (로그용).
	bool IsGuideTargetActionable(UWidget* Target, FString& OutReason) const;

	// 클레임 대기 시 가이드 오버레이가 크게 비출 대상(트래커 카드). nullptr = 풀스크린 딤만.
	UWidget* GetClaimSpotlightTarget() const;

	// 클레임 연출(딤 + 풀스크린 탭 수령)을 지금 띄워도 되는지.
	// 배치 모드(연속 배치 포함)나 프롬프트 모달이 살아 있으면 false — 풀스크린 수령 버튼이 z9000 이라
	// 그 화면으로 향한 탭을 통째로 가로채 "안 눌렀는데 수령됨" 이 된다. 조건 충족(bReadyToClaim)과 연출 제시는 별개다.
	bool IsClaimPresentable() const;

	// 조건 달성 후 클레임(카드 탭) 대기 중인지 — 트래커가 클릭 가능/완료 상태 전환에 사용
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool IsReadyToClaim() const { return bReadyToClaim; }

	// 하드 게이트 미션인지 (DT bHardGate). 소프트 존은 게이트/상시 가이드 대신 지연 힌트(오버레이)로 유도.
	bool IsHardGateMission() const { return HasActiveMission() && ActiveMissionRow.bHardGate; }

	// 이 미션이 '지연 힌트'(무진전 N초 후 점등)를 쓰는지 — 오버레이가 점등을 미룰지 판단한다.
	bool UsesDelayedHint() const;

	// 클레임 대기 미션 수령 (트래커 카드 탭) — 보상 스플래시 + 지급 + 다음 미션 활성화
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ClaimActiveMission();

	FOnMissionActivated OnMissionActivated;
	FOnMissionCompleted OnMissionCompleted;
	FOnGuidePhaseChanged OnGuidePhaseChanged;
	FOnMissionProgressChanged OnMissionProgressChanged;
	FOnMissionReadyToClaim OnMissionReadyToClaim;
	FOnConditionSignal OnConditionSignal;

private:
	void ReleaseExplainPresentation();
	void HandleGameDataLoaded();
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	// dev 무한모드 — 다음 틱에 자원(Rich)+HQ 레벨 적용 (CGDevSettings StartMode==Sandbox 일 때만 호출)
	void ApplyDevSandbox();

	// dev — 점프 타깃 미션의 선행 월드상태 시드 (자원 듬뿍 + 빌딩 존재 체크). next-tick 디퍼.
	void SeedTutorialPrereqs(FName TargetMissionID);

	// 공장 강화 완료 — CollectBricks 강화 넛지 페이즈 통과 신호
	UFUNCTION()
	void HandleFactoryUpgraded(EFactoryUpgradeType UpgradeType);

	// 가챠 뽑기 완료(RecruitmentManager->OnGachaPullCompleted) — M5 p1→p2 전이 (네이티브 멀티캐스트 핸들러)
	void HandleGachaPullCompleted(const FGachaResultData& Result);

	// 특성 가챠 뽑기 완료(BuildingTraitManager->OnTraitGachaCompleted) — 최근 뽑기 ID 보관 (네이티브 멀티캐스트 핸들러)
	void HandleTraitGachaPulled(const struct FBuildingTraitGachaResult& Result);

	// 직원 강화 시도 완료(EmployeeManager->OnEmployeeEnhanced, 네이티브) — 미션판 G9 완료 신호. 결과값 무관(확률형)
	void HandleEmployeeEnhanced(int32 EmployeeID, UEmployeeManager::EEnhanceResult Result);

	// 도시 회사 철거 완료(CityAcquisitionManager->OnCompanyCleared, 네이티브) — 미션판 G5 신호
	void HandleCompanyCleared(int32 Key);
	// CityAcquisitionManager 는 WorldSubsystem 이라 GameInstance 컬렉션의 InitializeDependency 로 못 잡는다.
	// 맵 전환마다 새 인스턴스가 생기므로 TryStartMissionChain 에서 레벨 단위로 재구독한다.
	void BindCityAcquisitionDelegates(UWorld* World);

	void SetActiveMission(FName NewMissionID);
	bool CompleteActiveMission();
	// 현재값으로 재평가 가능한 조건이 이미 충족돼 있으면 완료 정책을 적용한다 (활성화 직후/연쇄 완료용).
	void TryCompleteByResources();
	// M5 도달형 조건은 착석 성공 여부가 아니라 현재 건물 소속 직원 수로만 판정한다.
	void EvaluateRecruitEmployeesCondition();
	void SetGuidePhase(int32 NewPhase);

	// 델리게이트 없는 이탈 경로(모달 닫힘/배치 취소) 감지용 0.25s 폴링
	void PollGuideProgress();
	bool IsPlacementActive() const;
	// 책상 배치 모드 유지 여부 (연속 배치 안내 → 버튼 유도 후퇴 판정용)
	bool IsDeskPlacementActive() const;

	void EnsureMissionWidgets();
	void DestroyMissionWidgets();

	// 미션판 언락/수령으로 미션판 트래커 표시 조건이 바뀌었을 때 재평가
	void HandleGoalBoardChangedForWidgets();

	// 미션판 안내 대상 변경 — 멘토 밴드에 그 미션의 안내문을 띄운다(해제면 내린다).
	// 체인의 UpdateGuideNotification 을 거치지 않는다 — 그건 활성 미션 기준이라 미션판 문구를 즉시 지운다
	void HandleTrackedGoalChanged(FName TrackedID);

	// 활성 맵 레이어(MainMap=InGameLayer / OfficeMap=OfficeLayer)에 디자이너가 배치한 임베드 트래커 탐색.
	// 없으면 nullptr → 매니저가 뷰포트 폴백 생성.
	class UMissionTrackerWidget* FindEmbeddedTracker() const;

	// 스포트라이트 페이즈 동안 멘토 라인을 상단 상주 알림(Notification)으로 표시/해제.
	// 페이즈 전환 + 0.25s 폴링에서 호출 (같은 문구 재호출은 컨테이너가 무시 — 멱등)
	void UpdateGuideNotification();

	// 조건 달성 → 다음 미션이 있는 행은 즉시 진행, 체인 마지막 행만 클레임 대기 진입.
	void SetReadyToClaim();

	// 클레임 보상 스플래시 — 화면 중앙 코드 트리 위젯 (자기 수명 관리)
	void ShowRewardSplash(const FMissionTable& Mission);

	FName ActiveMissionID = NAME_None;
	FMissionTable ActiveMissionRow;

	// WatchStrip 진입 시각(월드 시간). 세이브하지 않는다 — 재시작 시 미션 처음부터라 관찰도 다시 한다.
	double StripWatchEnteredAt = 0.0;

	// 현 가이드 페이즈 진입 시각(월드 초). 소프트 존 지연 힌트 게이팅용 — 세이브 안 함(StripWatchEnteredAt 동형).
	double PhaseEnteredAt = 0.0;

	// 타겟 이상 경고 래치 — 매 틱 평가되므로 (미션·페이즈·사유) 조합이 일정 틱 유지될 때만 1회 로그.
	mutable FString LastGuideTargetProblemKey;
	mutable int32 GuideTargetProblemTicks = 0;

	// 방금 뽑은 특성 ID — 미션판 G10 안내가 "첫 카드"가 아니라 이 카드를 지목할 때 쓰는 단서.
	// 세이브 안 함(미션 단위 resume — 재시작하면 뽑기부터 다시 한다).
	FName TutorialPulledTraitID = NAME_None;

	// dev 시작 모드 미션 오버라이드(스킵=None / 점프=Mn)를 세션당 1회(게임 시작)만 적용하기 위한 가드.
	// HandleGameDataLoaded 는 레벨 로드마다 호출되므로, 없으면 오피스 진입 등 후속 로드에서
	// 진행 중 미션이 None 으로 지워지거나(스킵) 이전 단계로 되돌려진다(점프).
	// GameInstanceSubsystem 이라 레벨 전환엔 유지, 새 PIE(새 GameInstance) 엔 false 로 리셋.
	bool bDevStartHandled = false;

	// 진행 상태 프리셋 시드도 세션 첫 로드에서만 — 위와 같은 이유(레벨 로드마다 호출됨).
	bool bPresetSeedHandled = false;

	// 가이드 단계 (MentorLines 인덱스와 1:1). 의미는 ConditionType별 — cpp 상단 BuildGuide/BrickGuide 상수 참조
	int32 GuidePhase = 0;
	// 설명 페이즈 안의 현재 설명 인덱스. 한 번에 한 장만 띄운다(동시 표시는 문구끼리 겹쳤다)
	int32 ExplainStep = 0;
	bool bExplainAdvancePending = false;
	double ExplainAdvancePendingAt = 0.0;
	// 모달 닫힘↔배치 시작 사이 transient 프레임 보호 (2연속 미스부터 페이즈 후퇴)
	int32 TransitionMissCount = 0;
	// M4 PlaceDesks — 배치한 좌석 누적 수. 동일 미션에 한해 저장·복원한다.
	int32 DeskPlaceProgress = 0;
	// 완료 처리 중 보상 지급의 OnResourceChanged 재진입 차단
	bool bCompletingMission = false;
	// 체인 마지막 미션의 카드 탭(클레임) 대기 상태. 세이브/복원되며, 중간 미션은 즉시 진행해 이 상태에 들어오지 않는다.
	bool bReadyToClaim = false;
	// M7 피날레 수령과 같은 원자 저장 경계에서 true가 된다. dev 스킵 계열도 일반 플레이 허용을 위해 true.
	bool bTutorialCompleted = false;
	// dev 미션 점프가 StartMode 스킵보다 우선한 세션 동안 일반 강화가 열리지 않게 유지한다.
	bool bDevTutorialJumpSessionActive = false;

	// 안내 대상 회사 3단 폴백: 1채면 그 회사 / 여러 채면 마지막 관리한 회사 / 기록 없으면 nullptr(지목 생략).
	// ⚠ 인수·철거(G4·G5)에는 쓰지 말 것 — 어느 회사를 고르냐가 게임플레이라 지목이 선택을 왜곡한다
	ABuildingBaseActor* PickGuideBuilding() const;

	TWeakObjectPtr<UBuildOpenWidget> BuildOpenWidgetWeak;
	TWeakObjectPtr<UFactoryPanelWidget> FactoryPanelWeak;
	// 배치 바 — M2/M4 확정 페이즈 [배치] 버튼 링 타겟. GC 를 기다리면 닫힌 바를 계속 가리키므로 패널이 destruct 때 스스로 해제한다
	TWeakObjectPtr<class UBuildPlacementPanelWidget> BuildPlacementPanelWeak;
	TWeakObjectPtr<UCommonActivatableWidget> BuildModalWeak;
	// M3 EnterOffice: [입장] 버튼 하이라이트 타겟용 관리 패널 (NotifyManagePanelOpened에서 저장, 닫히면 weak 무효화)
	TWeakObjectPtr<UBuildingManagePanelWidget> ManagePanelWeak;
	// 스포트라이트 대상 공장 (레벨 프리배치 — 월드 전환 시 weak 무효화로 자동 재탐색)
	mutable TWeakObjectPtr<AActor> BrickFactoryWeak;
	// M3 EnterOffice p0 스포트라이트 대상 — M2에서 방금 지은 회사 빌딩 (NotifyBuildingPlaced에서 캡처)
	TWeakObjectPtr<ABuildingBaseActor> TutorialFirstBuildingWeak;

	// M5 — [채용] 버튼(p0) / [뽑기] 버튼(p1) / [확인] 버튼(p2) 하이라이트 타겟
	TWeakObjectPtr<UOfficeMainWidget> OfficeMainWeak;
	TWeakObjectPtr<UOfficeRecruitmentPanelWidget> RecruitPanelWeak;
	TWeakObjectPtr<UEmployeeGachaPresentationWidget> GachaPresentationWeak;
	// 책상 패널(WorkstationInfo) 하이라이트 타겟 조회용 (M5/M8b는 도크 [채용]로 이관)
	TWeakObjectPtr<UWorkstationInfoWidget> WorkstationPanelWeak;
	// 직원창(EmployeeWindow) 약참조 (미션판 G9 안내 확장 여지)
	TWeakObjectPtr<UEmployeeWindowWidget> EmployeeWindowWeak;
	// OfficeLayer 약참조 — M7(사무실 [뒤로]) 등에서 사용
	TWeakObjectPtr<class UOfficeLayerWidget> OfficeLayerWeak;
	// M6 — 기획 보드 첫 기획안(PickInHouse) / LaunchConfirm [출시](PressLaunch) 하이라이트 타겟
	TWeakObjectPtr<class UPitchBoardWidget> PitchBoardWeak;
	TWeakObjectPtr<class ULaunchConfirmWidget> LaunchConfirmWeak;
	// 특성 가챠 패널 약참조 (미션판 G10 안내 확장 여지)
	TWeakObjectPtr<class UBuildingTraitGachaPanelWidget> TraitGachaPanelWeak;
	// 가챠 연출 위젯 약참조
	TWeakObjectPtr<class UGachaRevealPresentationWidget> GachaRevealWeak;
	// 강화(스타포스) 모달 약참조 (미션판 G9 안내 확장 여지)
	TWeakObjectPtr<class UEnhanceStarforceModalWidget> StarforceModalWeak;
	// 레벨 단위로 재구독하는 WorldSubsystem — Deinitialize/재구독 시 이전 인스턴스 해제용
	TWeakObjectPtr<class UCityAcquisitionManager> CityAcqMgrWeak;
	// OnGachaPullCompleted 구독 핸들 (Initialize 구독 / Deinitialize 해제)
	TWeakObjectPtr<URecruitmentManagerSubsystem> RecruitMgrWeak;

	UPROPERTY()
	TObjectPtr<UMissionTrackerWidget> TrackerWidget = nullptr;

	// TrackerWidget이 InGameLayer에 디자이너 배치된 인스턴스인지 (true면 소유권이 레이어 → RemoveFromParent 금지)
	bool bTrackerEmbedded = false;

	// 미션판 트래커 (체인 종료 후) — 매니저가 생성하고 임베드 MissionTracker 와 같은 부모/슬롯 규약에 부착한다.
	UPROPERTY()
	TObjectPtr<class UGoalTrackerWidget> GoalTrackerWidget = nullptr;

	// OnGoalBoardChanged 지연 구독 1회 래치 — GoalBoardSubsystem 이 이 매니저를 의존해 Initialize 시점엔 아직 없다
	bool bGoalBoardWidgetHookBound = false;

	UPROPERTY()
	TObjectPtr<UMissionGuideOverlayWidget> GuideOverlayWidget = nullptr;

	FTimerHandle PollTimerHandle;

	// 연쇄 수령 차단 — 클레임 탭이 풀스크린이라 다음 미션이 즉시 대기 상태가 되면 줄줄이 수령된다.
	// 한 번의 탭 = 최대 한 미션. 보상 토스트(약 2.7s)보다 짧게 잡아 의도적 연속 수령은 막지 않는다.
	double LastClaimTime = 0.0;
	static constexpr double ClaimCooldown = 1.5;

	// 오프닝 체인 시작 미션 (신규 게임 = 세이브 없음일 때)
	static const FName OpeningChainStartID;
};
