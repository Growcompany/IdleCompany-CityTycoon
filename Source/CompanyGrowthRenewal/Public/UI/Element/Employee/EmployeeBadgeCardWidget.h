#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "EmployeeBadgeCardWidget.generated.h"

class UImage;
class UWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 사무실 직원 패널 왼쪽 레일의 사원증 카드 (UIE_EmployeeBadgeCard, 세로 = 가로 × 1.5)
 * - 무채색 카드 + 어두운 사진창. 잠재능력 등급은 사진 아래 빛 한 줄로만(일반 = 없음), ★은 카드에 없다(패널에만)
 * - 이름 = 베이스 NameText, 「Lv.N」 의 숫자 = 베이스 LevelText. StarsText 는 베이스 계약이라 두되 접는다
 * - 패널이 직접 만들어 SetBadge 로 채운다 — 사무실/대기 두 묶음 사이에 머리줄이 끼어야 해서 목록 엔트리로 쓰지 않는다
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeBadgeCardWidget : public UEmployeeRosterCardWidget
{
	GENERATED_BODY()

public:
	void SetBadge(const FEmployeeInstance& Employee, bool bCanLevelUp, bool bOnBench);

	void SetBadgeSelected(bool bNowSelected);

	UFUNCTION(BlueprintPure, Category = "Employee")
	int32 GetBadgeEmployeeId() const { return BadgeEmployeeId; }

	UFUNCTION(BlueprintPure, Category = "Employee")
	bool IsBadgeSelected() const { return bBadgeSelected; }

	// 초상 PNG 디코드는 동기라 목록을 다시 채울 때마다 하면 무겁다 — 파일이 그대로면 앞서 만든 텍스처를 다시 쓴다
	static UTexture2D* GetCachedPortrait(int32 EmployeeId, const FVector2D& TargetSize);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;

	// 왼쪽 위 흰 원판 + 짙은 직능 글리프
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> TeamGlyph;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> TeamDisc;

	// 오른쪽 위 레벨업 가능 표시 (대기 직원은 그 자리에 「대기」)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> LevelUpMark;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> BenchLabel;

	// 사진 아래 잠재 등급 빛 한 줄 — 색은 런타임 주입, 일반 등급이면 접는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> PotentialGlow;

	// 빛 줄 위로 옅게 번지는 층
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PotentialHaze;

	// 선택 링 묶음 (바깥 링 + 아래 턱). 베이스 SelectionBG 는 이 묶음 안의 링 면이다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> SelectionRing;

	// 인덱스 = EProductionDiscipline 순서(GetPrimaryDisciplineSlot). WBP CDO 가 레벨업 카드와 같은 글리프를 가리킨다
	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	TArray<TSoftObjectPtr<UTexture2D>> TeamGlyphIcons;

	// 대기 직원 사진 무채색화 재질 (텍스처 파라미터 Icon)
	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	TSoftObjectPtr<UMaterialInterface> BenchPortraitMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	FLinearColor BenchPortraitTint = FLinearColor(0.85f, 0.85f, 0.85f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	FLinearColor BenchNameColor = FLinearColor(FColor(0x7D, 0x87, 0x96));

	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	float BenchTeamDiscOpacity = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	float PotentialLineAlpha = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Badge")
	float PotentialHazeAlpha = 0.25f;

private:
	int32 BadgeEmployeeId = INDEX_NONE;
	bool bBadgeSelected = false;
	FSlateColor AuthoredNameColor;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BenchPortraitMID;

	void ApplyPortrait(bool bOnBench);
	void ApplyTeam(const FEmployeeInstance& Employee, bool bOnBench);
	void ApplyPotential(const FEmployeeInstance& Employee);
	void HandleBadgeClicked();
};
