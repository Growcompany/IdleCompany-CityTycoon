#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "EmployeeEquipmentListWidget.generated.h"

class UComboBoxString;
class UEmployeeEquipmentCardWidget;
class UImage;
class UCommonTextBlock;
class UEmployeeEquipmentManagerSubsystem;
class UTableManagerSubsystem;
class UTileView;
class UUserWidget;

DECLARE_DELEGATE_OneParam(FEquipmentListItemPicked, const FGuid& /*InstanceId*/);

/**
 * 부위·등급 선택박스와 보유 장비 타일 목록.
 *
 * 단독 장비 창과 직원창 오른쪽 칼럼이 같은 물건을 쓴다 ― 창이 제 목록을 따로 들고 있던 구성을
 * 부품으로 뽑은 것이다(2026-09-25). 이 부품은 목록과 선택만 책임지고, 상세·장착은 소유자가 한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentListWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	/** 이 직원을 기준으로 목록을 채운다. 부위 선택은 유지된다. */
	void Configure(int32 EmployeeId);
	void SelectPart(EEmployeeEquipmentSlot EquipmentSlot);
	void SelectAll();
	/** 전체 부위로 넓힌다 — SelectAll 과 달리 등급 거르기는 그대로 둔다. */
	void ShowAllParts();
	void Refresh();
	/** 지금 거르기(부위·등급)로 보이는 타일 수. */
	int32 GetVisibleItemCount() const;
	/** 현재 목록의 보유 인스턴스를 골라 선택 표시와 소유자의 상세를 함께 갱신한다. */
	bool SelectItem(const FGuid& InstanceId, bool bScrollIntoView = false);
	/** 튜토리얼 앵커 — 이 인스턴스의 생성된 타일(실제 클릭 대상). 목록에 없거나 가상화로 아직 안 만들어졌으면 nullptr */
	UWidget* GetEntryWidgetForInstance(const FGuid& InstanceId) const;
	bool IsNewSelectionNotification(const FGuid& InstanceId) const
	{
		return bNotifyingNewSelection && InstanceId.IsValid() && SelectedInstanceId == InstanceId;
	}

	/** 선택 표시를 지운다 ― 소유자가 상세를 닫을 때 부른다. */
	void ClearSelection();

	FEquipmentListItemPicked OnItemPicked;
	/** Refresh 가 끝날 때마다 — 등급 거르기처럼 목록 안에서 바뀐 개수를 소유자가 받아 적는다. */
	FSimpleDelegate OnListRefreshed;

protected:
	// 디자이너에서도 선택박스의 라벨이 있어야 실제 내용 폭을 확인할 수 있다.
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 데이터가 꽂히는 자리는 required ― Optional 은 오타 한 글자를 조용한 빈칸으로 바꾼다.
	// 부위는 6종이라 칩 7칸이 반쪽 폭에 안 들어간다(라벨이 칩 밖으로 삐져나왔다).
	// 선택박스는 부위마다 개수를 같이 실을 수 있어 0개인 부위를 헛으로 누르지 않는다.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> PartCombo;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> GradeCombo;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PartFilterChipBG;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PartFilterChipLine;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> GradeFilterChipBG;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> GradeFilterChipLine;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTileView> InventoryView;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> OwnedCountText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> EmptyListText;

private:
	friend struct FEmployeeEquipmentUIValidation;
	/** 부위 이름 7개(전체 + 6부위)를 DT 에서 읽어 채운다. 개수는 매 Refresh 에 고쳐 쓴다. */
	void RebuildPartOptions();
	void RebuildGradeOptions();
	UFUNCTION() void HandlePartChanged(FString Item, ESelectInfo::Type SelectType);
	UFUNCTION() void HandleGradeChanged(FString Item, ESelectInfo::Type SelectType);
	/** Popup rows show labels and counts; EquipmentFilterComboBox owns the selected label. */
	UFUNCTION() UWidget* GeneratePartRow(FString Item);
	UFUNCTION() UWidget* GenerateGradeRow(FString Item);
	UWidget* MakeFilterRow(const FString& Item, UComboBoxString* Combo, int32 Count);
	void HandleItemClicked(UObject* Item);
	void HandleInventoryEntryGenerated(UUserWidget& EntryWidget);
	void TryConsumeSelectionPulse(UEmployeeEquipmentCardWidget* Card);

	/** 표시 순서 0=전체, 1..6=부위. 개수는 같은 인덱스. */
	TArray<FString> PartLabels;
	TArray<int32> PartCounts;
	TArray<FString> GradeLabels;
	TArray<int32> GradeCounts;
	bool bUpdatingCombo = false;
	FVector2D LastPartChipSize = FVector2D::ZeroVector;
	FVector2D LastGradeChipSize = FVector2D::ZeroVector;
	int32 ContextEmployeeId = INDEX_NONE;
	bool bAllSlots = true;
	EEmployeeEquipmentSlot CurrentSlot = EEmployeeEquipmentSlot::Head;
	EEmployeeEquipmentGrade CurrentGrade = EEmployeeEquipmentGrade::Count;
	FGuid SelectedInstanceId;
	FGuid PendingSelectionPulseId;
	bool bNotifyingNewSelection = false;
};
