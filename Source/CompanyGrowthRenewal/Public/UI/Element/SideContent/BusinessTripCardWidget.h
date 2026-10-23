#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Table/BusinessTripCardTable.h"
#include "BusinessTripCardWidget.generated.h"

class UImage;
class UHorizontalBox;
class UVerticalBox;
class UCommonTextBlock;
class UGameInstance;
class UWidgetTree;
struct FSideDungeonLootRow;

/** 카드 상태 — 아트 하단 상태 띠의 유일한 입력. */
UENUM(BlueprintType)
enum class EBusinessTripCardState : uint8
{
	Normal = 0,
	CrewShort,
	DisciplineShort
};

/**
 * 출장 카드 1장 (목적지 · 기간 · 조건 · 확정 경험치 · 드랍).
 * 선택 표시는 패널이 SetSelected 로 준다 — 카드가 스스로 선택 상태를 기억하지 않는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBusinessTripCardWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTripCardClicked, FName /*CardKey*/);
	FOnTripCardClicked OnTripCardClicked;

	/** DisciplineName 은 건물 산업에 따라 달라지므로 패널이 DT_DisciplineDisplay 에서 읽어 넘긴다. */
	void SetCard(FName InCardKey, const FBusinessTripCardTable& InRow, const FText& InDisciplineName);

	void SetSelected(bool bInSelected);
	void SetCardState(EBusinessTripCardState InState);

	FName GetCardKey() const { return CardKey; }

	/** "30분" / "4시간" — 카드·판정 힌트·진행 카드가 같은 문자열을 쓴다. 분·시만 쓰고 초는 안 보인다. */
	static FText FormatTripDuration(int32 Seconds);

	/**
	 * 드랍 미리보기 — 아이콘 + 수량 + 확률 표기. 진행 카드도 같은 줄을 쓰므로 static 으로 한 벌만 둔다.
	 * 실제 지급된 보상은 확률이 없으므로 CGRewardChip::RenderIcons 가 그린다.
	 */
	static void BuildLootPreview(UWidgetTree& InWidgetTree, UGameInstance* InGameInstance, UVerticalBox& InList,
		const TArray<FSideDungeonLootRow>& InRows, int32 InStage, const UCommonTextBlock* InSample);

	/** 이 카드의 RewardList 에 위 미리보기를 채운다 — 폰트는 디자인타임 샘플에서 복사한다. */
	void SetLootPreview(const TArray<FSideDungeonLootRow>& InRows, int32 InStage);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* DestinationText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ConditionText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* DurationText = nullptr;

	/** 확정 보상 — 드랍(확률)과 한눈에 갈라야 해서 아트 위 초록 알약으로 따로 선다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ExperienceText = nullptr;

	/**
	 * 드랍 칩을 가로로 깐다. 칩은 연구소와 같은 `UTrainingRewardChipWidget` 이고 WBP 만 카드용 변형이다.
	 * 구 세로 텍스트 목록(`아이콘 x2 · 높음`)은 아이템 이름이 없어 표처럼 읽혔다 — 칩이 이름·수량·확률을 함께 싣는다.
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* RewardList = nullptr;

	/** 아트 하단 상태 띠. 카드 안에 밴드를 세로로 더하면 3장 한 줄 레이아웃의 세로가 무너진다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* StatusStrip = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* StatusText = nullptr;

	/** 목적지 아트 244x80. DT_BusinessTripCard.CardArt 가 단일 진실이라 코드가 브러시를 갈아 끼운다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* DestinationArt = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* SelectedRing = nullptr;

private:
	void HandleClicked();

	FName CardKey;
};
