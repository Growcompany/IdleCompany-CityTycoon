#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/EmployeeEquipmentData.h"
#include "Types/SlateEnums.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "EmployeeEquipmentWorkspaceWidget.generated.h"

class UButtonWidget;
class UIconWithButtonWidget;
class UGachaBannerButton;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UCommonButtonStyle;
class UComboBoxString;
class UTileView;
class UImage;
class UHorizontalBox;
class UOverlay;
class UProgressBar;
class UCommonTextBlock;
class UEmployeeEquipmentDialogWidget;
class UConfirmCancelWidget;
class UEquipmentDetailWindowWidget;
class UEmployeeEquipmentItemObject;
class UEmployeeEquipmentCardWidget;
class UEmployeeEquipmentManagerSubsystem;
class UTableManagerSubsystem;
class AGachaCaptureStage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UGachaRevealPresentationWidget;
struct FEmployeeEquipmentDefinitionRow;
struct FEmployeeEquipmentDialogView;
struct FGachaRevealAwardContext;

enum class EEquipmentWorkspaceView : uint8 { Employee, Inventory, Gacha };

// 정렬 순환 버튼이 클릭마다 이 순서로 돈다 — 직원 로스터의 ERosterSortMode 와 같은 문법이다.
enum class EEquipmentSortMode : uint8
{
	Grade,     // 등급순 (기본)
	Enhance,   // 강화순
	Name,      // 이름순 (가나다)
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentWorkspaceWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	// 튜토리얼이 장비 뽑기 결과 연출이 떠 있는지 보려고 읽는다
	UGachaRevealPresentationWidget* GetRevealPresentation() const;
	void Configure(int32 EmployeeId, EEquipmentWorkspaceView View = EEquipmentWorkspaceView::Employee);
	// 직원창이 소유하는 선택 직원을 유지하며, 이 화면에서는 직원을 다시 고르지 않는다.
	void ConfigureForEmployee(int32 EmployeeId, EEquipmentWorkspaceView View);
	// 드로어는 직원 홈의 초상화에 시착을 표시하고 중복 직원 영역을 숨긴다.
	void SetDrawerPresentation(UImage* PortraitTarget);
	bool IsDrawerPresentation() const { return DrawerPortraitTarget.IsValid() && CurrentView != EEquipmentWorkspaceView::Gacha; }
	void ConfigureGachaHost(int32 EmployeeId = INDEX_NONE);
	bool IsGachaHost() const { return bGachaHost; }
	bool IsReturningFromGacha() const { return bReturningFromGacha; }
	void OpenEquipmentItem(FGuid InstanceId);
	void OpenSlot(EEmployeeEquipmentSlot EquipmentSlot);
	void OpenAllEquipment();
	FSimpleDelegate OnViewChanged;
	EEquipmentWorkspaceView GetCurrentView() const { return CurrentView; }
	void Resume();
	bool IsReturningFromShop() const { return bReturningFromShop; }
	void DismissDialog();
	bool HandleBack();
	void CloseInspector();
	void RefreshPortrait();
	void SuspendPreview();
	int32 GetEmployeeId() const { return CurrentEmployeeId; }
	// 튜토리얼 앵커 — 뽑기 뷰일 때만 값을 준다(다른 뷰에선 GachaPane 이 접혀 버튼이 화면에 없다)
	UWidget* GetPullOneButtonWidget() const;
	UWidget* GetNormalTicketButtonWidget() const;
	bool IsNormalTicketSelected() const { return CurrentTicketType == EItemType::EmployeeEquipmentTicketNormal; }
	DECLARE_DELEGATE_OneParam(FOnEquipmentEmployeeSelected, int32);
	FOnEquipmentEmployeeSelected OnEmployeeSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTileView> InventoryView;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> EmployeePortrait;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> EmployeeNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> EmployeeCombo;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> EmployeeSelectorRow;
	// ⚠ 화면에서는 안 보이지만 위젯은 지우지 말 것 — 검증 하네스가 이 둘을 널 체크 없이 역참조한다
	// (EmployeeEquipmentUIValidation 6곳 · EquipmentTicketUIValidation 2곳). 부위는 직원 카드의
	// 장비 슬롯(HeroEquipmentSlot0..5 -> OpenSlot)이 고르고 등급은 타일 면 색과 정렬이 대신하므로,
	// 드로어에서는 WBP 가 Collapsed 로 접어 둔다. 데이터 경로(CurrentSlot/GradeFilter)는 그대로다.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> SlotFilterCombo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> GradeFilterCombo;
	// 정렬 순환 버튼 — WBP 가 아직 없을 수 있어 Optional 로 둔다. 판 헤더 띠 저작이 끝나면 required 로 올릴 것.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> SortCycleButton;
	// 드로어 부위 필터 세로 레일 — 드롭다운을 대신한다. All 이 0, 나머지가 EEmployeeEquipmentSlot 순서다.
	// 캐릭터 패널의 SlotButton0..5 는 같은 HandleSlot 을 쓰지만 다른 패널에 있어 그룹을 공유하지 않는다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilterAll;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter0;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter1;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter2;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter3;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter4;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter5;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> EmployeePane;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> CollectionPane;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> GachaPane;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WorkspaceToolbar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UOverlay> WorkspaceTopUtilityHost;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UOverlay> WorkspaceBottomUtilityHost;
	// 목록 띠 왼쪽의 부위명. 개수(InventoryStatusText)와 나눠 둔 이유는 라벨은 Fill, 개수는 Auto 라야
	// 긴 부위명이 개수를 밀어내지 않기 때문이다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> InventoryPartLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> InventoryStatusText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> BalanceText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> GachaButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> ShopButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> GachaShopButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> BulkButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> CurrentItemButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> EffectsButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> EnhanceButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> InspectorPane;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> InspectorContent;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> LegacyEmployeeActions;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> InspectorImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> InspectorGradeFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> InspectorNameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> InspectorGradeText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> InspectorBodyText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> PreviewLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> InspectorEquipButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> InspectorEnhanceButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> InspectorDetailButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SlotButton0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SlotButton1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SlotButton2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SlotButton3;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SlotButton4;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SlotButton5;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UGachaBannerButton> TicketNormalButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UGachaBannerButton> TicketAdvancedButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UGachaBannerButton> TicketPremiumButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> GachaTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> EpicPityText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> LegendaryPityText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> AscendantPityText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> EpicPityProgress;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> LegendaryPityProgress;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> AscendantPityProgress;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> GachaCostText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> GachaImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> GachaOddsRow;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UIconWithButtonWidget> PullOneButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UIconWithButtonWidget> PullTenButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UIconWithButtonWidget> PullThirtyButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> OddsButton;
	UPROPERTY(EditDefaultsOnly, Category = "Drawer") TSubclassOf<UCommonButtonStyle> DrawerUtilityStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Drawer") TSubclassOf<UCommonButtonStyle> DrawerEquipStyle;
	// 상세 창이 드로어 왼쪽에 붙을 때 화면 우변에서 띄울 거리(드로어 폭 + 창 사이 간격).
	UPROPERTY(EditDefaultsOnly, Category = "Drawer") float DrawerDetailDockOffset = 880.f;

private:
	friend struct FEquipmentGachaPanelValidation;
	bool bGachaHost = false;
	bool bReturningFromGacha = false;
	friend struct FEquipmentThirtyRevealValidation;
	bool bEmployeeContextLocked = false;
	TWeakObjectPtr<UImage> DrawerPortraitTarget;
	UImage* GetPreviewImage() const;
	void RefreshPresentation();
	UFUNCTION() UWidget* GenerateEquipmentFilterLabel(FString Option);
	// 칩 재질은 Wpx/Hpx 를 받아야 코너가 안 뭉갠다 — 로스터 필터와 같은 계약.
	void UpdateFilterChipMaterialSize();
	FVector2D LastSlotChipSize = FVector2D::ZeroVector;
	FVector2D LastGradeChipSize = FVector2D::ZeroVector;
	bool bPresentationInitialized = false;
	bool bCompactPresentation = false;
	UPROPERTY() TArray<TSubclassOf<UCommonButtonStyle>> NormalPresentationStyles;
	UPROPERTY() TArray<FSlateFontInfo> NormalPresentationFonts;
	TArray<FSlateColor> NormalPresentationColors;
	friend struct FEmployeeEquipmentUIValidation;
	friend struct FEquipmentTicketUIValidation;
	UPROPERTY() TObjectPtr<UCommonButtonGroupBase> TicketButtonGroup;
	UPROPERTY() TObjectPtr<UCommonButtonGroupBase> SlotButtonGroup;
	UPROPERTY() TObjectPtr<UCommonButtonGroupBase> PartFilterGroup;
	UPROPERTY() TObjectPtr<UEmployeeEquipmentManagerSubsystem> EquipmentManager;
	UPROPERTY() TObjectPtr<UTableManagerSubsystem> TableManager;
	UPROPERTY() TObjectPtr<UEmployeeEquipmentDialogWidget> Dialog;
	TWeakObjectPtr<UConfirmCancelWidget> PullNotice;
	// 드로어에서는 선택 상세를 목록 아래가 아니라 드로어 왼쪽의 독립 창으로 띄운다.
	UPROPERTY() TObjectPtr<UEquipmentDetailWindowWidget> DetailWindow;
	TWeakObjectPtr<UPanelWidget> DetailWindowHost;
	void RefreshDetailWindow(const struct FEquipmentDetailContent& Content);
	void DismissDetailWindow();
	UPROPERTY() TObjectPtr<UGachaRevealPresentationWidget> RevealPresentation;
	UPROPERTY() TArray<TObjectPtr<UEmployeeEquipmentItemObject>> Entries;
	UPROPERTY() TArray<TObjectPtr<UEmployeeEquipmentItemObject>> DialogEntries;
	UPROPERTY() TObjectPtr<AGachaCaptureStage> PreviewStage;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PreviewMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GachaIconMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> InspectorIconMaterial;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SlotIconMaterials;
	UPROPERTY(EditDefaultsOnly, Category = "Portrait") TSoftClassPtr<AGachaCaptureStage> PreviewStageClass;
	UPROPERTY(EditDefaultsOnly, Category = "Portrait") TSoftObjectPtr<UMaterialInterface> PreviewMaterial;
	int32 PreviewEmployeeId = INDEX_NONE;
	int64 PreviewRevision = -1;
	bool bPreviewEnabled = false;
	float PreviewWarmup = -1.f;
	int32 CurrentEmployeeId = INDEX_NONE;
	EEquipmentWorkspaceView CurrentView = EEquipmentWorkspaceView::Employee;
	EEquipmentWorkspaceView EmployeeReturnView = EEquipmentWorkspaceView::Employee;
	EEmployeeEquipmentSlot CurrentSlot = EEmployeeEquipmentSlot::Head;
	EEmployeeEquipmentGrade GradeFilter = EEmployeeEquipmentGrade::Count;
	EEquipmentSortMode SortMode = EEquipmentSortMode::Grade;
	bool bAllSlots = true;
	bool bUpdating = false;
	bool bReturnToResults = false;
	bool bResultsRevealPending = false;
	bool bBulkDialog = false;
	bool bReturningFromShop = false;
	EItemType CurrentTicketType = EItemType::EmployeeEquipmentTicketNormal;
	int32 RequestedPullCount = 1;
	FName CurrentBannerId;
	FGuid SelectedInstanceId;
	FGuid PendingSelectionPulseId;
	FName SelectedDefinitionId;
	FText InspectorFailureNotice;
	FEmployeeEquipmentRequest PendingRequest;
	TArray<FGuid> LastResults;
	TArray<FGuid> BulkSelectionIds;
	TArray<int32> EmployeeOptionIds;

