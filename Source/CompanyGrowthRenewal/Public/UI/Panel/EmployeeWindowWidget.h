// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Data/EmployeeTypes.h"
#include "Enum/ItemType.h"
#include "Enum/ProductionDiscipline.h"
#include "Types/SlateEnums.h"
#include "EmployeeWindowWidget.generated.h"

class UListView;
class UImage;
class UBorder;
class UProgressBar;
class UCommonTextBlock;
class UButtonWidget;
class UCloseButtonWidget;
class UResourceWidget;
class UEmployeeManager;
class UDisciplineCardWidget;
class UEnhanceStarforceModalWidget;
class UItemCardSlotWidget;
class UTexture2D;
class AGachaCaptureStage;
class UMaterialInstanceDynamic;
class UComboBoxString;
enum class EResourceType : uint8;

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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
	UCloseButtonWidget* CloseButton;

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

	// 히어로 라이브 뷰 (SceneCapture RT 컷아웃 — 걷는 전신). 무대 불가 시 증명사진 PNG 폴백
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* HeroPortraitImage;

	// 초상 림 — 등급색(GetRarityColor) 코드 틴트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroPortraitRim;

	// 미니 스테이지: 캐릭터 뒤 등급색 방사 글로우 (Glow_Oval) — 코드 틴트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroRarityGlow;

	// 미니 스테이지: 바닥 접지 그림자 — 직원 없으면 캐릭터와 함께 숨김
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroGroundShadow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* HeroNameText;

	// 별 배지 — 강화 수 만큼 별 아이콘 동적 생성 (0강 = 빈 별 1개 자리표시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* StarBadge;

	// 부서 칩 — DT_DepartmentDisplay(산업별) 데이터 주도
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DeptChipText;

	// "레전드" 등급 서브 라벨 (GetKoreanName — 최장 3자)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* HeroSubText;

	// 종합 수치 (CalculateOverall, 골드)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* OverallText;

	// "Lv.N / 30"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* ExpBar;

	// 채움 끝 글로우 헤드 (장식 — 없어도 바는 정상 동작)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ExpBarHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* ExpText;

	// [강화] 골드 CTA — P2 스타포스 모달까지 비활성 스텁
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EnhanceButton;

	// [해고] 고스트 2차 액션 — 파괴적이라 컬러 CTA 와 같은 행에 두지 않는다(CTARow 아래 DangerRow)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* FireButton;

	// (자리 비우기는 책상 도크 UI_WorkstationInfo 전담 — 직원창에서 제거 2026-07-09)

	// ===== 중: 히어로 능력치 미니그리드 (6스탯 2x3 — 표시 전용: 저장값 + 강화 보너스, D안) =====
	// 이름 = UEmployeeStatsHelper::GetStatDisplayName(i), 값 = 유효값(+강화보너스는 "N (+B)" 표기). RefreshHeroStats 가 채움.

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

	// 스탯 셀 탭 → 효과 설명 툴팁. 6칸에 설명을 상시 노출하면 값을 가려서 탭 노출로 옮겼다(2026-07-27).
	// 탭 타깃 = 트리의 HeroStatBtn0~5 (GetWidgetFromName 이름 규약 — 바인딩 멤버 추가 없이 연결).
	void HandleStatInfoTapped(int32 StatIndex);

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

	// ===== 우: 직능 투자 카드 (레벨업 SP → 6직능 특화) =====
	// UIE_DisciplineCard. M9 가이드 타겟 경로라 누락을 런타임 무음 null 로 넘기지 않는다(required).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCardWidget* DisciplineCard;

