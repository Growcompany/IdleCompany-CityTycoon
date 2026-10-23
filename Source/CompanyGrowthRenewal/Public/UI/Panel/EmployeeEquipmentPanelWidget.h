#pragma once
#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "EmployeeEquipmentPanelWidget.generated.h"

class UPanelHeaderWidget;
class UOverlay;
class UButtonWidget;
class UTileView;
class UCommonButtonBase;
class UCommonButtonGroupBase;
enum class EEmployeeEquipmentSlot : uint8;

UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()
public:
	void OpenWithContext(int32 EmployeeId, EEquipmentWorkspaceView View = EEquipmentWorkspaceView::Inventory);

	// OpenWithContext 이후에만 유효하다 — 그 전에는 늘 null.
	UEmployeeEquipmentWorkspaceWidget* GetWorkspace() const { return Workspace; }

	FSimpleDelegate OnPanelClosed;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UOverlay> WorkspaceHost;

	// 창 안에는 부위 선택과 장비 목록 둘뿐이다. 둘 다 배치돼 있으면 이 창이 직접 그리고,
	// 하나라도 없으면 구 경로(WorkspaceHost 에 워크스페이스 생성)로 떨어진다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilterAll;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter0;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter1;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter2;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter3;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter4;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButtonWidget> PartFilter5;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTileView> InventoryView;
private:
	void EnsureWorkspace();
	UFUNCTION() void HandleClose();

	// 부위 줄 + 목록을 이 창이 직접 들고 있는가
	bool HasOwnList() const;
	void SetupPartRail();
	void RefreshList();
	UFUNCTION() void HandlePartSelected(UCommonButtonBase* Button, int32 Index);

	UPROPERTY() TObjectPtr<UCommonButtonGroupBase> PartGroup;
	bool bAllSlots = true;
	EEmployeeEquipmentSlot CurrentSlot = EEmployeeEquipmentSlot::Head;

	UPROPERTY() TObjectPtr<UEmployeeEquipmentWorkspaceWidget> Workspace;
	int32 ContextEmployeeId = INDEX_NONE;
	EEquipmentWorkspaceView InitialView = EEquipmentWorkspaceView::Inventory;
};
