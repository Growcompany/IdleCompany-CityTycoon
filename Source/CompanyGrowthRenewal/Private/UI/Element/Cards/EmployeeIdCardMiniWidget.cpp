// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/EmployeeIdCardMiniWidget.h"
#include "Enum/LootBoxRarity.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

// 파일 고유 네임스페이스 — 익명 ns 는 유니티 빌드에서 타 cpp 상수와 충돌 (2026-07-11 실측)
namespace MiniCardFx
{
	// 센터 카드 EmpFx 와 동일 값 (EmployeeGachaPresentationWidget.cpp §EmpFx)
	constexpr float ShinePeriod = 2.7f;
	constexpr float ShineSweep = 0.8f;
	constexpr float ShineMaxA = 0.55f;
	constexpr float HaloMaxOpacity = 0.45f;
	constexpr float ShineTravel = 700.f; // 카드 대각 스윕 거리 (560 폭 + 여유)
	constexpr int32 GlintTierMin = 2;    // 레어+
	constexpr int32 HaloTierMin = 3;     // 에픽+
}

// 티어 = enum 서수 (센터 TierOf 와 동일 매핑) — 재정렬하면 여기서 걸린다
static_assert(static_cast<int32>(ELootBoxRarity::Rare) == MiniCardFx::GlintTierMin
	&& static_cast<int32>(ELootBoxRarity::Epic) == MiniCardFx::HaloTierMin,
	"ELootBoxRarity ordinal must stay aligned with FX tier thresholds");

void UEmployeeIdCardMiniWidget::SetCardData(const FGachaResultData& Result, int32 IdOrdinal,
	const FText& DeptDisplayName, const FString& RankDisplayName)
{
	RarityTier = static_cast<int32>(Result.PotentialRarity);
	// 동시 생성 카드들의 광택 동기 스윕(와이퍼 룩) 방지 — 사번 기반 위상 분산 (리셋 겸용)
	FxTime = IdOrdinal * 0.27f;

	if (BandLabel)
	{
		BandLabel->SetText(FText::FromString(FString::Printf(TEXT("ID NO.%03d"), IdOrdinal)));
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(Result.ResultEmployee.EmployeeName));
	}
	if (DeptText)
	{
		DeptText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"),
			*DeptDisplayName.ToString(), *RankDisplayName)));
	}
	if (RarityText)
	{
		RarityText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Result.PotentialRarity)));
		RarityText->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityInkOnLight(Result.PotentialRarity)));
	}

	const FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(Result.PotentialRarity);
	if (PhotoRing)
	{
		PhotoRing->SetColorAndOpacity(RarityColor);
	}
	if (CardShine)
	{
		CardShine->SetRenderOpacity(0.f);
	}
	if (BackGlow)
	{
		BackGlow->SetColorAndOpacity(FLinearColor(RarityColor.R, RarityColor.G, RarityColor.B, 0.f));
		BackGlow->SetVisibility(RarityTier >= MiniCardFx::HaloTierMin
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEmployeeIdCardMiniWidget::SetPhotoMaterial(UMaterialInstanceDynamic* MID)
{
	if (PhotoImage && MID)
	{
		PhotoImage->SetBrushFromMaterial(MID);
		PhotoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UEmployeeIdCardMiniWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FxTime += InDeltaTime;

	if (CardShine && RarityTier >= MiniCardFx::GlintTierMin)
	{
		const float Cycle = FMath::Fmod(FxTime, MiniCardFx::ShinePeriod);
		const bool bSweeping = Cycle < MiniCardFx::ShineSweep;
		const float T = Cycle / MiniCardFx::ShineSweep; // 0~1 스윕, 이후 휴지
		CardShine->SetRenderOpacity(bSweeping ? MiniCardFx::ShineMaxA * FMath::Sin(T * PI) : 0.f);
		CardShine->SetRenderTranslation(FVector2D((T - 0.5f) * MiniCardFx::ShineTravel, 0.f));
	}
	if (BackGlow && RarityTier >= MiniCardFx::HaloTierMin)
	{
		// 브리딩 = 밝기만 (스케일 펄스는 기각 이력)
		FLinearColor GlowColor = BackGlow->GetColorAndOpacity();
		GlowColor.A = MiniCardFx::HaloMaxOpacity * (0.7f + 0.3f * FMath::Sin(FxTime * 2.f));
		BackGlow->SetColorAndOpacity(GlowColor);
	}
}
