// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Data/EmployeeTypes.h"
#include "Enum/ItemType.h"
#include "Enum/ProductionDiscipline.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Components/SlateWrapperTypes.h"
#include "Types/SlateEnums.h"
#include "EmployeeWindowWidget.generated.h"

class UListView;
class UImage;
class UBorder;
class UCommonTextBlock;
class UButtonWidget;
class UPanelHeaderWidget;
class UResourceWidget;
class UEmployeeManager;
class UEmployeeLevelUpCardWidget;
class UEnhanceStarforceModalWidget;
class UItemCardSlotWidget;
class UTexture2D;
class AGachaCaptureStage;
class UMaterialInstanceDynamic;
class UComboBoxString;
class UOverlay;
class USizeBox;
class UEmployeeEquipmentManagerSubsystem;
class UPotentialResetWorkspaceWidget;
class UEmployeeEquipmentPanelWidget;
class UEmployeeEquipmentListWidget;
class UEquipmentDetailWindowWidget;
enum class EWidgetType : uint8;
enum class EResourceType : uint8;

enum class EEmployeeWindowPage : uint8 { Home, Equipment };

// 로스터 정렬 모드 — 정렬 순환 버튼 클릭마다 순서대로 순환 (직능은 정렬이 아니라 부서 필터로 이동 — RosterFilterCombo)
enum class ERosterSortMode : uint8
{
	Level,        // 레벨순 (기본)
	Rarity,       // 등급순
	Name,         // 이름순 (가나다)
};