	void Refresh();
	void RefreshInventory();
	void UpdateSortLabel();
	// CommonButtonBase 네이티브 OnClicked — AddUObject 로 바인딩하므로 UFUNCTION 이 필요 없다.
	void OnSortCycleClicked();
	bool IsInspectorActive() const;
	void SelectInspectionItem(FGuid InstanceId);
	void ClearInspectionSelection(bool bRestorePreview = true);
	void RefreshInspector();
	bool BuildItemDetailView(FGuid InstanceId, FEmployeeEquipmentDialogView& View, bool bChangesOnly = false) const;
	void HandleInspectorEquip();
	void HandleInspectorEnhance();
	void HandleInspectorDetail();
	void RefreshGacha();
	void AcquirePreview();
	bool PreviewDefinition(FName DefinitionId);
	void SetupFilters();
	void SetupBanners();
	void RefreshTickets();
	void HandleTicketItemsChanged(EItemType Item, int32 Count, int32 Delta);
	UFUNCTION() void HandleTicketSelected(UCommonButtonBase* Button, int32 Index);
	UFUNCTION() void HandleSlotButtonSelected(UCommonButtonBase* Button, int32 Index);
	UFUNCTION() void HandlePartFilterSelected(UCommonButtonBase* Button, int32 Index);
	// 부위가 코드에서 바뀌어도(슬롯 클릭·인스펙터 복귀) 레일 선택이 따라와야 한다.
	void SyncPartFilterSelection();
	UEmployeeEquipmentItemObject* MakeEntry(const FEmployeeEquipmentDefinitionRow& Definition, const FEmployeeEquipmentInstance* Instance = nullptr);
	void ShowDialog(const FEmployeeEquipmentDialogView& View);
	void HandleDialogBack();
	void ShowDetail(FGuid InstanceId);
	void ShowDefinition(FName DefinitionId);
	void FillDefinitionView(FEmployeeEquipmentDialogView& View, const FEmployeeEquipmentDefinitionRow& Definition, int32 Level) const;
	void ShowEnhance(EEquipmentMaterialMode Mode = EEquipmentMaterialMode::Core);
	void ShowDismantle();
	void ShowResults();
	void ShowResultReveal(const FGachaRevealAwardContext& Award, EItemType SourceTicket, int32 EmployeeId);
	void HandleRevealClosed();
	void ShowBulk();
	void ShowBulkConfirm();
	void UpdateBulkSelection();
	void ShowPull(int32 Count);
	void ShowPullQuote(const FEmployeeEquipmentPullQuote& Quote, int32 Count);
	void HandleStorageForCapacity();
	void ExecutePending();
	void EquipSelected();
	void UnequipSelected();
	void ToggleSelectedLock();
	// 외형은 별도 목록이 아니라 '지금 입고 있는 것의 겉모습' 이다 — 상세 창에서 이 부위의
	// 해금된 외형을 골라 적용한다(결정 §12.7 ④, 외형 목록/페이지 폐지).
	void ShowAppearancePicker();
	UFUNCTION() void HandleAppearanceChosen(UObject* Item);
	void ApplyAppearance();
	void ApplyEquipmentAppearance();
	void HideAppearance();
	void ShowOdds();
	void ShowEffects();
	void HandleItemClicked(UObject* Item);
	void HandleInventoryEntryGenerated(UUserWidget& EntryWidget);
	void TryConsumeSelectionPulse(UEmployeeEquipmentCardWidget* Card);
	void HandleResultClicked(UObject* Item);
	void HandleEquipmentChanged();
	void HandlePortraitChanged(int32 EmployeeId, int64 Revision);
	void HandleStorage();
	void HandleGacha();
	void HandleShop();
	void HandleCurrentItem();
	void HandleEnhance();
	void HandleEquippedEnhance();
	void HandleSlot(int32 Index);
	void HandlePullOne();
	void HandlePullTen();
	void HandlePullThirty();
	UFUNCTION() void HandleEmployeeSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleSlotSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleGradeSelected(FString Item, ESelectInfo::Type Type);
};