public:
	// M9 미션 가이드 — 스킬 카드 첫 [+] 버튼 하이라이트 타겟
	UWidget* GetFirstSkillInvestButtonWidget() const;

	// M9 미션 가이드 — 스킬 카드 전체(6직능 행 + [+] 전부). "원하는 스킬"을 고르게 하려면 구멍이 카드만큼 넓어야 한다.
	// SP 가 없어 [+] 가 전부 비활성이면 nullptr — 카드는 항상 GetIsEnabled()==true 라
	// 그대로 가리키면 안전망이 못 잡고 게이트가 켜진 채 소프트락된다.
	UWidget* GetSkillCardWidget() const;
	// M18 미션 가이드 — [강화] 버튼 하이라이트 타겟
	UWidget* GetEnhanceButtonWidget() const;

protected:

	// ===== 우: 잠재 패널 (줄 3칸 + 리롤) =====

	// 현재 등급 배지 — 코드 SetBrushColor(GetRarityColor)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* TierBadge;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TierText;

	// 줄 컨테이너 3칸 — 잠긴 칸은 RenderOpacity 딤 + "N성에 해금"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* PotLine0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* PotLine1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* PotLine2;

	// 좌측 등급색 바 (흰 fill — 코드 틴트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineBar0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineBar1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineBar2;

	// v7 젤 플레이트: 등급색 틴트 그라데이션 (GradationImg_White_Top, 흰 원본 — 코드 틴트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineGrad0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineGrad1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineGrad2;

	// v7 젤 플레이트: 내부 림 글로우 (RoundedBox 두꺼운 아웃라인, 흰 원본 — 코드 틴트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineGlow0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineGlow1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotLineGlow2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PotLineName0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PotLineName1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PotLineName2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PotLineValue0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PotLineValue1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PotLineValue2;

	// 명함 선택 슬롯 3개 (종이/골드/블랙, CubeTypes 순서 고정) — UIE_ItemCard_QtyBelow 재사용, 클릭은 카드 자체 델리게이트(OnItemCardClicked)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UItemCardSlotWidget* CubeSlot0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UItemCardSlotWidget* CubeSlot1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UItemCardSlotWidget* CubeSlot2;

	// (선택 표시 = ItemCard 내장 SetSelected 글로우 단일 — 구 CubeRing 오버레이는 2026-07-20 중복 제거)

	// 명함 이름/등급상한 캡션 (v7 — 이름=EItemType DisplayName, 상한=GetCubeCeiling 데이터 주도)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CubeName0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CubeName1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CubeName2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CubeCap0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CubeCap1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CubeCap2;

	// [명함 리롤]
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* RerollButton;

	// [확률] — 잠재 재설정 확률표 열기. Optional 이면 유실 시 진입점이 조용히 사라진다(RerollButton 과 같은 등급)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* OddsButton;

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
		TEXT("/Game/CompanyGrowth/Characters/StickmanCG/BP_GachaCaptureStage.BP_GachaCaptureStage_C")));

	// 명함 아이콘 폴백 — DT_ShopItem 에 해당 EItemType 행이 없을 때만. 3종 모두 행이 있으면 이 경로는 안 탄다.
	UPROPERTY(EditDefaultsOnly, Category = "Potential")
	TSoftObjectPtr<UTexture2D> FallbackCubeIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/BusinessCardPaper.BusinessCardPaper")));