/**
 * 풀스크린 직원창 (UI_EmployeeWindow) — 육성 허브. 하단 도크 [직원] 버튼이 PushPromptClass 로 오픈.
 *
 * 3컬럼: 좌 로스터 레일(UIE_EmployeeRosterCard, 종합점수 내림차순) / 중 히어로 카드 / 우 능력치+잠재 패널.
 * [강화]는 스타포스 모달(UEnhanceStarforceModalWidget) push, 채용(P3)/책상 도크 축소(P4)는 범위 밖.
 * 설계 SOT = docs/superpowers/specs/2026-07-09-employee-window-redesign-design.md
 * 목업 SOT = docs/05_UI/EmployeeWindow_MOCKUP.html (스크린 B 단일뷰, px×2 = 2560 캔버스)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeWindowWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	// ===== 등장 스태거 (v7 폴리시 — 레일/히어로/스킬/잠재 컬럼 순 페이드+슬라이드업) =====

	UPROPERTY(EditAnywhere, Category = "Intro")
	float IntroDuration = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Intro")
	float IntroStagger = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Intro")
	float IntroSlide = 18.f;

	// ===== 등급 글로우 브리딩 (앰비언트 — 강화 모달 v5 골드 숨쉬기 문법) =====

	UPROPERTY(EditAnywhere, Category = "Intro")
	float GlowBreatheHz = 0.7f;

	// RenderOpacity 진폭 (1-Amp ~ 1 사이 코사인)
	UPROPERTY(EditAnywhere, Category = "Intro")
	float GlowBreatheAmp = 0.25f;

	// ===== 크롬 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	// 상단 지갑 (Money) — OnResourceChanged 구독 갱신
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* WalletMoneyChip;

	// ===== 좌: 로스터 레일 =====

	// EntryWidgetClass = UIE_EmployeeRosterCard. 선택 = ListView 아이템 선택(단일)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UListView* RosterListView;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* RosterHeaderText;

	// 부서 필터 드롭다운 (레일 헤더 우측) — "전체" + GetDepartmentDisplayName(산업별) 6종
	UPROPERTY(meta = (BindWidgetOptional))
	UComboBoxString* RosterFilterCombo;

	// 정렬 순환 버튼 — 클릭마다 레벨→등급→이름 순환 (직능은 필터로 이동)
	UPROPERTY(meta = (BindWidgetOptional))
	UButtonWidget* RosterSortCycleBtn;

	// ===== 중: 히어로 카드 =====

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> HeroColBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> EmployeeContentSize;

	void UpdateColumnWidths(float RootWidth);

	// 히어로 라이브 뷰 (SceneCapture RT 컷아웃 — 걷는 전신). 무대 불가 시 증명사진 PNG 폴백
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* HeroPortraitImage;

	// 초상 림 — 등급색(GetRarityColor) 코드 틴트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroPortraitRim;

	// 미니 스테이지: 캐릭터 뒤 등급색 방사 글로우 (Glow_Oval) — 코드 틴트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroRarityGlow;

	// 등급 칩 — 등급명 + 등급색 코드 틴트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroRarityChipText;

	// 강화 레벨에서 파생된 DT 직급명
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroRankText;

	// 미니 스테이지: 바닥 접지 그림자 — 직원 없으면 캐릭터와 함께 숨김
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroGroundShadow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* HeroNameText;

	// 강화 배지 — 별 아이콘과 실제 강화 수치를 함께 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* StarBadge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroEnhancementIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroEnhancementText;

	// 부서 칩 — DT_DepartmentDisplay(산업별) 데이터 주도
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DeptChipText;

	// "레전드" 등급 서브 라벨 (GetKoreanName — 최장 3자)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroSubText;

	// 업무력 — 직원의 최고 직능 포인트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* OverallText;

	// "Lv.N" (캡 "/ 30" 은 HeroLevelMaxText 가 뮤트로 받는다)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroLevelMaxText;

	// [강화] 골드 CTA — P2 스타포스 모달까지 비활성 스텁
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EnhanceButton;

	// [해고] 고스트 2차 액션 — 파괴적이라 컬러 CTA 와 같은 행에 두지 않는다(CTARow 아래 DangerRow)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* FireButton;

	// (자리 비우기는 책상 도크 UI_WorkstationInfo 전담 — 직원창에서 제거 2026-07-09)

	// 중앙 기본 능력치 SP 투자 행. 저장값과 ★ 파생 보너스를 체감 단위로 표시한다.

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatName0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatValue0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatName1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatValue1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatName2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatValue2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatName3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatValue3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatName4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatValue4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatName5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatValue5;

	// 강화 보너스 "+N" 그린 배지 (v7 — 값과 분리 표기, 보너스 0이면 Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatBonus0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatBonus1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatBonus2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatBonus3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatBonus4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatBonus5;

	// 필터 콤보 SDF 배경 — Wpx/Hpx 가 실위젯 크기와 같아야 코너가 정합. 크기 변할 때만 재주입한다.
	// (절차식 RoundedBox 는 다크 위 옅은 키라인에서 코너가 깨져 SDF 로 이관 — UIE_Resource_Hud 선례)
	void UpdateFilterChipMaterialSize(const FGeometry& MyGeometry);
	FVector2D LastFilterChipSize = FVector2D::ZeroVector;

	void UpdateSurfaceMaterialSizes();

	// 스탯 셀 탭 → 효과 설명 툴팁. 6칸에 설명을 상시 노출하면 값을 가려서 탭 노출로 옮겼다(2026-07-27).
	// 탭 타깃 = 트리의 HeroStatBtn0~5 (GetWidgetFromName 이름 규약 — 바인딩 멤버 추가 없이 연결).
	void HandleStatInfoTapped(int32 StatIndex);
	UFUNCTION()
	FEventReply HandleWorkPowerTapped(FGeometry MyGeometry, const FPointerEvent& PointerEvent);

	UPROPERTY(Transient)
	class UItemTooltipWidget* ActiveStatTooltip = nullptr;

	// 효과 설명 1줄 (예: "크리티컬 확률 +6.0%") — UEmployeeStatsHelper::GetStatEffectText 단일 진실, RefreshHeroStats 가 채움
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatDesc0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatDesc1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatDesc2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatDesc3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatDesc4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroStatDesc5;

	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* AvailableStatPointsText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatPointText0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatPointText1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatPointText2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatPointText3;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatPointText4;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* HeroStatPointText5;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatNextEffectText0;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatNextEffectText1;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatNextEffectText2;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatNextEffectText3;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* HeroStatNextEffectText4;
	UPROPERTY(meta = (BindWidgetOptional)) UCommonTextBlock* HeroStatNextEffectText5;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* StatPlusButton0;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* StatPlusButton1;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* StatPlusButton2;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* StatPlusButton3;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* StatPlusButton4;
	UPROPERTY(meta = (BindWidgetOptional)) UButtonWidget* StatPlusButton5;
	void HandleStatPlusClicked(int32 StatIndex);

	// 왼쪽 성장 카드: 개인 경험치로 레벨업하고 주 직능과 SP를 얻는다.
	// UIE_EmployeeLevelUpCard is the growth action for the selected employee.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEmployeeLevelUpCardWidget* DisciplineCard;

public:
	void OpenWithContext(int32 EmployeeId, bool bOpenEquipment = true);
	void OpenEquipmentItem(int32 EmployeeId, FGuid InstanceId);
	UWidget* GetLevelUpButtonWidget() const;
	// SP가 있을 때 선택 가능한 능력치 버튼들을 함께 감싸는 영역.
	UWidget* GetStatPointTargetWidget() const;

	// M9 미션 가이드 — 스킬 카드 전체(6직능 행 + [+] 전부). "원하는 스킬"을 고르게 하려면 구멍이 카드만큼 넓어야 한다.
	// SP 가 없어 [+] 가 전부 비활성이면 nullptr — 카드는 항상 GetIsEnabled()==true 라
	// 그대로 가리키면 안전망이 못 잡고 게이트가 켜진 채 소프트락된다.
	UWidget* GetLevelUpCardWidget() const;
	// M18 미션 가이드 — [강화] 버튼 하이라이트 타겟
	UWidget* GetEnhanceButtonWidget() const;

	// 수용 기준 §9-④ 측정축 — 지금 이 프레임에 도는 상시 연출 수(글로우 브리딩 · 골드 CTA 샤인 · 빈자리 펄스).
	// 로그와 검증 하네스가 같은 값을 봐야 예산 위반이 캡처 없이도 잡힌다
	int32 GetAlwaysOnEffectCount() const;

protected:

	// 잠재능력은 직원창 안에서 명함 선택부터 재설정까지 처리한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UOverlay> PotentialHost;

	UPROPERTY()
	TObjectPtr<UPotentialResetWorkspaceWidget> PotentialWorkspace;

	void EnsurePotentialWorkspace();

	// ===== 우: 칼럼 교체 (직능·잠재 ↔ 장비) =====
	// 두 판은 배타다. [장비] 버튼이 무엇을 보여줄지 정하고, 판 제목이 그 상태를 말한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> GrowthVBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> EquipPlateSize;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UEmployeeEquipmentListWidget> EquipmentList;

	// 상세는 칼럼 왼쪽 절반에 상시 선다(사용자 결정 2026-09-26) ― 고를 때만 나타나면
	// 목록 폭이 튀어 고르던 자리를 잃는다. 자리를 미리 비워 두고 안에서만 갈아 끼운다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> EquipDetailHost;

	// 아무것도 안 고른 동안 그 자리에 보이는 안내.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> EquipDetailEmpty;

	// 판 제목 옆 = 전체 보유. 부품 안의 밴드는 현재 부위 필터의 개수라 서로 다른 수다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> EquipCountText;

	UPROPERTY() TObjectPtr<UEquipmentDetailWindowWidget> InlineDetail;

	// 히어로 초상 표시 크기 (정적 초상화 폴백 디코드 해상도) — 라이브뷰 후속까지 정적 초상화가 기본
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	FVector2D HeroPortraitSize = FVector2D(360.f, 450.f);

	// 라이브 뷰(SceneCapture) — 가챠와 동일 공유 무대(걷기 캣워크). 재활성 2026-07-20:
	//   구 "흰색 렌더"는 캡처 시작 직후 노출/TAA 정착 과도기 의심 → 워밍업 페이드인으로 가림(LiveViewWarmup*).
	UPROPERTY(EditDefaultsOnly, Category = "Portrait")
	bool bLiveHeroViewEnabled = true;

	// 라이브 뷰 워밍업 — 캡처 첫 프레임들(노출 정착)을 가리는 홀드/페이드 (초)
	UPROPERTY(EditAnywhere, Category = "Portrait")
	float LiveViewWarmupHold = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Portrait")
	float LiveViewWarmupFade = 0.3f;

	// 라이브 뷰 무대 클래스 — 가챠 리빌과 동일 BP(프레이밍/라이트/RT 공유, 동시 1개 원칙)
	UPROPERTY(EditDefaultsOnly, Category = "Portrait")
	TSoftClassPtr<AGachaCaptureStage> LiveStageClass = TSoftClassPtr<AGachaCaptureStage>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/BP_GachaCaptureStage.BP_GachaCaptureStage_C")));

private:
	int32 PendingEquipmentEmployeeId = INDEX_NONE;
	FGuid PendingEquipmentInstanceId;
	void ApplyPendingEquipmentItem();
	friend struct FEmployeeEquipmentUIValidation;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentOpenButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentSlot0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentSlot1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentSlot2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentSlot3;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentSlot4;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> HeroEquipmentSlot5;
	UPROPERTY() TObjectPtr<UEmployeeEquipmentManagerSubsystem> EquipmentManager;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> HeroSlotIconMaterials;
	TWeakObjectPtr<UImage> HeroSlotFrameImages[6];
	void UpdateHeroEquipmentFrameMaterialSize();
	FString LiveViewAppearanceKey;
	EEmployeeWindowPage CurrentEmployeePage = EEmployeeWindowPage::Home;
	float PageTransitionElapsed = -1.f;
	TWeakObjectPtr<UWidget> PageTransitionTarget;
	void FinishPageTransition();
	bool bHasExplicitEmployeeContext = false;
	// 상점 왕복 동안 선택 직원과 명함을 유지한다.
	bool bPotentialReturningFromShop = false;
	// 빈자리 선택 되돌리기 중 — SetSelectedItem 이 같은 델리게이트를 다시 쏘므로 재진입을 막는다
	bool bRestoringRosterSelection = false;
	void HandlePotentialShopRequested();
	void OpenOddsTable(int32 EmployeeID);
	void ShowEmployeePage(EEmployeeWindowPage Page);
	void HandleHeroEquipmentOpen();
	void HandleHeroEquipmentSlot(int32 Index);
	void RefreshHeroEquipment();

	// 칼럼 교체 + 인라인 상세
	void ShowEquipmentColumn(bool bShowEquipment);
	void RefreshEquipmentToggleButton();
	void RefreshEquipmentOwnedCount();
	void HandleEquipmentItemPicked(const FGuid& InstanceId);
	void RefreshInlineDetail();
	void CloseInlineDetail();
	void HandleInlineDetailPrimary();
	void HandleInlineDetailSecondary();
	FGuid InlineDetailInstanceId;
	void HandleHeroEquipmentChanged();
	void HandleHeroPortraitChanged(int32 EmployeeId, int64 Revision);
	int32 SelectedEmployeeID = -1;
	int32 BuildingIndex = INDEX_NONE;

	// 열려 있는 강화 모달 (뷰포트 오버레이 — 직원창 닫힘 시 함께 정리)
	TWeakObjectPtr<UEnhanceStarforceModalWidget> OpenEnhanceModal;

	// 열려 있는 확률표 (강화 모달과 같은 뷰포트 오버레이 — 직원창 닫힘 시 함께 정리)
	TWeakObjectPtr<class UPotentialOddsWidget> OpenOddsPanel;

	// 등장 스태거 상태 (-1 = 비활성). 대상은 이름 조회(바인딩 불필요) — WBP 이름 변경 시 자연 무시
	float IntroElapsed = -1.f;
	float GlowBreatheTime = 0.f;
	TArray<TWeakObjectPtr<UWidget>> IntroPanels;
	void StartIntroStagger();

	// ===== 값 변화 연출 (spec §9 4~9) =====
	// 전부 "값이 바뀐 순간"에만 시작하고, 끝나면 경과를 -1 로 되돌려 틱을 끈다(IntroElapsed 문법).
	// 대상 이미지는 전부 선택적 장식 — 이름이 없으면 해당 연출만 조용히 빠진다.

	// 연출 이미지 캐시 (NativeConstruct 1회 이름 조회 + DT_UIVFXTexture 주입)
	TWeakObjectPtr<UImage> SlotSparkleImages[6];
	static constexpr int32 NumEmployeeStats = static_cast<int32>(EEmployeeStatIndex::Count);
	TWeakObjectPtr<UImage> StatRowBGImages[NumEmployeeStats];
	TWeakObjectPtr<UImage> EnhanceShineImage;

	// 필터 칩 SDF 레이어 — 크기 재주입은 틱 경로라 이름 조회를 여기서 1회로 끝낸다
	TWeakObjectPtr<UWidget> FilterChipSizeWidget;
	TWeakObjectPtr<UImage> FilterChipLayers[2];

	void CacheEffectWidgets();

	// 장비 슬롯 — 바뀐 슬롯만 펀치 + 등급색 스파클 1회 팝
	FScalePunchAnimation SlotPunch[6];
	float SlotSparkleElapsed[6] = { -1.f, -1.f, -1.f, -1.f, -1.f, -1.f };
	FLinearColor SlotSparkleColor[6];
	TArray<FGuid> LastSlotItemIds;
	bool bSlotPunchScaleDirty = false;

	// 업무력 값 변경 시 숫자 카운트업
	FNumberCountUpAnimation OvrCount;
	int32 LastOverallValue = INDEX_NONE;

	// 강화 수치가 늘어난 순간 배지 전체에 펀치를 적용한다.
	FScalePunchAnimation StarChipPunch;
	int32 LastStarCount = INDEX_NONE;
	bool bStarChipScaleDirty = false;

	// 능력치 카운트업 + 행 하이라이트 + 보너스 칩 페이드인
	FNumberCountUpAnimation StatCount[NumEmployeeStats];
	float StatHighlightElapsed[NumEmployeeStats] = { -1.f, -1.f, -1.f, -1.f, -1.f, -1.f };
	float StatBonusFadeElapsed[NumEmployeeStats] = { -1.f, -1.f, -1.f, -1.f, -1.f, -1.f };
	int32 LastStatValues[NumEmployeeStats] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };

	// 기준선이 누구 것인지 — 직원이 바뀌면 이전 값은 남의 값이라 전부 버리고 다시 채운다(첫 채움 = 무연출)
	int32 EffectBaselineEmployeeID = INDEX_NONE;
	void SyncEffectBaselineOwner();
	void ResetValueEffectBaselines();

	// 골드 CTA 샤인 스윕 (글로우 브리딩과 함께 유이한 상시 틱)
	float CtaSweepClock = 0.f;

	// 수용 기준 §9-④ 근거 로그 (비-shipping 에서만 1초 1회 찍는다)
	float EffectBudgetLogClock = 0.f;

	// 로스터 빈자리 타일 펄스 게이트 — 채용 재화(채용권/다이아) 여유가 있을 때만 숨쉰다
	bool CanAffordAnyHire() const;
	void RefreshHireSlotAffordability();

	// 최초 진입 코치마크 (설계 2026-08-12) — 등장 스태거가 끝난 뒤 시작해야 구멍이 이동 중인 위젯을 쫓지 않는다
	void TryPlayPanelIntro();
	void HandlePanelIntroFinished();
	FTimerHandle PanelIntroTimer;
	TWeakObjectPtr<class UPanelIntroOverlayWidget> PanelIntroOverlay;

	UPROPERTY()
	UEmployeeManager* EmployeeManager;

	// ===== 히어로 라이브 뷰 (위젯 활성 동안만 캡처 — Activated/Deactivated 쌍) =====

	UPROPERTY()
	AGachaCaptureStage* LiveStage = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* LiveViewMID = nullptr;

	// 재의상 판정 키 — 직원/강화(의상 축) 변화 시에만 워커 재스폰
	int32 LiveViewEmployeeID = -1;
	int32 LiveViewEnhancementLevel = -1;

	// 워밍업 경과 (-1 = 비활성) — NativeTick 이 RT 이미지 페이드인
	float LiveViewWarmupElapsed = -1.f;

	// 선택 직원을 무대에 입혀 걷기 캡처 시작/교체 + RT 브러시 표시. 무대 불가 시 PNG 폴백.
	void UpdateHeroLiveView(FEmployeeInstance* Employee);

	// 캡처 정지 + 무대 반납 (가챠가 인계 중이면 no-op)
	void ReleaseHeroLiveView();
	FReply BeginHeroRotation(const FPointerEvent& PointerEvent);
	FReply MoveHeroRotation(const FPointerEvent& PointerEvent);
	FReply EndHeroRotation(const FPointerEvent& PointerEvent);
	void CancelHeroRotation();
	bool IsHeroRotationPointer(const FPointerEvent& PointerEvent) const;
	int32 HeroRotationPointerIndex = INDEX_NONE;
	int32 HeroRotationUserIndex = INDEX_NONE;
	bool bHeroRotationTouch = false;
	FVector2D HeroRotationLastPosition = FVector2D::ZeroVector;

	// 로스터 재수집(부서 필터 → 정렬 순) + ListView 채움 + 선택 유지(사라졌으면 1위)
	void RefreshRoster();
	// 표시 중인 카드와 재활용용 아이템 스냅샷을 함께 갱신한다. 정렬/스크롤은 유지한다.
	void RefreshRosterEmployee(int32 EmployeeID);
	void UpdateRosterSortLabel();

	// 부서 필터 옵션 채움 ("전체" + 6직능 표시명, CompanyType 데이터 주도) — BuildingIndex 확정 후(NativeOnActivated) 호출
	void SetupRosterFilterCombo();

	// RosterSortCycleBtn 클릭 (CommonButtonBase 네이티브 OnClicked — AddUObject 로 바인딩, UFUNCTION 불필요)
	void OnRosterSortCycleClicked();

	// RosterFilterCombo 선택 변경 (UComboBoxString 다이내믹 멀티캐스트 — AddDynamic 바인딩)
	UFUNCTION()
	void OnRosterFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	ERosterSortMode RosterSortMode = ERosterSortMode::Level;

	// 부서 필터 — EProductionDiscipline::Count = "전체"(직능 없음과 같은 센티넬 규약, DepartmentToDiscipline 참조)
	EProductionDiscipline RosterFilterDiscipline = EProductionDiscipline::Count;

	// 히어로 + 능력치 + 잠재 일괄 갱신 (선택 직원 기준)
	void RefreshSelected();
	void RefreshHero();
	void RefreshHeroStats();

	void HandleItemChanged(EItemType Type, int32 NewCount, int32 Delta);
	void RefreshExpOnly();
	void RefreshWallet();

	void SelectEmployee(int32 EmployeeID);

	// 선택 직원 데이터 (없으면 nullptr)
	FEmployeeInstance* GetSelectedEmployee() const;

	// ===== 핸들러 =====

	UFUNCTION()
	void OnRosterCardClicked(int32 EmployeeID);

	// 로스터 빈자리 타일 → 채용 패널 (직원 선택 경로와 분리)
	UFUNCTION()
	void OnHireSlotClicked();

	// ListView 선택이 빈자리 타일에 얹히는 것을 막는다 (선택 링 = 직원 전용)
	void OnRosterItemSelectionChanged(UObject* Item);

	// ListView Entry 생성 시 카드 클릭 델리게이트 바인딩 (재활용 대응 AddUniqueDynamic)
	void OnRosterEntryGenerated(UUserWidget& EntryWidget);

	// [강화] → 스타포스 모달 push + Configure(선택 직원)
	void OnEnhanceClicked();

	// [해고] → 확인 모달. 되돌릴 수 없으므로 확인 없이는 실행하지 않는다
	void OnFireClicked();

	// 확인 모달의 [확인] 콜백. 대상 ID 를 페이로드로 받는 이유 = 모달이 떠 있는 동안
	// 로스터 갱신으로 선택이 옮겨가면 엉뚱한 직원이 잘린다
	void HandleFireConfirmed(int32 EmployeeID);

	UFUNCTION()
	void OnCloseDelegate();

	// ===== 매니저 이벤트 구독 (키 필터 + 부분 갱신) =====

	void HandleEmployeeStatsChanged(int32 EmployeeID);
	void HandleEmployeeDisciplineChanged(int32 EmployeeID);
	void HandleEmployeeLevelUp(int32 EmployeeID, int32 NewLevel);
	void HandleRosterChanged();
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

};
