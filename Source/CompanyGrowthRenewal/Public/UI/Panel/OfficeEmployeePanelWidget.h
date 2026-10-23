#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Player/PlayerCamera.h"
#include "Office/OfficeEmployeeInspect.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeEquipmentData.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "Layout/SlateRect.h"
#include "Types/SlateEnums.h"
#include "OfficeEmployeePanelWidget.generated.h"

class UWidget;
class UCanvasPanel;
class UOverlay;

class UTextBlock;
class UButton;
class UImage;
class UPanelWidget;
class UWidgetSwitcher;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UButtonWidget;
class UCloseButtonWidget;
class UResourceWidget;
class UScrollBox;
class UEmployeeLevelUpCardWidget;
class UOfficeEmployeeStatRowWidget;
class UPotentialResetWorkspaceWidget;
class UPotentialOddsWidget;
class UEnhanceStarforceModalWidget;
class UEmployeeBadgeCardWidget;
class UUniformGridPanel;
class UComboBoxString;
class UCommonActivatableWidget;
class UEmployeeEquipmentListWidget;
class UEquipmentDetailWindowWidget;
class UEmployeeEquipmentDialogWidget;
class UEquipmentCalloutLinesWidget;
class UIconWithButtonWidget;
class USizeBox;
class UMaterialInstanceDynamic;
class USelectionChevronWidget;
class UEmployeeRotateRingWidget;
class AFunnyOfficeworker;
class UEmployeeManager;
class AOfficeCameraPawn;
class AOfficeworker;
enum class EResourceType : uint8;

// 왼쪽 사원증 레일의 거르기·정렬 상태 (OfficeEmployeePanelRoster.cpp)
namespace OfficeEmployeeRoster
{
	enum class EFilterKind : uint8 { All, Discipline, LevelUpReady, Bench };

	struct FFilterOption
	{
		EFilterKind Kind = EFilterKind::All;
		int32 DisciplineSlot = INDEX_NONE;
		bool operator==(const FFilterOption& Other) const { return Kind == Other.Kind && DisciplineSlot == Other.DisciplineSlot; }
	};

	enum class ESortMode : uint8 { Todo, Level, Name, Count };
}

