// 부지 가격 배지 — WBP 기반 UUserWidget (UIE_PlotPriceBadge).
// 비주얼(글래스/라운드/골드/그림자/잠금)은 WBP 가 소유(디자이너가 에디터에서 스타일).
// C++ 는 가격 텍스트/색 + 클릭→인수만 담당. 인수는 배지 클릭 또는 3D 부지 탭 둘 다 ACityPlotActor::TryPurchase.
// EWidgetType::PlotPriceBadge + DT_WidgetClass 로 로드 (표준 패턴), InGameLayer 가 풀로 생성/배치.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlotPriceBadgeWidget.generated.h"

class UButton;
class UCommonTextBlock;
class UImage;
class UTexture2D;
class ACityPlotActor;

UCLASS()
class COMPANYGROWTHRENEWAL_API UPlotPriceBadgeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이 배지가 대표하는 부지(클릭 시 이 부지 인수). 약참조.
	void SetPlot(ACityPlotActor* InPlot);

	// 가격 텍스트 + 자금 여부(부족=뮤트레드). 캐시 후 PriceText 위젯에 반영(NativeConstruct 전 호출도 안전).
	void SetPrice(const FText& InPrice, bool bInCanAfford);

	// 매 틱 위치 추적용 — 이 배지가 대표하는 부지(InGameLayer 가 재투영). 부지 소멸 시 null.
	ACityPlotActor* GetPlot() const { return Plot.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 클릭 루트 버튼 — WBP 에서 글래스/라운드/그림자 스타일. C++ 는 OnClicked 만 바인딩.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BadgeButton;

	// 가격 숫자 — C++ 가 SetText + 색(충분=화이트/부족=레드) 적용. NEXON Bold 등 폰트는 WBP 지정.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PriceText;

	// (선택) 자물쇠 아이콘 — 보통 WBP 에 Locked_Gold 고정. C++ 교체가 필요할 때만 바인딩.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LockIcon;

private:
	UFUNCTION()
	void HandleBadgeClicked();

	// 캐시 표시 상태(위젯이 아직 없을 때 보관 → NativeConstruct 에서 반영).
	void ApplyPriceToWidget();

	FText CachedPrice;
	bool bCachedCanAfford = true;

	TWeakObjectPtr<ACityPlotActor> Plot;
};