private:
	int32 SelectedEmployeeID = -1;
	int32 BuildingIndex = INDEX_NONE;

	// 열려 있는 강화 모달 (뷰포트 오버레이 — 직원창 닫힘 시 함께 정리)
	TWeakObjectPtr<UEnhanceStarforceModalWidget> OpenEnhanceModal;

	// 열려 있는 확률표 (강화 모달과 같은 뷰포트 오버레이 — 직원창 닫힘 시 함께 정리)
	TWeakObjectPtr<class UPotentialOddsWidget> OpenOddsPanel;

	// 잠재 리롤 연출 (-1 = 비활성) — 플레이트 3줄 순차 재추첨(펀치+페이드) + 등급 배지 펀치 + 큐브 소모 눌림
	float RerollFxElapsed = -1.f;
	bool bRerollUpgraded = false;
	int32 RerollCubeIndex = -1;

	// FX 시작 시점의 줄별 목표 오파시티 (잠금 줄 딤 0.55 보존 — FX 는 비율 페이드)
	float RerollLineTargetOpacity[3] = { 1.f, 1.f, 1.f };

	// 등장 스태거 상태 (-1 = 비활성). 대상은 이름 조회(바인딩 불필요) — WBP 이름 변경 시 자연 무시
	float IntroElapsed = -1.f;
	float GlowBreatheTime = 0.f;
	TArray<TWeakObjectPtr<UWidget>> IntroPanels;
	void StartIntroStagger();

	// 최초 진입 코치마크 (설계 2026-08-12) — 등장 스태거가 끝난 뒤 시작해야 구멍이 이동 중인 위젯을 쫓지 않는다
	void TryPlayPanelIntro();
	void HandlePanelIntroFinished();
	FTimerHandle PanelIntroTimer;
	TWeakObjectPtr<class UPanelIntroOverlayWidget> PanelIntroOverlay;
	// 인트로가 도는 동안 숨겨 둔 미션 가이드 오버레이 (딤 2겹 방지) — 종료 시 되돌린다
	TWeakObjectPtr<UWidget> HiddenMissionOverlay;

	// 잠재 리롤에 사용할 명함 (기본 = 종이 명함) — 명함 슬롯 클릭으로 변경
	EItemType SelectedCube = EItemType::BusinessCardPaper;

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

	// 로스터 재수집(부서 필터 → 정렬 순) + ListView 채움 + 선택 유지(사라졌으면 1위)
	void RefreshRoster();
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
	void RefreshPotential();

	// 잠재 패널 하단 큐브 슬롯 3개(아이콘/수량/딤/선택 링) + RerollButton 활성 상태 갱신
	void RefreshCubeSlots();
	void RefreshExpOnly();
	void RefreshWallet();

	void SelectEmployee(int32 EmployeeID);

	// 선택 직원 데이터 (없으면 nullptr)
	FEmployeeInstance* GetSelectedEmployee() const;

	// ===== 핸들러 =====

	UFUNCTION()
	void OnRosterCardClicked(int32 EmployeeID);

	// ListView Entry 생성 시 카드 클릭 델리게이트 바인딩 (재활용 대응 AddUniqueDynamic)
	void OnRosterEntryGenerated(UUserWidget& EntryWidget);

	void OnRerollClicked();

	// 큐브 슬롯 클릭 (CubeIndex = 0~2, CubeTypes 배열 인덱스) → SelectedCube 변경 + 슬롯/링 갱신. 품절 큐브는 무시.
	void OnCubeSlotClicked(FName ItemID, int32 CubeIndex);

	// [강화] → 스타포스 모달 push + Configure(선택 직원)
	void OnEnhanceClicked();

	// [해고] → 확인 모달. 되돌릴 수 없으므로 확인 없이는 실행하지 않는다
	void OnFireClicked();

	// 확인 모달의 [확인] 콜백. 대상 ID 를 페이로드로 받는 이유 = 모달이 떠 있는 동안
	// 로스터 갱신으로 선택이 옮겨가면 엉뚱한 직원이 잘린다
	void HandleFireConfirmed(int32 EmployeeID);

	void OnOddsClicked();

	UFUNCTION()
	void OnCloseDelegate();

	// ===== 매니저 이벤트 구독 (키 필터 + 부분 갱신) =====

	void HandleEmployeeStatsChanged(int32 EmployeeID);
	void HandleEmployeeDisciplineChanged(int32 EmployeeID);
	void HandleEmployeeLevelUp(int32 EmployeeID, int32 NewLevel);
	void HandleRosterChanged();
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	// OnExperienceGained 은 다이내믹 멀티캐스트 (UPROPERTY BlueprintAssignable)
	UFUNCTION()
	void HandleExperienceGained(int32 EmployeeID, float Amount);
};