/**
 * 사무실 직원 패널 (UI_OfficeEmployeePanel) — 3D 사무실을 배경으로 좌측 사원증 레일 + 우측 840 패널.
 * 직원을 고르면 그 직원을 두 판 사이 빈 공간 가운데로 잡고(렌즈 시프트), 직원은 카메라 쪽으로 돌아선다.
 * BottomStack push 라 도크·좌측 레일은 스택이 내리고, 상단 HUD 는 OfficeLayer 억제 API 로 접는다.
 * 설계 SOT = docs/superpowers/specs/2026-10-14-office-employee-panel-design.md
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeEmployeePanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UOfficeEmployeePanelWidget(const FObjectInitializer& ObjectInitializer);

	// 열려 있는 동안 게임 입력 차단 = 카메라 드래그·줌과 다른 3D 직원 탭을 막는다
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// push 직후 호출. 아직 활성화 전이면 보류했다가 NativeOnActivated 에서 적용한다.
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel")
	void OpenForEmployee(int32 EmployeeId);

	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel")
	void SelectEmployee(int32 EmployeeId);

	UFUNCTION(BlueprintPure, Category = "Office Employee Panel")
	int32 GetSelectedEmployeeId() const { return SelectedEmployeeId; }

	// 탭 인덱스 = TabSwitcher 자식 순서 (0 성장 / 1 잠재능력 / 2 장비). 클릭이 아니라 무음
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel")
	void SelectTab(int32 TabIndex);

	// 튜토리얼 앵커 (직원 성장 안내)
	UWidget* GetLevelUpButtonWidget() const;
	UWidget* GetCloseButtonWidget() const;
	UWidget* GetStatPointTargetWidget() const;
	// 성장 탭이 아니면 「성장」 탭 버튼(푸터가 성장 탭에만 있다)
	UFUNCTION(BlueprintPure, Category = "Office Employee Panel|Tutorial")
	UWidget* GetEnhanceButtonWidget() const;

	// 튜토리얼 앵커 (장비 장착 안내, OfficeEmployeePanelEquipment.cpp) — 화면에 실제로 있을 때만 값을 준다
	bool IsEquipmentTabActive() const;
	UWidget* GetEquipmentTabButtonWidget() const;
	// 선택 직원의 빈 부위 칸 — 보유 0개 부위 우선(그 칸을 골라야 [장비 뽑기]가 든 빈 목록 안내가 뜬다)
	UWidget* GetEmptyEquipmentSlotWidget() const;
	UWidget* GetEquipmentGachaButtonWidget() const;
	// 아무도 안 낀 보유 장비 타일(목록 순서 첫째). 부위 거르기로 안 보이면 nullptr — 그때는 「전체」 칩
	UWidget* GetUnequippedEquipmentEntryWidget() const;
	UWidget* GetAllEquipmentPartsChipWidget() const;
	// 상세 카드의 [장착] — 카드가 닫혔거나 이미 이 직원이 낀 장비(주 버튼 = 해제)면 nullptr
	UWidget* GetEquipDetailEquipButtonWidget() const;

	// BottomStack 에 push + OpenForEmployee + UI 모드. 위젯 클래스가 미등록이면 nullptr — 호출부는 구 경로로 폴백한다.
	static UOfficeEmployeePanelWidget* PushForEmployee(const UObject* WorldContextObject, int32 EmployeeId);

	// 스크립트·자동화 진입점(-game 파이썬은 static 일반 함수를 못 부른다) — 동작은 PushForEmployee 와 같다
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel", meta = (WorldContext = "WorldContextObject"))
	static UOfficeEmployeePanelWidget* OpenPanelForEmployee(const UObject* WorldContextObject, int32 EmployeeId);

	// DT_WidgetClass 에 이 패널이 등록돼 있는가 — 자기를 닫고 넘어가는 호출부가 닫기 전에 폴백 여부를 정한다
	static bool IsPanelClassRegistered(const UObject* WorldContextObject);

	// 할 일 순 = 레벨업 가능 직원 먼저, 그다음 레벨 내림차순 (동률은 입력 순서 유지)
	static void SortRosterByTodo(const UEmployeeManager& EmployeeManager, TArray<FEmployeeInstance>& InOutRoster);

	// 현재 관리 건물 직원 중 할 일 순 첫 직원 — 레일 표시 순서(사무실 묶음 → 대기 묶음)의 맨 앞 (없으면 INDEX_NONE)
	static int32 FindFirstTodoEmployeeId(const UObject* WorldContextObject);

	// 레일 「모두 레벨업」과 같은 동작 — 자동화 진입점. 레벨업한 인원 수
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel")
	int32 LevelUpAllReady();

	// 레일 필터(0 = 전체, 이후 콤보 옵션 순서)·정렬(0 할 일 순 / 1 레벨순 / 2 이름순) — 자동화 진입점
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel")
	void SetRosterFilterIndex(int32 OptionIndex);

	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel")
	void CycleRosterSort();

	// 레일 표시 순서 (‹ › 순환 기준, 사무실 → 대기)
	UFUNCTION(BlueprintPure, Category = "Office Employee Panel")
	TArray<int32> GetRosterOrder() const { return RosterOrder; }

	// ===== 장비 탭 자동화 진입점 (OfficeEmployeePanelEquipment.cpp) — 칸 클릭·목록 선택·상세 버튼과 같은 경로 =====

	// 부위 = EEmployeeEquipmentSlot 순서 (0 머리 · 1 안경 · 2 의상 · 3 손 · 4 신발 · 5 가방)
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	void SelectEquipmentPart(int32 PartIndex);

	// 부위 칩 — 0 = 전체, 1..6 = 부위 + 1 (칩 버튼과 같은 경로)
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	void SelectEquipmentChip(int32 ChipIndex);

	// 현재 부위(「전체」면 모든 부위) 보유 장비 중 목록 순서(등급·강화 내림차순) N 번째를 고른다 — 상세 카드가 열린다
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	bool PickEquipmentItem(int32 OrderIndex);

	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	void PressEquipmentDetailPrimary();

	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	void PressEquipmentDetailSecondary();

	// 장비 강화 다이얼로그의 [강화하기]
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	void ConfirmEquipmentDialog();

	// 검증용 상태 덤프(JSON) — 칸·앵커·지시선 좌표(월드 레이어 로컬), 보유·장착, 카드·연출 상태
	UFUNCTION(BlueprintCallable, Category = "Office Employee Panel|Equipment")
	FString GetEquipmentDebugState() const;

	// 검증용 — 화면 절대 좌표에 실제 터치 누름·(끌기)·뗌을 Slate 로 흘린다(히트 순서·끌기 거부까지 실제 경로). 반환 = 누른 자리 경로 끝 위젯들
	UFUNCTION(BlueprintCallable, Category = "Test")
	FString TestClickAtAbsolute(FVector2D AbsolutePosition, float DragPixels = 0.f);

	// 검증용 — 위젯 안 비율 지점(0~1)을 위와 같은 실제 터치로 누른다 (파이썬은 FGeometry 를 못 옮긴다)
	UFUNCTION(BlueprintCallable, Category = "Test")
	FString TestTapWidget(UWidget* Target, FVector2D Fraction, float DragPixels = 0.f);

	// 검증용 — 위젯의 화면 절대 사각형 (왼·위·오른·아래)
	UFUNCTION(BlueprintCallable, Category = "Test")
	FVector4 TestGetAbsoluteRect(UWidget* Target) const;

	// 검증용 — 터치 누름(bBegin)과 뗌을 다른 프레임에 따로 흘린다. 반환 = 경로 끝 위젯 + 그 순간 포인터 0 의 캡처 주인·키보드 포커스
	UFUNCTION(BlueprintCallable, Category = "Test")
	FString TestTouchPhase(UWidget* Target, FVector2D Fraction, bool bBegin);

	// 검증용 — 기존 직원창(EWidgetType::EmployeeWindow)을 PromptStack 에 연다. 진입 3곳이 모두 이 패널로 바뀌어 공용 잠재 워크스페이스 회귀 캡처 경로가 없다
	UFUNCTION(BlueprintCallable, Category = "Test", meta = (WorldContext = "WorldContextObject"))
	static UCommonActivatableWidget* TestOpenLegacyEmployeeWindow(const UObject* WorldContextObject);

	// 검증용 — 선택 직원에게 머리 장비 외형을 미리 입힌다(장착 아님, 패널 닫힘·전환 때 걷힘). 셰브론 큰 모자 캡처용
	UFUNCTION(BlueprintCallable, Category = "Test")
	bool TestPreviewHeadwear(FName AppearanceId);

	// 검증용 — 셰브론 꼭짓점의 월드 레이어 로컬 좌표와 기준점(겉모양 맨 위 투영). 셰브론이 없으면 (-1,-1,-1,-1)
	UFUNCTION(BlueprintCallable, Category = "Test")
	FVector4 TestGetHeadChevronAnchor() const;

	// 검증용 — 장비 탭 돌리기 상태(JSON): 요 오프셋·기준 요·액터 요·고리/글자/누름 영역(화면 절대)·끄는 중·카메라 위치
	UFUNCTION(BlueprintCallable, Category = "Test")
	FString TestGetRotateState() const;

	// 검증용 — 셰브론 기준점(겉모양 맨 위)의 부품별 기여(JSON, 뷰포트 위젯 좌표): 부품·머리 부품 여부·상자 높이·화면 맨 위
	UFUNCTION(BlueprintCallable, Category = "Test")
	FString TestGetVisualTopParts() const;

	// 같은 절대 좌표계에서 좌측 판 오른쪽 끝~우측 판 왼쪽 끝 사이 빈 공간의 가운데를 뷰포트 가로 비율로
	static bool TryGetGapCenterRatioX(const FSlateRect& ViewportRect, float LeftPanelRight, float RightPanelLeft, float& OutRatio);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// 스택 전이(페이드)가 끝나 실제로 보이는 프레임을 잰다 — 목록 모달 등장 시작 시점
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool NativeOnHandleBackAction() override;

	// 장비 탭 직원 돌리기 — 몸·발밑 고리에서 시작한 누름은 빈칸 닫기 버튼보다 먼저(터널) 잡는다 (OfficeEmployeePanelEquipment.cpp)
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	// CardHost 에 ★ 강화 카드를 띄운다 — 다른 카드(장비 상세)가 떠 있으면 교체한다
	virtual void ShowEnhanceCard();

	// ===== 루트 구성 =====

	// 좌측 사원증 레일 — 오른쪽 끝이 프레이밍 빈 공간의 왼쪽 경계
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> RailPanel;

	// 우측 840 패널 — 왼쪽 끝이 프레이밍 빈 공간의 오른쪽 경계
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> DetailPanel;

	// 월드 앵커 레이어 (장비 칸·지시선·대기 안내 카드). SelfHitTestInvisible
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> WorldLayer;

	// 장비 상세 / ★ 강화 카드가 한 번에 하나 뜨는 자리
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UOverlay> CardHost;

	// 두 판 사이 빈 사무실을 탭하면 닫는다(카드가 떠 있으면 카드만) — WorldLayer 맨 뒤의 투명 버튼
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> GapCloseButton;

	// ===== 레일 =====

	// 「N명」 — 앞의 「직원」 은 WBP 글자
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RosterCountText;

	// 사무실 묶음 → 「대기 N」 머리줄 → 대기 묶음을 한 스크롤로 (목록 위젯 둘을 겹치면 터치 스크롤을 안쪽이 먹는다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> RosterScroll;

	// 사원증 2열 (UIE_EmployeeBadgeCard 를 런타임에 채운다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> SeatedGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> BenchGrid;

	// 「대기 N」 구분줄 — 대기 직원이 없으면 접는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> BenchHeader;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> BenchCountText;

	// 전체 · 직능(있는 것만) · 레벨업 가능 · 대기
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UComboBoxString> FilterCombo;

	// 할 일 순 → 레벨순 → 이름순 순환
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> SortCycleButton;

	// 「모두 레벨업 · N명」 묶음 (버튼 + 위에 얹은 글자·인원 칩). 레벨업 가능 인원이 0 이면 통째로 접는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> BulkLevelUpBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> BulkLevelUpButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> BulkLevelUpCountText;

	// ===== 패널 헤더 =====

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	// 직급 (레벨 → 직급 표시명)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RankText;

	// 주 직능 표시명 (DT_DisciplineDisplay). 직능이 없으면 접는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DeptText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StarText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DeptIcon;

	// 직능 앞 구분점 — 직능이 접힐 때 같이 접어야 구분점이 겹쳐 남지 않는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DeptSeparator;

	// 기본 헤더에는 ‹ › 가 없다(작아서 누르기 어렵고 직원 전환은 레일이 맡는다) — 두는 WBP 에서만 연결된다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> PrevButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> NextButton;

	// UIE_CloseButton (UUserWidget 파생 — 클릭은 OnCloseClicked)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	// ===== 탭 =====

	// UWidget 인 이유 = UIE_TabButton(UTabButtonWidget, 건물 관리 패널 폴더 탭)과 단일 버튼을 둘 다 받는다 ― 그룹에는 폴더 탭이면 GetButton() 을 넣는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> TabGrowthButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> TabPotentialButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> TabEquipmentButton;

	// 자식 순서 = 탭 인덱스 (0 성장 / 1 잠재능력 / 2 장비)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> GrowthPage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> PotentialPage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> EquipmentPage;

	// ===== 성장 탭 =====

	// 레벨업 카드 — 자체 구독(개인 경험치·레벨업), SP 변화는 패널이 다시 Configure 한다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEmployeeLevelUpCardWidget> LevelUpCard;

	// 레벨업 카드 · 「기본 능력치」 머리 · 능력치 6행(런타임 생성)이 함께 스크롤된다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> StatScroll;

	// 「보유 ● N」 칩의 숫자
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatPointText;

	// ===== 잠재능력 탭 =====

	// 공용 잠재 워크스페이스(EWidgetType::PotentialResetWorkspace)를 런타임에 채우는 자리
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UOverlay> PotentialHost;

	// ===== 장비 탭 =====

	// 머리 줄 — 부위 실루엣 · 부위명 · 「장착 중 · 장갑 C 등급」
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> EquipPartIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipPartNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipPartStateText;

	// 「전체」일 때 머리 줄 실루엣 자리에 여섯 부위 묶음
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquipAllPartsGlyph;

	// 부위 칸 줄 — 「전체」 + 여섯 부위 (EquipPartChipButton/On/Well/Text0..6 · EquipPartChipGlyph1..6, 0 = 전체)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> EquipPartChipRow;

	// 「보유 장비 N개」 — 지금 거르기(부위·등급)로 보이는 장비 수
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipOwnedCountText;

	// 공용 보유 목록(UIE_OfficeEquipmentList — 부위 콤보는 접힘, 부위는 칸이 고른다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEmployeeEquipmentListWidget> EquipmentList;

	// 「보유한 {부위} 장비가 없습니다」 + [장비 뽑기]
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> EquipEmptyNotice;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipEmptyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> EquipGachaButton;

	// 대기 직원(액터 없음) — 월드 칸 대신 패널 머리 위 6칸 한 줄 (BenchSlot0..5 · BenchSlotLabel0..5 · BenchSlotRing0..5)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> BenchSlotRow;

	// 월드 레이어: 칸 6개(WorldSlotBox0..5 > WorldSlot0..5 · WorldSlotLabel0..5 · WorldSlotState0..5 · WorldSlotRing0..5) + 지시선
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEquipmentCalloutLinesWidget> EquipCalloutLines;

	// 장착 순간 머리 위 외곽선 글자 「{장비명} 장착」 + 효과 한 줄
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> EquipToast;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipToastTitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EquipToastEffectText;

	// 발밑 「끌어서 돌리기」 고리 — WorldLayer 캔버스(정렬 0.5/0.5), 위치·크기는 매 틱 발 투영점에서
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEmployeeRotateRingWidget> RotateRing;

	// 고리 아래 「끌어서 돌리기」 (정렬 0.5/0)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RotateCaption;

	// ===== 푸터 =====

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> FireButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> EnhanceButton;

	// 해고 · ★ 강화 줄 — 성장 탭에서만 보이고, 다른 탭에서는 접혀 그 높이를 탭 내용이 쓴다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> FooterRow;

	// ===== 대기(자리 없는) 직원 안내 =====

	// 「사무실 밖에서 대기 중인 직원입니다」 카드 — 문구는 WBP
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> BenchNotice;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> BenchSeatButton;

	// ===== 지갑 (UIE_Resource 재사용, 종류는 WBP 인스턴스가 지정) =====

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UResourceWidget> WalletMoneyChip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UResourceWidget> WalletDiamondChip;

private:
	UPROPERTY()
	TObjectPtr<UEmployeeManager> EmployeeManager;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> TabButtonGroup;

	int32 PendingOpenEmployeeId = INDEX_NONE;
	int32 SelectedEmployeeId = INDEX_NONE;
	// 해고 직후 로스터가 다시 만들어질 때 고를 이웃 직원
	int32 PendingSelectAfterFireId = INDEX_NONE;
	// 현재 레일 표시 순서 (‹ › 순환 기준, 사무실 → 대기)
	TArray<int32> RosterOrder;

	// push 직후 OpenForEmployee 가 같은 프레임에 오므로 기본 선택은 한 틱 미룬다 — 먼저 고르면 첫 직원이 섰다 앉는다
	bool bNeedsDefaultSelection = false;

	// 판 지오메트리가 이번 프레임에 아직 없으면 다음 틱에 다시 잰다
	bool bCameraFramingPending = false;
	int32 CameraFramingAttempts = 0;
	// 대기 직원 전경도 두 판 사이 빈칸 가운데로 — 판 지오메트리가 잡힐 때까지 틱마다 다시 잰다
	bool bBenchLensShiftPending = false;
	int32 BenchLensShiftAttempts = 0;
	// 열린 채 창 크기가 바뀌면 다시 잡는다. 판 지오메트리는 레이아웃 한 바퀴 뒤에 새 크기가 되므로 몇 틱 미룬다
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	int32 ViewportReframeDelayTicks = 0;
	// 왼쪽 목록 모달은 패널을 열 때만 왼쪽에서 미끄러져 들어온다 (직원·탭 전환·정렬에는 안 탄다)
	bool bRailSlidingIn = false;
	float RailSlideElapsed = 0.f;
	// 스택 페이드·패널 등장이 끝나 패널이 다 보일 때 시작한다 — 그 전엔 화면 밖에서 기다린다
	bool bRailSlideStarted = false;
	float RailSlideWaitElapsed = 0.f;
	mutable float LastPaintedOpacity = 0.f;

	// 표시 순서대로 (EEmployeeStatIndex 순서와 다르다)
	UPROPERTY()
	TArray<TObjectPtr<UOfficeEmployeeStatRowWidget>> StatRows;

	FOfficeCameraViewState SavedCameraView;
	FOfficeEmployeeInspect Inspect;
	TWeakObjectPtr<AOfficeworker> FocusWorker;
	FTimerHandle SeatRefocusTimer;

	// 고른 직원 머리 위 이중 V — 건물 선택 셰브론(USelectionChevronWidget) 재사용, WorldLayer 캔버스에 런타임 생성
	UPROPERTY()
	TObjectPtr<USelectionChevronWidget> HeadChevron;
	// 같은 셰브론을 짙은 색으로 한 겹 아래 — 흰 머리·밝은 벽 위에서도 읽히게 (목업 .selchev 의 짙은 밑 획)
	UPROPERTY()
	TObjectPtr<USelectionChevronWidget> HeadChevronShadow;
	TWeakObjectPtr<AOfficeworker> HeadChevronWorker;
	float HeadChevronElapsed = 0.f;
	// 마지막으로 잰 겉모양 맨 위의 월드 레이어 로컬 좌표 (셰브론 꼭짓점 = 이 점 위 HeadChevronGapUnits)
	FVector2D HeadChevronTopLocal = FVector2D::ZeroVector;

	void TickHeadChevron(float DeltaTime);
	void HideHeadChevron();
	// 직원 겉모양(몸 + 모자 등 메시 부품) 경계 상자의 화면 맨 위(뷰포트 위젯 좌표, 가로 = 머리) — 큰 모자에도 셰브론이 겹치지 않게
	static bool GetWorkerVisualTop(APlayerController* PC, const AOfficeworker* Worker, FVector2D& OutViewportTop,
		TArray<FString>* OutParts = nullptr);
	void SyncFooterForTab();

	void RefreshRoster();
	void RefreshHeader();
	void RefreshWallet();
	void SyncRosterSelection(bool bScrollIntoView = false);

	// ===== 사원증 레일 (OfficeEmployeePanelRoster.cpp) =====

	TArray<OfficeEmployeeRoster::FFilterOption> RosterFilterOptions;
	OfficeEmployeeRoster::FFilterOption ActiveRosterFilter;
	OfficeEmployeeRoster::ESortMode RosterSortMode = OfficeEmployeeRoster::ESortMode::Todo;
	// 레벨업 가능 집합이 바뀔 때만 레일을 다시 채운다 (개인 경험치 신호는 잦다)
	TSet<int32> RosterReadyIds;
	// 모두 레벨업 도중의 레벨업 신호마다 레일을 다시 채우지 않는다 — 끝에서 한 번
	bool bBulkLevelingUp = false;

	// 카드 i 는 늘 칸 (i/2, i%2) — 풀 순서를 고정해 갱신 때 꼬리만 붙이고 뗀다
	UPROPERTY()
	TArray<TObjectPtr<UEmployeeBadgeCardWidget>> SeatedBadges;

	UPROPERTY()
	TArray<TObjectPtr<UEmployeeBadgeCardWidget>> BenchBadges;

	void BindRosterControls();
	void UnbindRosterControls();
	void ResetRosterView();
	void GatherRoster(TArray<FEmployeeInstance>& OutSeated, TArray<FEmployeeInstance>& OutBench) const;
	void RebuildFilterOptions(const TArray<FEmployeeInstance>& Seated, const TArray<FEmployeeInstance>& Bench);
	bool PassesRosterFilter(const FEmployeeInstance& Employee, bool bOnBench) const;
	void SortRosterForDisplay(TArray<FEmployeeInstance>& InOutRoster) const;
	void FillBadgeGrid(UUniformGridPanel* Grid, TArray<TObjectPtr<UEmployeeBadgeCardWidget>>& Pool,
		const TArray<FEmployeeInstance>& Employees, bool bOnBench);
	void RefreshBulkLevelUp(const TArray<FEmployeeInstance>& Seated);
	void UpdateSortLabel();
	TSet<int32> CollectReadyIds() const;
	void HandleSortCycleClicked();
	void HandleBulkLevelUpClicked();
	void HandleEmployeePortraitUpdated(int32 EmployeeId, int64 Revision);

	UFUNCTION()
	void HandleFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	void ApplyWorldFocus();
	void ShowBenchView();
	void TryApplyCameraFraming();
	void TryApplyBenchLensShift();
	void TickViewportReframe();
	void BeginRailSlideIn();
	void TickRailSlideIn(float InDeltaTime, const FVector2D& PanelOffsetThisFrame);
	void EndRailSlideIn();
	bool TryComputeFramingCenterRatioX(float& OutRatio);
	void CycleSelection(int32 Direction);
	void EndWorldFocus();

	// 성장 탭 (OfficeEmployeePanelGrowth.cpp)
	void BuildGrowthRows();
	void RefreshGrowth();

	// 잠재능력 탭 (OfficeEmployeePanelPotential.cpp)
	UPROPERTY()
	TObjectPtr<UPotentialResetWorkspaceWidget> PotentialWorkspace;

	// 뷰포트 오버레이(Z120)라 스택이 거둬 가지 않는다 — 탭을 떠나거나 닫을 때 직접 걷는다
	TWeakObjectPtr<UPotentialOddsWidget> OpenOddsPanel;

	bool IsPotentialTabShown() const;
	void SyncPotentialWorkspace();
	void SuspendPotentialWorkspace();
	void OpenOddsTable(int32 EmployeeId);
	bool CloseOddsTable();
	void HandlePotentialShopRequested();

	// ★ 강화 카드 (OfficeEmployeePanelEnhance.cpp) — CardHost 의 자식, 활성화하지 않는다
	UPROPERTY()
	TObjectPtr<UEnhanceStarforceModalWidget> EnhanceCard;

	bool IsEnhanceCardShown() const;
	void SyncEnhanceCard();
	bool CloseEnhanceCard();

	// ===== 장비 탭 (OfficeEmployeePanelEquipment.cpp) =====

	EEmployeeEquipmentSlot EquipSelectedPart = EEmployeeEquipmentSlot::Head;
	// 「전체」 = 부위 미선택 (칸 링·곡선 없음, 목록 전체). 장비 탭에 들어올 때마다 「전체」로 시작한다
	bool bEquipAllParts = true;
	bool bEquipTabEntered = false;

	// 칸은 EEmployeeEquipmentSlot 순서. 이름 규약으로 찾는다 (NativeConstruct)
	UPROPERTY()
	TArray<TObjectPtr<UIconWithButtonWidget>> WorldSlotButtons;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> WorldSlotBoxes;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> WorldSlotLabels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> WorldSlotStates;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> WorldSlotRings;

	// 장착된 칸 — 등급 옅은 면 + 등급 테(WorldSlotFace) 위에 장비 그림을 칸 가득(WorldSlotArt)
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> WorldSlotEquipped;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> WorldSlotFaces;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> WorldSlotArts;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> EquipChipButtons;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> EquipChipOn;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> EquipChipTexts;

	// 켜진 칸의 흰 우물 — 실루엣은 우물 위에 저작 잉크 그대로 남는다
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> EquipChipWells;

	// 칩 글자의 저작 잉크 (켜진 칩은 흰색)
	FSlateColor EquipChipInk;
	bool bEquipChipInkCaptured = false;

	UPROPERTY()
	TArray<TObjectPtr<UIconWithButtonWidget>> BenchSlotButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> BenchSlotLabels;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> BenchSlotRings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WorldSlotIconMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BenchSlotIconMaterials;

	// 장비 상세 카드 — CardHost 의 자식(캔버스 루트라 desired 0 → 높이를 내용 desired 로 맞추는 상자에 담는다)
	UPROPERTY()
	TObjectPtr<UEquipmentDetailWindowWidget> EquipDetailCard;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> EquipDetailCardBox;

	// 상세 카드 틀(판 · 안쪽 여백 · 오른쪽 위 X) — 저작 자리에서 떼어 두었다가 CardHost 에 붙인다. 공용 상세 창 WBP 는 그대로
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> EquipDetailFrame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> EquipDetailCloseButton;

	// 패널에 띄운 공용 상세 창 인스턴스에서 접을 위젯 — 직원창 칼럼용 구역 제목·자체 닫기·자체 판 (공용 WBP 는 그대로)
	UPROPERTY(EditDefaultsOnly, Category = "Equipment Detail")
	TArray<FName> EquipDetailCollapsedWidgets = { TEXT("DetailTitleText"), TEXT("DetailCloseButton") };

	// 머리 줄(그림·이름·등급 칩)이 틀 오른쪽 위 X 밑으로 들어가지 않게 그 위젯 슬롯 오른쪽을 이만큼 비운다
	UPROPERTY(EditDefaultsOnly, Category = "Equipment Detail")
	FName EquipDetailHeaderWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Equipment Detail")
	float EquipDetailHeaderReserve = 0.f;

	FGuid EquipDetailInstanceId;

	// 장비 강화 다이얼로그 (뷰포트 Z120 — 스택이 거둬 가지 않으므로 직접 걷는다)
	UPROPERTY()
	TObjectPtr<UEmployeeEquipmentDialogWidget> EquipDialog;

	FEmployeeEquipmentRequest EquipPendingRequest;
	FGuid EquipDialogInstanceId;

	// 미리 입혀 보기 중인 직원 액터 — 카드가 닫히거나 직원이 바뀌면 이 액터에서 걷는다
	TWeakObjectPtr<AFunnyOfficeworker> EquipPreviewWorker;

	// 장착 순간 연출 (칸·지시선 숨김 + 빛 고리 + 머리 위 글자)
	bool bEquipMomentActive = false;
	float EquipMomentElapsed = 0.f;
	EEmployeeEquipmentSlot EquipMomentPart = EEmployeeEquipmentSlot::Head;

	// 장착 실행 중 목록 Refresh 가 같은 항목을 다시 골라 카드를 다시 여는 것을 막는다
	bool bEquipCommitting = false;
	// 다른 직원 장비를 가져오기로 확인한 뒤 같은 처리기를 다시 탈 때만 켠다
	bool bTransferConfirmed = false;

	// 마지막으로 계산한 월드 배치 (디버그 덤프용, 월드 레이어 로컬)
	TArray<FVector2D> LastWorldSlotCenters;
	TArray<FVector2D> LastWorldAnchors;
	// 고른 부위 곡선 (시작·조절점·끝·앵커, 월드 레이어 로컬). 「전체」면 비어 있다
	TArray<FVector2D> LastCurvePoints;
	bool bWorldSlotsShown = false;

	// ===== 장비 탭 직원 돌리기 — 포인터 하나만 (선례: 직원창 BeginHeroRotation) =====
	int32 RotatePointerIndex = INDEX_NONE;
	int32 RotateUserIndex = INDEX_NONE;
	bool bRotateTouch = false;
	// 끌기 문턱을 넘었다 — 뗄 때 탭(빈칸 닫기)으로 치지 않는다
	bool bRotateDragging = false;
	FVector2D RotatePressPosition = FVector2D::ZeroVector;
	FVector2D RotateLastPosition = FVector2D::ZeroVector;
	// 살펴보기 시작 때의 요(카메라를 보는 정면)와 그 위에 더한 끌기 회전
	TWeakObjectPtr<AOfficeworker> RotateWorker;
	float RotateBaseYaw = 0.f;
	float RotateYawOffset = 0.f;
	// 누르면 돌리기가 시작되는 영역 = 몸 + 발밑 고리 (월드 레이어 로컬, 고리가 보일 때만 유효)
	FSlateRect RotateHitRectLocal = FSlateRect(0.f, 0.f, 0.f, 0.f);
	bool bRotateHintPlaced = false;

	bool CanRotateWorker() const;
	bool IsRotatePointer(const FPointerEvent& PointerEvent) const;
	FReply BeginRotateDrag(const FPointerEvent& PointerEvent);
	FReply MoveRotateDrag(const FPointerEvent& PointerEvent);
	FReply EndRotateDrag(const FPointerEvent& PointerEvent);
	// 잡은 포인터만 놓는다(각도는 유지)
	void CancelRotateDrag();
	// 살펴보기 정면으로 되돌린다 — 직원·탭 전환, 닫기
	void ResetWorkerRotation();
	void CaptureRotateBase(AOfficeworker* Worker);
	void SetRotateHintShown(bool bShown);
	// 발 아래 바닥점·고리 폭을 투영한다 (월드 레이어 로컬). 실패하면 false
	bool ComputeRotateHint(AOfficeworker* Worker, const FGeometry& LayerGeo, FVector2D& OutCenter, FVector2D& OutSize) const;

	void BindEquipmentControls();
	void UnbindEquipmentControls();
	bool IsEquipmentTabShown() const;
	void SyncEquipmentTab(bool bEmployeeChanged);
	void RefreshEquipmentTab();
	void RefreshEquipHeader();
	void RefreshEquipSlots();
	void TickEquipmentWorld(float DeltaTime);
	void SetWorldSlotsShown(bool bShown);
	bool ComputeEquipAnchors(AOfficeworker* Worker, TArray<FVector2D>& OutAbsolute, FVector2D& OutHeadTopAbsolute, FVector2D& OutCenterAbsolute) const;
	void SelectEquipPart(EEmployeeEquipmentSlot Part);
	void SelectAllEquipParts();
	// 0 = 전체, 1..6 = EEmployeeEquipmentSlot + 1
	void SelectEquipChip(int32 ChipIndex);
	void RefreshEquipChips();
	void RefreshEquipOwnedCount();
	void HandleEquipSlotClicked(int32 SlotIndex);
	void HandleEquipItemPicked(const FGuid& InstanceId);
	void ShowEquipDetail(const FGuid& InstanceId);
	bool IsEquipDetailShown() const;
	bool CloseEquipDetail();
	void SyncEquipDetailHeight();
	void ClearEquipPreview();
	void HandleEquipDetailPrimary();
	void HandleEquipDetailSecondary();
	void ShowEquipEnhanceDialog(EEquipmentMaterialMode Mode);
	void ExecuteEquipEnhance();
	bool CloseEquipDialog();
	void BeginEquipMoment(EEmployeeEquipmentSlot Part, const FText& Title, const FText& Effect);
	void EndEquipMoment();
	void HandleEquipmentChanged();
	void HandleEquipGachaClicked();

	// 닫기 먼저 → PromptStack push → 그 창이 닫히면 같은 직원·같은 탭으로 패널을 다시 연다 (상점·장비 뽑기 왕복)
	void PushPromptAndReturn(UClass* PromptClass, TFunction<void(UCommonActivatableWidget&)> InitPrompt, int32 ReturnTabIndex);

	AOfficeworker* FindWorkerActor(int32 EmployeeId) const;
	AOfficeCameraPawn* GetOfficeCamera() const;
	void SetOfficeHUDSuppressed(bool bSuppressed);

	// ===== 핸들러 =====

	void HandlePrevClicked();
	void HandleNextClicked();
	void HandleFireClicked();
	void HandleFireConfirmed(int32 EmployeeId);
	void HandleEnhanceClicked();
	void HandleBenchSeatClicked();
	void HandleRosterChanged();
	void HandleEmployeeStatsChanged(int32 EmployeeId);
	void HandleEmployeeLevelUp(int32 EmployeeId, int32 NewLevel);
	void HandleEmployeeExperienceChanged(int32 EmployeeID);
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	UFUNCTION()
	void HandleRosterCardClicked(int32 EmployeeId);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleGapClicked();

	UFUNCTION()
	void HandleTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void HandleEquipDetailCloseClicked();

	// 부위 칩 — 버튼 OnClicked 는 인자를 못 실어 칩마다 한 줄
	UFUNCTION() void HandleEquipChip0() { SelectEquipChip(0); }
	UFUNCTION() void HandleEquipChip1() { SelectEquipChip(1); }
	UFUNCTION() void HandleEquipChip2() { SelectEquipChip(2); }
	UFUNCTION() void HandleEquipChip3() { SelectEquipChip(3); }
	UFUNCTION() void HandleEquipChip4() { SelectEquipChip(4); }
	UFUNCTION() void HandleEquipChip5() { SelectEquipChip(5); }
	UFUNCTION() void HandleEquipChip6() { SelectEquipChip(6); }
};
