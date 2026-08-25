// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BulkModeSelectorWidget.generated.h"

class UCheckBox;

// 강화 배율 모드 (공용 부품 로컬 — EWidgetType 아님). BuildingFloor 같은 마일스톤형은 호출부에서 배율 대상 제외.
// '최대' 모드는 노가다 손맛("많이, 조금씩")을 죽여 기각 (2026-07-23 사용자 결정 — 재도입 금지)
enum class EEnhanceBulkMode : uint8
{
	x1,
	x10,
	x50
};

// 유저 선택으로 모드가 바뀔 때만 발신 (SetMode 동기화는 발신하지 않음)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBulkModeChanged, EEnhanceBulkMode /*NewMode*/);

/**
 * 강화 배율 선택기 (x1/x10/x50 배타 라디오) — 빌딩/공장 강화 패널 공용 부품.
 * 자가 트리: WBP 가 빈 트리면 RebuildWidget 에서 플레이트+체크3 을 C++ 로 구성(DisciplineRadar 패턴).
 *            디자이너가 트리를 넣었으면 BindWidgetOptional 로 그걸 사용(양립 — 플레이북 패턴).
 * 브러시/폰트를 C++ 로 구성하는 건 "런타임 상태 기반 시각 + 자가 트리"라 정적 WBP 스타일 규칙의 허용 예외
 * (자가 트리엔 대응 WBP 스타일 자산이 없음).
 * 내부 부품이라 BlueprintCallable 미노출 (C++ 우선).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBulkModeSelectorWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 체크 상태를 InMode 와 일치시킴 (브로드캐스트 없음 — 세션 복원/외부 동기화용)
	void SetMode(EEnhanceBulkMode InMode);
	EEnhanceBulkMode GetMode() const { return CurrentMode; }

	// 유저가 배율을 바꿨을 때만 발신
	FOnBulkModeChanged OnModeChanged;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ===== 스타일 노브 (UIE_BulkModeSelector WBP Class Defaults 에서 디자이너 튜닝) =====
	// 기본값 = 관리 패널 셸(Panel_Float v2 근흑 #17191C)과 동일 계열 — 패널의 연장으로 읽히게

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor PlateFill = FLinearColor(0.0086f, 0.0097f, 0.0116f, 0.94f);      // #17191C

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor PlateHairline = FLinearColor(0.1022f, 0.1094f, 0.1170f, 0.8f);   // #5A5D60 (Panel_Float 림)

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor CheckFillSelected = FLinearColor(0.047f, 0.328f, 0.745f, 1.0f);  // #3D9BE0 기능 블루

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor CheckWellUnselected = FLinearColor(0.0160f, 0.0194f, 0.0242f, 1.0f); // #22262B InnerPlate

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor CheckHairline = FLinearColor(1.0f, 1.0f, 1.0f, 0.18f);

	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor LabelInk = FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.0f);        // #ECEEF0

	UPROPERTY(EditAnywhere, Category = "Style|Layout")
	float CheckBoxSize = 48.f;

	UPROPERTY(EditAnywhere, Category = "Style|Layout")
	float PlateCornerRadius = 14.f;

	UPROPERTY(EditAnywhere, Category = "Style|Layout")
	int32 LabelFontSize = 28;

	UPROPERTY(EditAnywhere, Category = "Style|Layout")
	FMargin PlateContentPadding = FMargin(18.f, 14.f, 18.f, 14.f);

	// 자가 트리 구성 시 배선되거나, 디자이너 트리에서 이름으로 바인딩됨
	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* BulkCheck_x1;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* BulkCheck_x10;

	UPROPERTY(meta = (BindWidgetOptional))
	UCheckBox* BulkCheck_x50;

private:
	UFUNCTION() void OnCheck_x1(bool bIsChecked);
	UFUNCTION() void OnCheck_x10(bool bIsChecked);
	UFUNCTION() void OnCheck_x50(bool bIsChecked);

	// 라디오 규약: 현재 모드 해제 시도는 되돌림(항상 하나 선택), 새 모드 선택만 브로드캐스트
	void HandleCheckChanged(EEnhanceBulkMode Mode, UCheckBox* Sender, bool bIsChecked);

	// 세 체크를 CurrentMode 에 맞게 SetIsChecked (SetIsChecked 는 무브로드캐스트라 재귀 안전)
	void SyncVisuals();

	// 빈 트리일 때 플레이트+VerticalBox+체크3 을 구성하고 멤버 포인터를 배선
	void BuildSelfTree();

	// 체크 3개 OnCheckStateChanged 바인딩/해제 (bChecksBound 가드 — Construct/Destruct 쌍)
	void BindChecks();
	void UnbindChecks();

	EEnhanceBulkMode CurrentMode = EEnhanceBulkMode::x1;
	bool bChecksBound = false;
};
