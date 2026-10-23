#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "EmployeeEquipmentDialogWidget.generated.h"

class UCommonTextBlock;
class UButtonWidget;
class UCloseButtonWidget;
class UImage;
class UTileView;
class UWidgetSwitcher;
class UScrollBox;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** 능력치 변화 한 줄 — 상세 창이 표로 그린다. 문자열로 조립된 Comparison 은 구 모달이 계속 쓴다. */
struct FEquipmentDeltaRow
{
	// Before 가 비면 절대값 한 칸짜리 행이다 — 이미 장착한 장비의 "장비 효과" 표가 그 꼴이다.
	FText Name, Before, After;
	// 원본 값은 혜택량이다. 표시값이 감소 방향이어도 원본 혜택량 증가를 초록색으로 그린다.
	bool bUp = true;
};

/** 교체로 잃거나 얻는 고유효과 — 상세 창의 칩. */
struct FEquipmentUniqueChange
{
	FText Name;
	bool bGained = true;
};

struct FEmployeeEquipmentDialogView
{
	FText Title, CurrentHeader, SelectedHeader, Effects, Comparison, Description, Notice;
	// 교체 비교(이전→이후) 와 장착 중 절대값은 서로 다른 표다 — 한 배열에 섞으면 순서에 따라 덮인다.
	TArray<FEquipmentDeltaRow> DeltaRows;
	TArray<FEquipmentDeltaRow> EffectRows;
	TArray<FEquipmentUniqueChange> UniqueChanges;
	FText Primary, Secondary, Tertiary, Extra;
	UTexture2D* CurrentIcon = nullptr;
	UTexture2D* SelectedIcon = nullptr;
	EEmployeeEquipmentGrade CurrentGrade = EEmployeeEquipmentGrade::Count;
	EEmployeeEquipmentGrade SelectedGrade = EEmployeeEquipmentGrade::Count;
	EEmployeeEquipmentGrade CurrentVisualGrade = EEmployeeEquipmentGrade::Count;
	EEmployeeEquipmentGrade SelectedVisualGrade = EEmployeeEquipmentGrade::Count;
	bool bPrimaryEnabled = true;
	bool bSecondaryEnabled = true;
	bool bShowCurrent = true;
};

DECLARE_DELEGATE_OneParam(FEquipmentDialogItemAction, UObject*);

UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentDialogWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	void Configure(const FEmployeeEquipmentDialogView& View);
	void ShowItems(const TArray<UObject*>& Items, bool bMultiSelect = false);
	void RestoreSelectedItem(UObject* Item);
	void RestoreSelectedItems(const TArray<UObject*>& Items);
	void SetNotice(const FText& Text);
	void SetPrimaryEnabled(bool bEnabled);
	void SetPreviewMaterial(UMaterialInterface* Material);
	TArray<UObject*> GetSelectedItems() const;
	FSimpleDelegate OnPrimary;
	FSimpleDelegate OnSecondary;
	FSimpleDelegate OnTertiary;
	FSimpleDelegate OnExtra;
	FSimpleDelegate OnClose;
	FSimpleDelegate OnSelectionChanged;
	FEquipmentDialogItemAction OnItemClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCloseButtonWidget> ModalCloseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> CurrentHeaderText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SelectedHeaderText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CurrentIconImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> SelectedIconImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CurrentGradeFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> SelectedGradeFrame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PreviewImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> PreviewSize;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> CurrentHeaderBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> EffectsText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> ComparisonText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> DescriptionText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> NoticeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> ModalBodySwitcher;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> ModalBody;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ModalFooter;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTileView> ResultView;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> PrimaryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> SecondaryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> TertiaryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> ExtraButton;

private:
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CurrentIconMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SelectedIconMaterial;
	UFUNCTION() void HandleClose();
	void HandlePrimary();
	void HandleSecondary();
	void HandleTertiary();
	void HandleExtra();
	void HandleItemClicked(UObject* Item);
	void HandleItemSelection(UObject* Item);
};
