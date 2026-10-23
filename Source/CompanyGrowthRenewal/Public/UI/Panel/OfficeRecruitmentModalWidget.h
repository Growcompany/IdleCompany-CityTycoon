#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/GachaTier.h"
#include "Enum/ItemType.h"
#include "Enum/ProductionDiscipline.h"
#include "Enum/ResourceType.h"
#include "OfficeRecruitmentModalWidget.generated.h"

class AWorkstationActorBase;
class UButtonWidget;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UIconWithButtonWidget;
class UModalFrameWidget;
class UOfficeRecruitmentFlowSubsystem;
class URecruitmentManagerSubsystem;
class UItemInventoryManager;
class UEmployeeManager;
class UResourceItemManager;
class UTextBlock;
class UWidget;
class UImage;
class USizeBox;
class UPanelIntroOverlayWidget;

UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRecruitmentModalWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	void SetBuildingIndex(int32 InBuildingIndex);
	void SetPreferredWorkstation(AWorkstationActorBase* InWorkstation);
	UWidget* GetJobSelectionWidget(EProductionDiscipline Discipline = EProductionDiscipline::Count) const;
	UWidget* GetPullButtonMultiWidget() const;
	// 한 명만 더 필요할 때 ― 일괄 채용으로 정원을 넘겨 채우지 않게
	UWidget* GetPullButtonSingleWidget() const;
	// 원격 검증 재현용(콘솔 cg.OfficeRecruit.Select/Press). 게임 규칙은 일반 클릭과 같다.
	void DebugSelect(int32 JobIndex, int32 TierIndex);
	void DebugPress(bool bBatch);
	void DebugOpenProbability();
	void DebugPressSeatGuide();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta=(BindWidget)) TObjectPtr<UModalFrameWidget> ModalFrame;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USizeBox> ViewportExtent;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobSelectionArea;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> JobPlan;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> JobDev;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> JobGraphics;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> JobSound;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> JobServer;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> JobQA;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> TicketNormal;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> TicketAdvanced;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCommonButtonBase> TicketPremium;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UIconWithButtonWidget> PullButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UIconWithButtonWidget> PullButtonMulti;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButtonWidget> ProbabilityButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> FreeSeatsText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobCaptionText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> FailureText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketNormalCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketAdvancedCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketPremiumCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketHeading;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketNormalName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketAdvancedName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TicketPremiumName;
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor TicketLabelColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor SelectedTicketLabelColor = FLinearColor::White;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobPlanCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobPlanName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobDevName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobGraphicsName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobSoundName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobServerName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobQAName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobDevCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobGraphicsCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobSoundCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobServerCount;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JobQACount;
	// 직무 타일의 선택 장식 묶음(흰 테두리·직무색 빛·밝은 실루엣 면·체크 배지). C++ 은 보이기만 바꾼다.
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobPlanSelected;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobDevSelected;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobGraphicsSelected;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobSoundSelected;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobServerSelected;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> JobQASelected;
	// 2026-10-11 가로 2단 개편 바인딩.
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> TicketHero;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> StageGlow;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StageTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StageOwned;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsCaptionTier;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsGrade0;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsGrade1;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsGrade2;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsGrade3;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsGrade4;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsValue0;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsValue1;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsValue2;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsValue3;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> OddsValue4;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> TicketNormalIcon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> TicketAdvancedIcon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> TicketPremiumIcon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> TicketNormalSelectRing;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> TicketAdvancedSelectRing;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> TicketPremiumSelectRing;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> BodyDim;
	// 2026-10-12 막힘 이유 안내(채용권·다이아 부족).
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> BlockedHintText;
	// 빈 자리 0 안내(버튼 위 빈 공간): 제목·설명·[책상 배치] 바로가기. 정원이 찬 경우는 버튼을 숨긴다.
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> SeatGuide;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> SeatGuideTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> SeatGuideBody;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButtonWidget> SeatGuideButton;
	// 채용 버튼 글자색은 채용권 글자색과 분리한다(탭이 어두워져 같은 값을 쓸 수 없다).
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor PullTextColor = FLinearColor(0.0284f, 0.0284f, 0.0284f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor PullDisabledTextColor = FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor OddsValueColor = FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor OddsEmptyColor = FLinearColor(0.1119f, 0.1384f, 0.1878f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor TicketEmptyCountColor = FLinearColor(0.3093f, 0.3663f, 0.4678f, 1.f);
	// 제목 줄 "빈 자리 N" 숫자색: 있음 = 흰색, 0 = 실패 문구와 같은 #FF8E7A.
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor SeatCountColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor SeatEmptyColor = FLinearColor(1.f, 0.2705f, 0.1946f, 1.f);
	// 무대 빛 = 채용권 그림의 몸통색(아이콘 텍스처에는 색 정보가 없다). 일반 #44AFEB · 고급 #A746EB · 프리미엄 #EBB044.
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor StageGlowNormalColor = FLinearColor(0.0578f, 0.4287f, 0.8308f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor StageGlowAdvancedColor = FLinearColor(0.3864f, 0.0612f, 0.8308f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Recruitment|Appearance") FLinearColor StageGlowPremiumColor = FLinearColor(0.8308f, 0.4342f, 0.0578f, 1.f);

private:
	FTimerHandle DepartmentIntroTimer;
	TWeakObjectPtr<UPanelIntroOverlayWidget> DepartmentIntroOverlay;
	void TryPlayDepartmentIntro();
	void StopDepartmentIntro();
	UPROPERTY(Transient) TObjectPtr<UCommonButtonGroupBase> JobGroup;
	UPROPERTY(Transient) TObjectPtr<UCommonButtonGroupBase> TicketGroup;
	TWeakObjectPtr<AWorkstationActorBase> PreferredWorkstation;
	TWeakObjectPtr<UOfficeRecruitmentFlowSubsystem> RecruitmentFlow;
	TWeakObjectPtr<URecruitmentManagerSubsystem> RecruitmentManager;
	TWeakObjectPtr<UItemInventoryManager> ItemInventory;
	TWeakObjectPtr<UEmployeeManager> EmployeeManager;
	TWeakObjectPtr<UResourceItemManager> Resources;
	int32 BuildingIndex = INDEX_NONE;
	EProductionDiscipline SelectedJob = EProductionDiscipline::Dev;
	EGachaTier SelectedTier = EGachaTier::Normal;
	bool bRequestPending = false;
	FVector2D LastViewportExtent = FVector2D::ZeroVector;
	enum class EHireFxPhase : uint8 { Idle, Waiting, Succeeding, Failing };
	EHireFxPhase HireFxPhase = EHireFxPhase::Idle;
	FText HireFxFailureMessage;
	float HireFxClock = 0.f;
	bool bHireFxCalm = false;
	bool bHoldingEntrance = false;
	// 창이 떠 있는 동안 숨긴 사무실 메인의 [기획안 보기] 묶음(RightIdleZone).
	TWeakObjectPtr<UWidget> HiddenOfficeIdleZone;
	void HideOfficeIdleZone();
	void RestoreOfficeIdleZone();
	TWeakObjectPtr<UWidget> HireFxButton;

	UFUNCTION() void HandleJobClicked(UCommonButtonBase* Button, int32 Index);
	UFUNCTION() void HandleTicketClicked(UCommonButtonBase* Button, int32 Index);
	UFUNCTION() void HandleCloseClicked();
	void HandlePullClicked();
	void HandlePullMultiClicked();
	void HandleProbabilityClicked();
	void HandleSeatGuideClicked();
	// 빈 자리 0 에서 채용을 누르면 안내를 한 번 깜빡여 시선을 끈다(1 → 0).
	float SeatGuidePulse = 0.f;
	void HandleItemChanged(EItemType Type, int32 NewCount, int32 Delta);
	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	void HandleRequestFinished(bool bSuccess, const FName& Reason);
	void HandleTrackedGoalChanged(FName GoalID);
	EProductionDiscipline GetRequiredRecruitDiscipline() const;
	void SyncRequiredJobSelection();
	void Refresh();
	void RefreshJobNames();
	void RefreshViewportExtent();
	int32 GetAvailableCount() const;
	bool CanSinglePullWithDiamond() const;
	void RequestPull(int32 Count, UWidget* SourceButton);
	void BeginHireFx(UWidget* SourceButton);
	void ResolveHireFx(bool bSuccess, const FText& FailureMessage);
	void TickHireFx(float DeltaTime);
	void ResetHireFx(bool bResetFrame);
	void FinishHireFxSuccess();
	void FinishHireFxFailure();
	void ReleaseHeldEntrance();
	void RefreshStageAndOdds();
};
