// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Table/BuildableCardTable.h"
#include "Table/WorkstationCardTable.h"
#include "Table/DecorationCardTable.h"
#include "Enum/ResourceType.h"
#include "BuildPlacementPanelWidget.generated.h"

class UButton;
class APlayerCamera;
class AOfficeCameraPawn;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnOfficePlacementFeedback, const FText&);

/**
 * 건물/업무공간/장식품 배치 패널 (화면 하단 고정)
 * 기존 BuildPlacementWidget은 건물을 따라다니는 UI였으나,
 * 이 위젯은 화면 하단에 고정된 패널 형태
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UBuildPlacementPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FBuildableCardTable BuildableInfo;

	void SetBuildableInfo(const FBuildableCardTable& InBuildableInfo, bool bIsMoving = false);

	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetWorkstationInfo(const FWorkstationCardTable& InWorkstationInfo);

	UFUNCTION(BlueprintCallable, Category = "Decoration")
	void SetDecorationInfo(const FDecorationCardTable& InDecorationInfo);

	// 미션 가이드 타겟 (M2/M4 배치 확정 페이즈 펄스 링)
	UWidget* GetPlaceButtonWidget() const;
	void SetOfficeEditMode(AOfficeCameraPawn* OfficePawn);
	void RefreshOfficeEditSelection();
	FOnOfficePlacementFeedback OnOfficePlacementFeedback;

protected:
	// 버튼 바인딩
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* PlaceButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* RotateButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* CancelButton;
	UPROPERTY(EditDefaultsOnly, Category = "Office|Edit")
	TObjectPtr<UTexture2D> RemoveIcon;

	// 철거 버튼은 취소 버튼 스타일을 복사해 만들어지므로 틴트를 따로 주지 않으면 취소와 같은 색이 된다
	UPROPERTY(EditDefaultsOnly, Category = "Office|Edit")
	FLinearColor RemoveIconTint = FLinearColor(0.552011f, 0.033105f, 0.023153f, 1.0f);

private:
	APlayerCamera* Player = nullptr;
	bool bIsOfficeEditMode = false;
	UPROPERTY(Transient) TObjectPtr<UButton> RemoveButton;
	void BuildOfficeRemoveButton();
	UFUNCTION() void OnRemoveButtonClicked();

	// 현재 모드의 비용을 지불할 수 있는지 (UI 부작용 없음)
	bool CanAffordCurrent() const;

	// 못 내면 안내 토스트까지 띄우고 false. 배치 확정 직전 게이트용.
	bool TryAffordCurrent() const;

	// 잔액에 따라 [배치] 활성/비활성 — 연속 배치가 어디서 멈춰야 하는지 버튼이 알려주게 한다.
	void RefreshPlaceButtonEnabled();

	// 유휴 수입으로 돈이 들어오면 꺼져 있던 [배치]가 다시 켜져야 한다.
	// 구독이 없으면 자금이 회복돼도 패널을 닫았다 열기 전까지 버튼이 죽은 채로 남는다.
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	FDelegateHandle ResourceChangedHandle;

	// 오피스 배치 종료 — 배치 바만 닫으면 바로 OfficeMain 이 드러난다(카탈로그는 배치 시작 시 이미 닫혔다).
	void CloseOfficePlacementUI();

	UFUNCTION()
	void OnPlaceButtonClicked();
	UFUNCTION()
	void OnRotateButtonClicked();
	UFUNCTION()
	void OnCancelButtonClicked();

	void CheckHasConstructionCost() const;
	void SpendConstructionCost() const;
	void SpendDecorationCost() const;

	// 기존 건물 재배치 모드인지 여부
	bool bIsMovingExistingBuilding = false;

	// 업무공간 모드인지 여부
	bool bIsWorkstationMode = false;

	// 장식품 모드인지 여부
	bool bIsDecorationMode = false;

	// 업무공간 정보
	FWorkstationCardTable WorkstationInfo;

	// 장식품 정보
	FDecorationCardTable DecorationInfo;
};
