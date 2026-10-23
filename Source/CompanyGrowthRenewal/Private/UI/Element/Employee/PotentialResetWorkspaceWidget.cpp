// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Employee/PotentialResetWorkspaceWidget.h"

#include "Manager/CloudEconomySubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeePotentialData.h"
#include "Enum/LootBoxRarity.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "UI/Panel/PotentialTierUpOverlayWidget.h"
#include "Enum/WidgetType.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Table/ShopItemTable.h"

namespace
{
	namespace PotInk
	{
		FORCEINLINE FLinearColor Body() { return FLinearColor::FromSRGBColor(FColor(0x0B, 0x23, 0x46)); }
		FORCEINLINE FLinearColor Mute() { return FLinearColor::FromSRGBColor(FColor(0x50, 0x68, 0x87)); }
		FORCEINLINE FLinearColor Value() { return FLinearColor::FromSRGBColor(FColor(0x13, 0x83, 0x4D)); }
		FORCEINLINE FLinearColor Short() { return FLinearColor::FromSRGBColor(FColor(0xC4, 0x33, 0x2F)); }
		FORCEINLINE FLinearColor Off() { return FLinearColor::FromSRGBColor(FColor(0x7A, 0x8B, 0xA0)); }
	}

	const FLinearColor DrawerResetLabelDim = FLinearColor(0.353f, 0.420f, 0.490f, 1.f);

	// v7 젤 플레이트 — 비활성 줄 아웃라인
	const FLinearColor LockedOutline = FLinearColor::FromSRGBColor(FColor(0xDC, 0xE2, 0xEA));

	// 선택 테두리는 직원창 선택 기준링(PitchTierTab.SelectBand #4A83DA·4)과 같다.
	const FLinearColor TileOutline = FLinearColor::FromSRGBColor(FColor(0xC6, 0xD2, 0xE0));
	const FLinearColor TileSelectedOutline = FLinearColor::FromSRGBColor(FColor(0x4A, 0x83, 0xDA));
	// 타일 아래 립(시안 box-shadow 0 4px 0) — 선택되면 파란 립
	const FLinearColor TileLip = FLinearColor::FromSRGBColor(FColor(0xB6, 0xC4, 0xD6));
	const FLinearColor TileSelectedLip = FLinearColor::FromSRGBColor(FColor(0x3D, 0x6D, 0xB5));

	// 판 한 장 = SDF 재질(M_UIPanel_Rounded) 하나가 면·테두리·바닥 립까지 그린다(바깥 Border 는 그리지 않는다).
	// 재질 길이 단위가 「짧은 변의 절반」이라 줄·타일·확률판에 같은 값을 넣으면 모서리가 제각각이 된다 — px 로 적고 실제 크기에서 환산한다.
	// MarginPx = 판 바깥 여유(아래 립 자리). 이미지가 그만큼 칸 밖으로 나와 있어야(WBP 음수 패딩) 판 크기가 칸과 같다.
	struct FPotPlateSpec
	{
		float RadiusPx;
		float EdgePx;
		float InnerLipPx;
		float MarginPx;
		float DropPx;
	};
	constexpr FPotPlateSpec PotLinePlate = { 16.f, 3.f, 4.f, 0.f, 0.f };
	constexpr FPotPlateSpec PotLockedLinePlate = { 16.f, 3.f, 0.f, 0.f, 0.f };
	constexpr FPotPlateSpec PotTilePlate = { 20.f, 3.f, 0.f, 5.f, 4.f };
	constexpr FPotPlateSpec PotSelectedTilePlate = { 20.f, 4.f, 0.f, 5.f, 4.f };
	constexpr FPotPlateSpec PotWellPlate = { 18.f, 3.f, 0.f, 0.f, 0.f };

	const FName PotPlateRadius(TEXT("CornerRadius"));
	const FName PotPlateEdge(TEXT("OuterThickness"));
	const FName PotPlateEdgeColor(TEXT("OuterColor"));
	const FName PotPlateLip(TEXT("LipHeight"));
	const FName PotPlateLipColor(TEXT("LipColor"));
	const FName PotPlateMargin(TEXT("ShadowMargin"));
	const FName PotPlateDrop(TEXT("ShadowOffsetY"));
	const FName PotPlateDropSoft(TEXT("ShadowSize"));
	const FName PotPlateDropColor(TEXT("ShadowColor"));
	const FName PotPlateFillTop(TEXT("FillTop"));
	const FName PotPlateFillBottom(TEXT("FillBottom"));

	// CSS rgba 를 불투명 면 위에 얹은 색 — 재질 테두리·립은 불투명이라 sRGB 에서 미리 섞는다
	FLinearColor PotMixSRGB(const FColor& Base, const FColor& Over, float Alpha)
	{
		auto Mix = [Alpha](uint8 A, uint8 B) { return static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A), static_cast<float>(B), Alpha))); };
		return FLinearColor::FromSRGBColor(FColor(Mix(Base.R, Over.R), Mix(Base.G, Over.G), Mix(Base.B, Over.B)));
	}

	const FLinearColor ChanceChipRise = FLinearColor::FromSRGBColor(FColor(0xE2, 0xF3, 0xE9));
	const FLinearColor ChanceChipNone = FLinearColor::FromSRGBColor(FColor(0xE6, 0xEB, 0xF1));

	// 명함 아이콘 = 288² 정사각 가운데 카드(19,67)-(269,221) — 투명한 위아래를 잘라 카드만 그린다(1px 여유)
	const FBox2f PotCardArtUV(FVector2f(18.f / 288.f, 66.f / 288.f), FVector2f(270.f / 288.f, 222.f / 288.f));

	void ApplyCardArtCrop(UImage* Art)
	{
		if (!Art || Art->GetBrush().GetUVRegion() == PotCardArtUV) return;
		FSlateBrush ArtBrush = Art->GetBrush();
		ArtBrush.SetUVRegion(PotCardArtUV);
		Art->SetBrush(ArtBrush);
	}

	// 0장 명함 — 그림만 딤(선택 테두리는 그대로라 "선택됨"과 "못 쓴다"가 갈린다)
	constexpr float EmptyCardDimOpacity = 0.42f;

	// 서버 소비 단위 — EmployeeManager::RequestPotentialReroll 은 1회에 명함 1장(maxPerOp 1)
	constexpr int32 CardsPerReroll = 1;

	FText FormatChancePercent(float Chance)
	{
		const bool bWhole = FMath::IsNearlyEqual(Chance, FMath::RoundToFloat(Chance), 0.05f);
		return FText::FromString(FString::Printf(bWhole ? TEXT("%.0f%%") : TEXT("%.1f%%"), Chance));
	}

	// 등급 칩 면 = 등급색 22% — TierChipBorder 와 같은 규칙
	FLinearColor GradeChipFill(ELootBoxRarity Rarity)
	{
		const FLinearColor Grade = EmployeeRarityDisplay::Color(Rarity);
		return FLinearColor(Grade.R, Grade.G, Grade.B, 0.22f);
	}

	// 리롤 연출 시간표 — 승인 시안 docs/05_UI/mockups/PotentialRerollFx_2026-10-11
	constexpr float PotFxLineStagger = .07f;
	constexpr float PotFxLineExit = .08f;
	constexpr float PotFxLineEnter = .22f;
	constexpr float PotFxShineTime = .28f;
	constexpr float PotFxGlowTime = .4f;
	constexpr float PotFxReduceSwap = .15f;
	constexpr float PotFxCardPress = .25f;
	constexpr float PotFxCostFloatTime = .6f;
	// 등급 상승 — 화면 전체 연출이 걷히는 데 걸리는 꼬리 시간, 그 연출의 뷰포트 층(알림 9999 아래)
	constexpr float PotFxReleaseTail = .26f;
	constexpr int32 PotFxTierUpZOrder = 9000;
	constexpr float PotFxWaitDimDelay = .15f;
	constexpr float PotFxWaitDimOpacity = .72f;

	float PotFxEaseOut(float T)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(T, 0.f, 1.f), 3.f);
	}

	void PotFxHide(UWidget* Layer)
	{
		if (!Layer) return;
		Layer->SetRenderOpacity(0.f);
		Layer->SetRenderTransform(FWidgetTransform());
	}

#if !UE_BUILD_SHIPPING
	bool PotFxIsRest(const UWidget* Layer, bool bHidden)
	{
		if (!Layer) return true;
		const FWidgetTransform& Transform = Layer->GetRenderTransform();
		const bool bIdentity = Transform.Translation.IsNearlyZero(.01f) && Transform.Scale.Equals(FVector2D(1.f, 1.f), .001f)
			&& FMath::IsNearlyZero(Transform.Angle, .01f);
		return bIdentity && FMath::IsNearlyEqual(Layer->GetRenderOpacity(), bHidden ? 0.f : 1.f, .001f);
	}
#endif

}

void UPotentialResetWorkspaceWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	// 디자이너도 같은 비율로 보이게 여기서 자른다 — 그림 교체(SetBrushResourceObject)는 자르기 영역을 유지한다
	UImage* CardArts[] = { CardArt0, CardArt1, CardArt2, ResetCostIcon };
	for (UImage* Art : CardArts)
	{
		ApplyCardArtCrop(Art);
	}
}

void UPotentialResetWorkspaceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
		ItemManager = GI->GetSubsystem<UItemInventoryManager>();
	}

	UButtonWidget* TileButtons[3] = { CardTileButton0, CardTileButton1, CardTileButton2 };
	for (int32 i = 0; i < 3; ++i)
	{
		if (TileButtons[i]) TileButtons[i]->OnClicked().AddUObject(this, &ThisClass::HandleCardClicked, i);
	}
	if (ResetButton) ResetButton->OnClicked().AddUObject(this, &ThisClass::HandleResetClicked);
	if (OddsButton) OddsButton->OnClicked().AddUObject(this, &ThisClass::HandleOddsClicked);
	if (ShopButton) ShopButton->OnClicked().AddUObject(this, &ThisClass::HandleShopClicked);
	if (EmployeeManager) EmployeeManager->OnEmployeeStatsChanged.AddUObject(this, &ThisClass::HandleStatsChanged);
	if (ItemManager) ItemManager->OnItemChanged.AddUObject(this, &ThisClass::HandleItemChanged);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
		{
			Cloud->OnEconomyReady.AddUObject(this, &ThisClass::HandleEconomyReady);
		}
	}

	CollectRerollFxLayers();
	ResetRerollFxVisuals();
}

void UPotentialResetWorkspaceWidget::CollectRerollFxLayers()
{
	auto Find = [this](const FString& Name) { return GetWidgetFromName(FName(*Name)); };
	for (int32 i = 0; i < 3; ++i)
	{
		LineRows[i] = Find(FString::Printf(TEXT("PotLineRow%d"), i));
		LineLocks[i] = Find(FString::Printf(TEXT("PotLineLock%d"), i));
		LineGlows[i] = Cast<UImage>(Find(FString::Printf(TEXT("PotLineGlow%d"), i)));
		LineShines[i] = Cast<UImage>(Find(FString::Printf(TEXT("PotLineShine%d"), i)));
		LineSparks[i][0] = Cast<UImage>(Find(FString::Printf(TEXT("PotLineSparkA%d"), i)));
		LineSparks[i][1] = Cast<UImage>(Find(FString::Printf(TEXT("PotLineSparkB%d"), i)));
	}

	// 펀치·눌림은 가운데를 축으로
	UWidget* Pivoted[] = { PotLine0, PotLine1, PotLine2, TierChipBorder, CardTile0, CardTile1, CardTile2,
		CardChanceChip0, CardChanceChip1, CardChanceChip2, OddsLabelText, OddsValueText, OddsSubText, OddsGradeRow };
	for (UWidget* Widget : Pivoted)
	{
		if (Widget) Widget->SetRenderTransformPivot(FVector2D(.5f, .5f));
	}
}

void UPotentialResetWorkspaceWidget::NativeDestruct()
{
	Suspend();
	UButtonWidget* TileButtons[3] = { CardTileButton0, CardTileButton1, CardTileButton2 };
	for (UButtonWidget* TileButton : TileButtons)
	{
		if (TileButton) TileButton->OnClicked().RemoveAll(this);
	}
	if (ResetButton) ResetButton->OnClicked().RemoveAll(this);
	if (OddsButton) OddsButton->OnClicked().RemoveAll(this);
	if (ShopButton) ShopButton->OnClicked().RemoveAll(this);
	if (EmployeeManager) EmployeeManager->OnEmployeeStatsChanged.RemoveAll(this);
	if (ItemManager) ItemManager->OnItemChanged.RemoveAll(this);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
		{
			Cloud->OnEconomyReady.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UPotentialResetWorkspaceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdatePlateMaterialSizes();

	// 서버 응답을 오래 기다리면 재설정 버튼을 눌린 채로 둔다 — 응답이 빠르면 깜빡이지 않게 지연 뒤에만
	if (bRerollRequestInFlight && RerollWaitElapsed >= 0.f)
	{
		RerollWaitElapsed += InDeltaTime;
		if (ResetButton && RerollWaitElapsed >= PotFxWaitDimDelay) ResetButton->SetRenderOpacity(PotFxWaitDimOpacity);
	}

	if (RerollFxElapsed >= 0.f && !bRerollFxFrozen)
	{
		// 화면 전체 연출이 OnRelease 없이 사라지면(뷰포트 일괄 정리 등) 줄이 영영 안 뽑힌다 — 사라진 순간을 걷힘으로 본다
		if (bRerollUpgraded && RerollReleaseTime >= TNumericLimits<float>::Max() && (!TierUpOverlay.IsValid() || !TierUpOverlay->IsInViewport()))
		{
			HandleTierUpRelease();
		}
		RerollFxElapsed += InDeltaTime;
		if (RerollFxElapsed >= GetRerollFxEnd())
		{
			ResetRerollFxVisuals();
			RefreshLines(GetEmployee());
		}
		else
		{
			ApplyRerollFx(RerollFxElapsed);
		}
	}
}

// FX 를 중간에 끊으면 마지막 틱이 쓴 변형/오파시티가 그대로 굳는다 — 재사용 인스턴스라 다음 오픈까지 따라간다
void UPotentialResetWorkspaceWidget::ResetRerollFxVisuals()
{
	RerollFxElapsed = -1.f;
	RerollCardIndex = INDEX_NONE;
	bRerollUpgraded = false;
	RerollReleaseTime = 0.f;
	RerollLineCount = 0;
	RerollTopLineMask = 0;
	CloseTierUpOverlay();

	UBorder* Lines[3] = { PotLine0, PotLine1, PotLine2 };
	for (int32 i = 0; i < 3; ++i)
	{
		RerollLineSwapped[i] = true;
		if (Lines[i])
		{
			Lines[i]->SetRenderScale(FVector2D(1.f, 1.f));
			Lines[i]->SetRenderOpacity(1.f);
		}
		if (UWidget* Row = LineRows[i].Get())
		{
			Row->SetRenderTranslation(FVector2D::ZeroVector);
			Row->SetRenderOpacity(1.f);
		}
		PotFxHide(LineGlows[i].Get());
		PotFxHide(LineShines[i].Get());
		PotFxHide(LineSparks[i][0].Get());
		PotFxHide(LineSparks[i][1].Get());
	}
	UWidget* Scaled[] = { CardTile0, CardTile1, CardTile2, CardChanceChip0, CardChanceChip1, CardChanceChip2,
		OddsLabelText, OddsValueText, OddsSubText, OddsGradeRow };
	for (UWidget* Widget : Scaled)
	{
		if (Widget) Widget->SetRenderScale(FVector2D(1.f, 1.f));
	}
	PotFxHide(RerollCostFloat);
}

void UPotentialResetWorkspaceWidget::StartRerollFx(bool bUpgraded, ELootBoxRarity PreRarity, ELootBoxRarity NewRarity, EItemType UsedCard)
{
	ResetRerollFxVisuals();
	const FEmployeeInstance* Employee = GetEmployee();

	bRerollUpgraded = bUpgraded;
	for (int32 i = 0; i < 3; ++i)
	{
		if (CardTypes[i] == UsedCard) RerollCardIndex = i;
	}

	// 다시 뽑힌 줄 = 열린 칸. 잠김·미개방 줄은 그대로 둔다
	const int32 Slots = Employee
		? UEmployeePotentialHelper::GetPotentialSlotsForRank(UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel))
		: 0;
	RerollLineCount = Employee ? FMath::Min(Slots, FMath::Min(3, Employee->PotentialAbility.Options.Num())) : 0;
	for (int32 i = 0; i < RerollLineCount; ++i)
	{
		const FPotentialOptionLine& Line = Employee->PotentialAbility.Options[i];
		float Min = 0.f, Max = 0.f;
		UEmployeePotentialHelper::GetValueRangeForRarity(Line.Rarity, Min, Max);
		// 고점 = 그 등급이 낼 수 있는 값의 위쪽 25% — 세 줄은 늘 같은 등급이라 등급으로는 갈리지 않는다
		if (Line.Value >= Min + .75f * (Max - Min)) RerollTopLineMask |= static_cast<uint8>(1 << i);
		RerollLineGrade[i] = MakeLineView(i, Employee).Grade;
		RerollLineInk[i] = EmployeeRarityDisplay::VividColor(Line.Rarity);
		RerollLineSwapped[i] = false;
		ApplyLineView(i, RerollOldLines[i]);
	}

	if (RerollCostFloat) RerollCostFloat->SetText(FText::FromString(FString::Printf(TEXT("-%d"), CardsPerReroll)));

	// 등급이 오르면 화면 전체 연출이 먼저 뜨고, 그것이 걷히는 순간 줄이 다시 뽑힌다. 등급별 소리는 연출이 낸다
	RerollReleaseTime = 0.f;
	if (bUpgraded && !bRerollFxSilent && ShowTierUpOverlay(PreRarity, NewRarity))
	{
		RerollReleaseTime = TNumericLimits<float>::Max();
	}
	if (!bUpgraded && !bRerollFxSilent)
	{
		if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
		{
			SoundMgr->PlayUISound(CGUISoundTags::PotentialReroll);
		}
	}

	RerollFxElapsed = 0.f;
	ApplyRerollFx(0.f);
}

bool UPotentialResetWorkspaceWidget::ShowTierUpOverlay(ELootBoxRarity From, ELootBoxRarity To)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> OverlayClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PotentialTierUpOverlay) : nullptr;
	if (!OverlayClass || !OverlayClass->IsChildOf(UPotentialTierUpOverlayWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("PotentialResetWorkspace: PotentialTierUpOverlay is missing or incompatible in DT_WidgetClass"));
		return false;
	}
	UPotentialTierUpOverlayWidget* Overlay = GetOwningPlayer()
		? CreateWidget<UPotentialTierUpOverlayWidget>(GetOwningPlayer(), OverlayClass)
		: CreateWidget<UPotentialTierUpOverlayWidget>(GetWorld(), OverlayClass);
	if (!Overlay) return false;
	Overlay->AddToViewport(PotFxTierUpZOrder);
	Overlay->OnRelease.BindUObject(this, &ThisClass::HandleTierUpRelease);
	Overlay->Play(From, To, true);
	TierUpOverlay = Overlay;
	return true;
}

void UPotentialResetWorkspaceWidget::CloseTierUpOverlay()
{
	if (UPotentialTierUpOverlayWidget* Overlay = TierUpOverlay.Get())
	{
		Overlay->OnRelease.Unbind();
		Overlay->RemoveFromParent();
	}
	TierUpOverlay.Reset();
}

void UPotentialResetWorkspaceWidget::HandleTierUpRelease()
{
	if (RerollFxElapsed >= 0.f && RerollReleaseTime > RerollFxElapsed) RerollReleaseTime = RerollFxElapsed;
}

float UPotentialResetWorkspaceWidget::GetRerollFxEnd() const
{
	const bool bReduce = USettingsManagerSubsystem::IsReduceMotion(this);
	const float LineStart = bRerollUpgraded ? RerollReleaseTime : 0.f;
	if (LineStart >= TNumericLimits<float>::Max()) return TNumericLimits<float>::Max();
	const float LineSpan = bReduce ? PotFxReduceSwap : FMath::Max(PotFxLineExit + PotFxLineEnter, PotFxLineExit + PotFxShineTime);
	const bool bGlow = RerollTopLineMask != 0 || bRerollUpgraded;
	float End = FMath::Max(PotFxCardPress, PotFxCostFloatTime);
	if (RerollLineCount > 0)
	{
		End = FMath::Max(End, LineStart + (RerollLineCount - 1) * PotFxLineStagger + LineSpan + (bGlow ? PotFxGlowTime : 0.f));
	}
	if (bRerollUpgraded) End = FMath::Max(End, LineStart + PotFxReleaseTail);
	return End + .02f;
}

bool UPotentialResetWorkspaceWidget::GetFxLocalPoint(const UWidget* Widget, const FVector2D& Point, FVector2D& OutLocal) const
{
	if (!Widget || !RerollFxCanvas) return false;
	const FGeometry& CanvasGeometry = RerollFxCanvas->GetCachedGeometry();
	const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
	if (CanvasGeometry.GetLocalSize().X < 1.f || WidgetGeometry.GetLocalSize().X < 1.f) return false;
	OutLocal = CanvasGeometry.AbsoluteToLocal(WidgetGeometry.LocalToAbsolute(FVector2D(WidgetGeometry.GetLocalSize()) * Point));
	return true;
}

void UPotentialResetWorkspaceWidget::ApplyRerollFx(float Time)
{
	const bool bReduce = USettingsManagerSubsystem::IsReduceMotion(this);

	// 쓴 명함 판 눌림 — 원래 크기 이하
	UBorder* Tiles[3] = { CardTile0, CardTile1, CardTile2 };
	if (RerollCardIndex != INDEX_NONE && Tiles[RerollCardIndex])
	{
		const float T = FMath::Clamp(Time / PotFxCardPress, 0.f, 1.f);
		const float Scale = bReduce ? 1.f : 1.f - .06f * FMath::Sin(PI * T);
		Tiles[RerollCardIndex]->SetRenderScale(FVector2D(Scale, Scale));
	}

	// 소비량 -1 — 쓴 명함 보유 수의 「장」 바로 오른쪽 제자리에 떴다가 사라진다(떠오르는 움직임은 쓰지 않는다)
	if (RerollCostFloat)
	{
		UCommonTextBlock* Units[3] = { CardCountUnit0, CardCountUnit1, CardCountUnit2 };
		FVector2D Anchor;
		const float T = Time / PotFxCostFloatTime;
		if (RerollCardIndex != INDEX_NONE && T < 1.f && GetFxLocalPoint(Units[RerollCardIndex], FVector2D(1.f, .5f), Anchor))
		{
			// 캔버스 슬롯 정렬이 가운데라 글자 절반 폭만큼 더 밀어 왼끝을 「장」 뒤에 붙인다
			const float HalfWidth = static_cast<float>(RerollCostFloat->GetDesiredSize().X) * .5f;
			RerollCostFloat->SetRenderTranslation(Anchor + FVector2D(HalfWidth + 2.f, 4.f));
			RerollCostFloat->SetRenderOpacity(T < .45f ? 1.f : 1.f - (T - .45f) / .55f);
		}
		else
		{
			RerollCostFloat->SetRenderOpacity(0.f);
		}
	}

	ApplyRerollLines(Time, bRerollUpgraded ? RerollReleaseTime : 0.f);
}

void UPotentialResetWorkspaceWidget::ApplyRerollLines(float Time, float LineStart)
{
	const bool bReduce = USettingsManagerSubsystem::IsReduceMotion(this);
	const FEmployeeInstance* Employee = GetEmployee();
	UBorder* Lines[3] = { PotLine0, PotLine1, PotLine2 };
	const float Settle = bReduce ? PotFxReduceSwap : PotFxLineExit + PotFxLineEnter;

	for (int32 i = 0; i < RerollLineCount; ++i)
	{
		UBorder* Line = Lines[i];
		UWidget* Row = LineRows[i].Get();
		const float Local = Time - LineStart - i * PotFxLineStagger;
		if (Local < 0.f) continue;

		const bool bSwapNow = Local >= (bReduce ? PotFxReduceSwap * .5f : PotFxLineExit);
		if (bSwapNow && !RerollLineSwapped[i])
		{
			RerollLineSwapped[i] = true;
			ApplyLineView(i, MakeLineView(i, Employee));
		}

		// A — 판·글자의 크기와 위치는 그대로 두고, 이전 글자가 제자리에서 흐려진 뒤 새 글자가 같은 자리에서 진해진다
		float RowAlpha = 1.f;
		if (bReduce)
		{
			const float Half = PotFxReduceSwap * .5f;
			RowAlpha = Local < Half ? 1.f - Local / Half : FMath::Clamp((Local - Half) / Half, 0.f, 1.f);
		}
		else if (!bSwapNow)
		{
			RowAlpha = 1.f - Local / PotFxLineExit;
		}
		else
		{
			RowAlpha = .35f + .65f * PotFxEaseOut((Local - PotFxLineExit) / PotFxLineEnter);
		}
		if (Row) Row->SetRenderOpacity(RowAlpha);

		const FLinearColor Grade = RerollLineGrade[i];
		const FLinearColor Ink = RerollLineInk[i];
		// 흰 판 위라 흰 빛은 안 보인다 — 등급색 쪽으로 기운 빛줄기가 줄을 한 번 훑는다
		if (UImage* Shine = LineShines[i].Get())
		{
			const float K = (Local - PotFxLineExit) / PotFxShineTime;
			const float LineWidth = Line ? static_cast<float>(Line->GetCachedGeometry().GetLocalSize().X) : 0.f;
			if (!bReduce && K >= 0.f && K < 1.f && LineWidth > 1.f)
			{
				const float ShineWidth = FMath::Max(1.f, static_cast<float>(Shine->GetCachedGeometry().GetLocalSize().X));
				Shine->SetRenderTransform(FWidgetTransform(FVector2D(FMath::Lerp(-ShineWidth, LineWidth, K), 0.f),
					FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
				Shine->SetColorAndOpacity(FMath::Lerp(Ink, FLinearColor::White, .25f));
				Shine->SetRenderOpacity(FMath::Sin(PI * K) * .9f);
			}
			else
			{
				PotFxHide(Shine);
			}
		}

		// B — 고점 줄은 테두리가 등급색으로 진해지며 바깥으로 번진다. 등급이 오른 리롤은 열린 줄 모두가 약하게 한 번
		const bool bTop = ((RerollTopLineMask >> i) & 1) != 0;
		const float Peak = bTop ? 1.f : bRerollUpgraded ? .55f : 0.f;
		const float GlowK = (Local - Settle) / PotFxGlowTime;
		const float Glow = Peak > 0.f && GlowK >= 0.f && GlowK < 1.f
			? Peak * (GlowK < .15f ? GlowK / .15f : 1.f - (GlowK - .15f) / .85f) : 0.f;
		if (UImage* GlowLayer = LineGlows[i].Get())
		{
			GlowLayer->SetColorAndOpacity(FLinearColor(Ink.R, Ink.G, Ink.B, 1.f));
			GlowLayer->SetRenderOpacity(Glow * .85f);
		}
		if (RerollLineSwapped[i]) ApplyPlateStyle(i, Grade, true, .45f + .55f * Glow);
		for (int32 j = 0; j < 2; ++j)
		{
			UImage* Spark = LineSparks[i][j].Get();
			if (!Spark) continue;
			const float K = (Local - Settle - j * .07f) / .3f;
			if (!bTop || bReduce || K < 0.f || K >= 1.f) { PotFxHide(Spark); continue; }
			Spark->SetColorAndOpacity(j == 0 ? FLinearColor(Ink.R, Ink.G, Ink.B, 1.f) : FMath::Lerp(FLinearColor(Ink.R, Ink.G, Ink.B, 1.f), FLinearColor::White, .35f));
			Spark->SetRenderOpacity(FMath::Sin(PI * K));
		}
	}
}

void UPotentialResetWorkspaceWidget::Configure(int32 InEmployeeID)
{
	if (EmployeeID != InEmployeeID || !bPresentationActive)
	{
		++PresentationGeneration;
		ResetRerollFxVisuals();
	}
	EmployeeID = InEmployeeID;
	bPresentationActive = true;
	RefreshAll();
}

void UPotentialResetWorkspaceWidget::Suspend()
{
	bPresentationActive = false;
	++PresentationGeneration;
	ResetRerollFxVisuals();
}

FEmployeeInstance* UPotentialResetWorkspaceWidget::GetEmployee() const
{
	return (EmployeeManager && EmployeeID >= 0) ? EmployeeManager->GetEmployeeData(EmployeeID) : nullptr;
}

void UPotentialResetWorkspaceWidget::RefreshAll()
{
	const FEmployeeInstance* Employee = GetEmployee();
	RefreshTier(Employee);
	RefreshLines(Employee);
	RefreshCards(Employee);
	RefreshOddsAndAction(Employee);
}

void UPotentialResetWorkspaceWidget::RefreshTier(const FEmployeeInstance* Employee)
{
	const ELootBoxRarity CurrentRarity = Employee ? Employee->PotentialAbility.CurrentRarity : ELootBoxRarity::Common;
	const ELootBoxRarity MaxAchieved = Employee ? Employee->PotentialAbility.MaxAchievedRarity : ELootBoxRarity::Common;

	if (TierChipBorder)
	{
		// 등급색은 배지 면으로 구분하고 글자는 본문 대비를 유지한다.
		TierChipBorder->SetBrushColor(GradeChipFill(CurrentRarity));
	}
	if (TierText)
	{
		FString Label = FLootBoxRarityUtility::GetKoreanName(CurrentRarity);
		if (MaxAchieved != CurrentRarity)
		{
			// 래칫 바닥은 최고 달성 등급이라 지금 등급만 보면 리롤 결과를 오해한다
			Label += TEXT(" · 최고 ") + FLootBoxRarityUtility::GetKoreanName(MaxAchieved);
		}
		TierText->SetText(FText::FromString(Label));
		// 등급색을 글자에 실으면 Common(#93A0B2) 같은 저채도 등급이 라이트 판에서 사라진다.
		TierText->SetColorAndOpacity(FSlateColor(PotInk::Body()));
	}
}

void UPotentialResetWorkspaceWidget::ApplyPlateStyle(int32 Index, const FLinearColor& Grade, bool bActive, float OutlineAlpha)
{
	UImage* Backgrounds[3] = { PotLineBG0, PotLineBG1, PotLineBG2 };
	LinePlateActive[Index] = bActive;
	UMaterialInstanceDynamic* LineMaterial = Backgrounds[Index] ? Backgrounds[Index]->GetDynamicMaterial() : nullptr;
	if (!LineMaterial) return;
	const FColor FillTop = bActive ? FColor(0xFF, 0xFF, 0xFF) : FColor(0xE7, 0xEF, 0xF7);
	const FColor FillBottom = bActive ? FColor(0xF4, 0xF8, 0xFC) : FColor(0xDC, 0xE7, 0xF2);
	LineMaterial->SetVectorParameterValue(PotPlateFillTop, FLinearColor::FromSRGBColor(FillTop));
	LineMaterial->SetVectorParameterValue(PotPlateFillBottom, FLinearColor::FromSRGBColor(FillBottom));
	// 시안 테두리 = 등급색 rgba(OutlineAlpha)를 흰 면에 얹은 색, 바닥 립 = 안쪽 아래 그림자 rgba(160,178,200,.22)
	LineMaterial->SetVectorParameterValue(PotPlateEdgeColor, bActive ? PotMixSRGB(FillTop, Grade.ToFColor(true), OutlineAlpha) : LockedOutline);
	LineMaterial->SetVectorParameterValue(PotPlateLipColor, PotMixSRGB(FillBottom, FColor(0xA0, 0xB2, 0xC8), .22f));
}

void UPotentialResetWorkspaceWidget::ApplyTileStyle(int32 Index, bool bSelected)
{
	UImage* Backgrounds[3] = { CardTileBG0, CardTileBG1, CardTileBG2 };
	if (Backgrounds[Index])
	{
		if (UMaterialInstanceDynamic* TileMaterial = Backgrounds[Index]->GetDynamicMaterial())
		{
			TileMaterial->SetVectorParameterValue(PotPlateFillTop, FLinearColor::FromSRGBColor(bSelected
				? FColor(0xF6, 0xFA, 0xFF) : FColor(0xFF, 0xFF, 0xFF)));
			TileMaterial->SetVectorParameterValue(PotPlateFillBottom, FLinearColor::FromSRGBColor(bSelected
				? FColor(0xE1, 0xEC, 0xFB) : FColor(0xED, 0xF2, 0xF8)));
			TileMaterial->SetVectorParameterValue(PotPlateEdgeColor, bSelected ? TileSelectedOutline : TileOutline);
			TileMaterial->SetVectorParameterValue(PotPlateDropColor, bSelected ? TileSelectedLip : TileLip);
		}
	}
	TilePlateSelected[Index] = bSelected;
}

void UPotentialResetWorkspaceWidget::UpdatePlateMaterialSizes()
{
	struct FPlate
	{
		UImage* Image;
		const FPotPlateSpec& Spec;
	};
	const FPlate Plates[7] = {
		{ PotLineBG0, LinePlateActive[0] ? PotLinePlate : PotLockedLinePlate },
		{ PotLineBG1, LinePlateActive[1] ? PotLinePlate : PotLockedLinePlate },
		{ PotLineBG2, LinePlateActive[2] ? PotLinePlate : PotLockedLinePlate },
		{ CardTileBG0, TilePlateSelected[0] ? PotSelectedTilePlate : PotTilePlate },
		{ CardTileBG1, TilePlateSelected[1] ? PotSelectedTilePlate : PotTilePlate },
		{ CardTileBG2, TilePlateSelected[2] ? PotSelectedTilePlate : PotTilePlate },
		{ OddsWellBG, PotWellPlate } };
	for (const FPlate& Plate : Plates)
	{
		if (!Plate.Image) continue;
		const FVector2D LocalSize = Plate.Image->GetCachedGeometry().GetLocalSize();
		// 재질 좌표 1 = 이미지 짧은 변의 절반
		const float Half = static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y)) * .5f;
		if (Half < 1.f) continue;
		UMaterialInstanceDynamic* PlateMaterial = Plate.Image->GetDynamicMaterial();
		if (!PlateMaterial) continue;
		auto Put = [PlateMaterial](const FName& Param, float Value)
		{
			if (!FMath::IsNearlyEqual(PlateMaterial->K2_GetScalarParameterValue(Param), Value, 1e-4f)) PlateMaterial->SetScalarParameterValue(Param, Value);
		};
		Put(PotPlateRadius, Plate.Spec.RadiusPx / Half);
		Put(PotPlateEdge, Plate.Spec.EdgePx / Half);
		// 립 띠 두께 = LipHeight × 2 (재질 좌표)
		Put(PotPlateLip, Plate.Spec.InnerLipPx / (Half * 2.f));
		Put(PotPlateMargin, Plate.Spec.MarginPx / Half);
		Put(PotPlateDrop, Plate.Spec.DropPx / Half);
		// 아래 립 가장자리는 1px 로 딱 떨어지게
		Put(PotPlateDropSoft, Plate.Spec.DropPx > 0.f ? 1.f / Half : 0.f);
	}
}

void UPotentialResetWorkspaceWidget::RefreshLines(const FEmployeeInstance* Employee)
{
	for (int32 i = 0; i < 3; ++i)
	{
		// 리롤 연출 중 아직 빠지는 줄은 직전 화면을 유지한다 — 새 값은 그 줄이 바뀌는 순간에 들어온다
		if (RerollFxElapsed >= 0.f && !RerollLineSwapped[i]) continue;
		ApplyLineView(i, MakeLineView(i, Employee));
	}
}

UPotentialResetWorkspaceWidget::FPotLineView UPotentialResetWorkspaceWidget::MakeLineView(int32 Index, const FEmployeeInstance* Employee) const
{
	FPotLineView View;
	const int32 Slots = Employee
		? UEmployeePotentialHelper::GetPotentialSlotsForRank(
			UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee->EnhancementLevel))
		: 0;
	const TArray<FPotentialOptionLine>* Options = Employee ? &Employee->PotentialAbility.Options : nullptr;

	if (Index >= Slots)
	{
		// 잠긴 칸 — 열리는 조건 한 구절(「★3에 열림」). 경계 별 수 = 슬롯 매핑(별0~2:1줄/별3~5:2줄/별6+:3줄)
		View.Name = FText::FromString(UEmployeePotentialHelper::GetLockedLineUnlockText(Index));
		View.Value = FText::GetEmpty();
		View.bLocked = true;
		View.NameInk = PotInk::Mute();
		View.ValueInk = PotInk::Mute();
		View.Grade = LockedOutline;
	}
	else if (Options && Index < Options->Num())
	{
		const FPotentialOptionLine& Line = (*Options)[Index];
		View.Name = UEmployeePotentialHelper::GetPotentialOptionName(Line.OptionType);
		View.Value = UEmployeePotentialHelper::GetPotentialOptionValueText(Line);
		View.NameInk = PotInk::Body();
		View.ValueInk = PotInk::Value();
		// 테두리 틴트는 시안처럼 16진 그대로의 등급색을 45% 로 얹는다(옅은 칩 톤을 쓰면 흰 판에서 사라진다)
		View.Grade = EmployeeRarityDisplay::VividColor(Line.Rarity);
		View.Grade.A = 1.f;
		View.bActive = true;
	}
	else
	{
		// 해금됐지만 아직 안 굴린 줄 (강화로 슬롯이 늘어난 직후)
		// 「빈 줄」은 결함처럼 읽힌다 — 상태를 말하는 「미개방」으로 적고 이름도 값과 같은 낮은 잉크로 둔다.
		View.Name = FText::FromString(TEXT("미개방"));
		View.Value = FText::FromString(TEXT("재설정으로 열립니다"));
		View.NameInk = PotInk::Mute();
		View.ValueInk = PotInk::Mute();
		View.Grade = LockedOutline;
	}
	return View;
}

void UPotentialResetWorkspaceWidget::ApplyLineView(int32 Index, const FPotLineView& View)
{
	UBorder* Lines[3] = { PotLine0, PotLine1, PotLine2 };
	UCommonTextBlock* Names[3] = { PotLineName0, PotLineName1, PotLineName2 };
	UCommonTextBlock* Values[3] = { PotLineValue0, PotLineValue1, PotLineValue2 };
	if (!Lines[Index] || !Names[Index] || !Values[Index]) return;

	Lines[Index]->SetRenderOpacity(1.f);
	Names[Index]->SetText(View.Name);
	Names[Index]->SetColorAndOpacity(FSlateColor(View.NameInk));
	Values[Index]->SetText(View.Value);
	Values[Index]->SetColorAndOpacity(FSlateColor(View.ValueInk));
	// 숫자=Pretendard·문구=NEXON — 「★3 해금」 같은 문구가 숫자 글꼴이면 같은 줄에 한글 서체가 둘이 된다
	if (!LineValueNumberFont.FontObject) LineValueNumberFont = Values[Index]->GetFont();
	FSlateFontInfo ValueFont = LineValueNumberFont;
	if (!View.bActive)
	{
		ValueFont.FontObject = Names[Index]->GetFont().FontObject;
		ValueFont.TypefaceFontName = Names[Index]->GetFont().TypefaceFontName;
	}
	Values[Index]->SetFont(ValueFont);
	if (UWidget* Lock = LineLocks[Index].Get())
	{
		Lock->SetVisibility(View.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplyPlateStyle(Index, View.Grade, View.bActive);
}

void UPotentialResetWorkspaceWidget::RefreshCards(const FEmployeeInstance* Employee)
{
	UImage* Arts[3] = { CardArt0, CardArt1, CardArt2 };
	UCommonTextBlock* CardNames[3] = { CardName0, CardName1, CardName2 };
	UCommonTextBlock* Counts[3] = { CardCountText0, CardCountText1, CardCountText2 };
	UCommonTextBlock* Units[3] = { CardCountUnit0, CardCountUnit1, CardCountUnit2 };
	UBorder* ChanceChips[3] = { CardChanceChip0, CardChanceChip1, CardChanceChip2 };
	UCommonTextBlock* ChanceArrows[3] = { CardChanceArrow0, CardChanceArrow1, CardChanceArrow2 };
	UCommonTextBlock* ChanceTexts[3] = { CardChanceText0, CardChanceText1, CardChanceText2 };
	UWidget* Checks[3] = { CardCheck0, CardCheck1, CardCheck2 };

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const ELootBoxRarity CurrentRarity = Employee ? Employee->PotentialAbility.CurrentRarity : ELootBoxRarity::Common;
	const ELootBoxRarity MaxAchieved = Employee ? Employee->PotentialAbility.MaxAchievedRarity : ELootBoxRarity::Common;

	for (int32 i = 0; i < 3; ++i)
	{
		const EItemType CardType = CardTypes[i];
		const int32 Count = ItemManager ? ItemManager->GetItemCount(CardType) : 0;
		const bool bSelected = CardType == SelectedCard;
		FShopItemTable CardDefinition;
		const bool bHasDefinition = TableMgr && TableMgr->GetShopItemByItemType(CardType, CardDefinition);
		if (!bHasDefinition)
		{
			UE_LOG(LogTemp, Warning, TEXT("PotentialResetWorkspace: DT_ShopItem entry missing for card type %d"), static_cast<int32>(CardType));
		}

		if (Arts[i])
		{
			Arts[i]->SetBrushResourceObject(bHasDefinition ? CardDefinition.ResolveIconResource() : nullptr);
			Arts[i]->SetRenderOpacity(Count > 0 ? 1.f : EmptyCardDimOpacity);
		}
		if (CardNames[i])
		{
			const FText CardName = bHasDefinition ? CardDefinition.DisplayName : FText::GetEmpty();
			CardNames[i]->SetText(CardName);
			CardNames[i]->SetVisibility(CardName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			CardNames[i]->SetColorAndOpacity(FSlateColor(PotInk::Body()));
		}
		if (Counts[i])
		{
			Counts[i]->SetText(FText::AsNumber(Count));
			Counts[i]->SetColorAndOpacity(FSlateColor(Count > 0 ? PotInk::Body() : bMuteEmptyCardCount ? PotInk::Off() : PotInk::Short()));
		}
		if (Units[i])
		{
			Units[i]->SetColorAndOpacity(FSlateColor(Count > 0 ? PotInk::Mute() : bMuteEmptyCardCount ? PotInk::Off() : PotInk::Short()));
		}

		// 고른 명함만이 아니라 세 장 모두 상승 확률을 보여야 「어느 명함을 써야 오르나」가 판 위에서 갈린다
		const float Chance = Employee
			? UEmployeePotentialHelper::GetRarityUpgradeChancePercent(CurrentRarity, MaxAchieved, UEmployeePotentialHelper::GetCubeCeiling(CardType))
			: 0.f;
		const bool bRises = Chance > 0.0001f;
		const FLinearColor ChanceInk = bRises ? PotInk::Value() : PotInk::Off();
		if (ChanceChips[i])
		{
			ChanceChips[i]->SetBrushColor(bRises ? ChanceChipRise : ChanceChipNone);
			ChanceChips[i]->SetVisibility(Employee ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (ChanceArrows[i]) ChanceArrows[i]->SetColorAndOpacity(FSlateColor(ChanceInk));
		if (ChanceTexts[i])
		{
			ChanceTexts[i]->SetText(FormatChancePercent(Chance));
			ChanceTexts[i]->SetColorAndOpacity(FSlateColor(ChanceInk));
		}
		if (Checks[i])
		{
			Checks[i]->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		ApplyTileStyle(i, bSelected);
	}
}

void UPotentialResetWorkspaceWidget::RefreshOddsAndAction(const FEmployeeInstance* Employee)
{
	const ELootBoxRarity CurrentRarity = Employee ? Employee->PotentialAbility.CurrentRarity : ELootBoxRarity::Common;
	const ELootBoxRarity MaxAchieved = Employee ? Employee->PotentialAbility.MaxAchievedRarity : ELootBoxRarity::Common;
	const ELootBoxRarity Ceiling = UEmployeePotentialHelper::GetCubeCeiling(SelectedCard);
	const float Chance = UEmployeePotentialHelper::GetRarityUpgradeChancePercent(CurrentRarity, MaxAchieved, Ceiling);
	const bool bCanRise = Employee && Chance > 0.0001f;
	const bool bHighestRarity = Employee && CurrentRarity >= UEmployeePotentialHelper::GetCubeCeiling(EItemType::BusinessCardBlack);

	if (OddsLabelText)
	{
		OddsLabelText->SetText(!Employee ? FText::GetEmpty() : bHighestRarity ? FText::FromString(TEXT("최고 등급 달성"))
			: bCanRise ? RiseLabelText : FText::FromString(TEXT("상위 명함으로 등급 상승")));
		FLinearColor LabelInk = bHighestRarity
			? FLootBoxRarityUtility::GetRarityInkOnLight(ELootBoxRarity::Legendary) * 0.18f : PotInk::Mute();
		LabelInk.A = 1.f;
		OddsLabelText->SetColorAndOpacity(FSlateColor(LabelInk));
	}
	if (OddsGradeRow)
	{
		OddsGradeRow->SetVisibility(bCanRise ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bCanRise)
	{
		// 상승 가능 = 현재 < 명함 상한 ≤ 레전드 이므로 다음 등급이 항상 존재한다
		const ELootBoxRarity NextRarity = static_cast<ELootBoxRarity>(static_cast<uint8>(CurrentRarity) + 1);
		if (OddsFromChip) OddsFromChip->SetBrushColor(GradeChipFill(CurrentRarity));
		if (OddsToChip) OddsToChip->SetBrushColor(GradeChipFill(NextRarity));
		if (OddsFromText)
		{
			OddsFromText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(CurrentRarity)));
			OddsFromText->SetColorAndOpacity(FSlateColor(PotInk::Body()));
		}
		if (OddsToText)
		{
			OddsToText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(NextRarity)));
			OddsToText->SetColorAndOpacity(FSlateColor(PotInk::Body()));
		}
	}
	if (OddsValueText)
	{
		OddsValueText->SetText(bCanRise ? FormatChancePercent(Chance) : FText::GetEmpty());
		OddsValueText->SetColorAndOpacity(FSlateColor(PotInk::Value()));
		OddsValueText->SetVisibility(bCanRise ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (OddsSubText)
	{
		const bool bShowSub = Employee && !bCanRise;
		OddsSubText->SetText(bShowSub ? FText::FromString(TEXT("옵션 값만 다시 정해집니다")) : FText::GetEmpty());
		OddsSubText->SetVisibility(bShowSub ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (OddsSubSeparator) OddsSubSeparator->SetVisibility(OddsSubText->GetVisibility());
	}

	const int32 Count = ItemManager ? ItemManager->GetItemCount(SelectedCard) : 0;
	const bool bBuyMode = Employee && Count <= 0;
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FShopItemTable SelectedDefinition;
	const bool bHasSelectedDefinition = TableMgr && TableMgr->GetShopItemByItemType(SelectedCard, SelectedDefinition);

	if (ResetLabelText)
	{
		FText Label = NSLOCTEXT("PotentialResetWorkspace", "ResetAction", "재설정");
		if (bBuyMode)
		{
			Label = bHasSelectedDefinition
				? FText::Format(NSLOCTEXT("PotentialResetWorkspace", "BuyCard", "{0} 구매"), SelectedDefinition.DisplayName)
				: NSLOCTEXT("PotentialResetWorkspace", "BuyCardFallback", "명함 구매");
		}
		ResetLabelText->SetText(Label);
		ResetLabelText->SetColorAndOpacity(FSlateColor(Employee ? FLinearColor::White : DrawerResetLabelDim));
	}
	if (ResetCostChip)
	{
		ResetCostChip->SetVisibility(Employee && !bBuyMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ResetCostIcon)
	{
		ResetCostIcon->SetBrushResourceObject(bHasSelectedDefinition ? SelectedDefinition.ResolveIconResource() : nullptr);
	}
	if (ResetCostText)
	{
		ResetCostText->SetText(FText::AsNumber(CardsPerReroll));
	}
	if (ResetShopIcon)
	{
		ResetShopIcon->SetVisibility(bBuyMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ResetButton)
	{
		// 0장도 버튼은 산다 — 구매로 이어지는 다음 행동이 그 자리에 있어야 막힌 화면이 안 된다
		ResetButton->SetIsEnabled(Employee != nullptr);
	}
}

void UPotentialResetWorkspaceWidget::HandleCardClicked(int32 CardIndex)
{
	if (CardIndex < 0 || CardIndex >= 3)
	{
		return;
	}

	// 재고가 없어도 확률을 살펴보고 구매할 명함을 고를 수 있다.
	SelectedCard = CardTypes[CardIndex];

	const FEmployeeInstance* Employee = GetEmployee();
	RefreshCards(Employee);
	RefreshOddsAndAction(Employee);
}

void UPotentialResetWorkspaceWidget::HandleResetClicked()
{
	if (!bPresentationActive || EmployeeID < 0 || !EmployeeManager || bRerollRequestInFlight)
	{
		return;
	}

	if (!ItemManager || ItemManager->GetItemCount(SelectedCard) <= 0)
	{
		OnRequestShop.ExecuteIfBound();
		return;
	}

	const FEmployeeInstance* PreEmployee = GetEmployee();
	const ELootBoxRarity PreRarity = PreEmployee ? PreEmployee->PotentialAbility.CurrentRarity : ELootBoxRarity::Common;
	const int32 RequestEmployeeID = EmployeeID;
	const EItemType RequestCard = SelectedCard;
	const uint32 RequestGeneration = PresentationGeneration;

	// 응답 뒤 연출에서 이전 줄이 빠지는 모습을 보여 주려면, 갱신 델리게이트가 덮기 전의 화면이 필요하다
	for (int32 i = 0; i < 3; ++i) RerollOldLines[i] = MakeLineView(i, PreEmployee);

	bRerollRequestInFlight = true;
	RerollWaitElapsed = 0.f;

	// 소모는 서버가 한다 — 리롤(RNG)은 성공 응답 뒤에만 돈다.
	// 실패 원인은 코드로 갈린다: 명함 부족만 "부족합니다", 잠금/네트워크/서버는 3상태·DT 문구(1-B-4c)
	EmployeeManager->RequestPotentialReroll(RequestEmployeeID, RequestCard,
		FOnCloudEconomyOpDone::CreateWeakLambda(this, [this, PreRarity, RequestEmployeeID, RequestCard, RequestGeneration](bool bOk, FName Code)
	{
		bRerollRequestInFlight = false;
		RerollWaitElapsed = -1.f;
		if (ResetButton) ResetButton->SetRenderOpacity(1.f);

		if (!bOk)
		{
			if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
			{
				UIMgr->NotifyCloudOpFailureWithText(Code,
					NSLOCTEXT("PotentialResetWorkspace", "ResetNoCard", "명함이 부족합니다"));
			}
			return;
		}

		// 요청 중 직원 변경·상점 이동·닫힘이 있었다면 데이터 갱신만 받고 이전 화면의 연출은 버린다.
		if (bPresentationActive && EmployeeID == RequestEmployeeID && PresentationGeneration == RequestGeneration)
		{
			PlayRerollFx(PreRarity, RequestCard);
		}
	}));
}

void UPotentialResetWorkspaceWidget::HandleEconomyReady(bool bReady)
{
	// 잠긴 동안에도 버튼은 살려 둔다 — 조용히 죽은 버튼은 "명함이 사라졌다" 로 읽힌다.
	// 해제되면 서버값으로 정정된 명함 재고로 카드·확률·CTA 를 다시 계산한다
	RefreshAll();
}

void UPotentialResetWorkspaceWidget::PlayRerollFx(ELootBoxRarity PreRarity, EItemType UsedCard)
{
	// 갱신(델리게이트)은 이미 끝난 뒤다 — 연출은 화면만 움직인다
	const FEmployeeInstance* PostEmployee = GetEmployee();
	const ELootBoxRarity NewRarity = PostEmployee ? PostEmployee->PotentialAbility.CurrentRarity : PreRarity;
	bRerollFxSilent = false;
	StartRerollFx(NewRarity > PreRarity, PreRarity, NewRarity, UsedCard);
}

#if !UE_BUILD_SHIPPING
void UPotentialResetWorkspaceWidget::DebugSeekRerollFx(bool bTierUp, float Time, int32 ForcedTopLineMask)
{
	const FEmployeeInstance* Employee = GetEmployee();
	if (!Employee) return;
	for (int32 i = 0; i < 3; ++i) RerollOldLines[i] = MakeLineView(i, Employee);
	bRerollFxSilent = true;
	const ELootBoxRarity Current = Employee->PotentialAbility.CurrentRarity;
	const ELootBoxRarity Previous = Current == ELootBoxRarity::Common ? Current : static_cast<ELootBoxRarity>(static_cast<uint8>(Current) - 1);
	StartRerollFx(bTierUp, Previous, Current, SelectedCard);
	if (ForcedTopLineMask >= 0) RerollTopLineMask = static_cast<uint8>(ForcedTopLineMask & 7);
	bRerollFxFrozen = true;
	ApplyRerollFx(FMath::Max(0.f, Time));
}

void UPotentialResetWorkspaceWidget::DebugEndRerollFx()
{
	bRerollFxFrozen = false;
	bRerollFxSilent = false;
	ResetRerollFxVisuals();
	RefreshAll();
}

bool UPotentialResetWorkspaceWidget::DebugIsRerollFxAtRest(FString& OutDetail) const
{
	TArray<FString> Dirty;
	auto Expect = [&Dirty](const UWidget* Layer, bool bHidden)
	{
		if (Layer && !PotFxIsRest(Layer, bHidden)) Dirty.Add(Layer->GetName());
	};
	const UWidget* Visible[] = { PotLine0, PotLine1, PotLine2, TierChipBorder, CardTile0, CardTile1, CardTile2,
		CardChanceChip0, CardChanceChip1, CardChanceChip2, OddsLabelText, OddsValueText, OddsSubText, OddsGradeRow };
	for (const UWidget* Layer : Visible) Expect(Layer, false);
	Expect(RerollCostFloat, true);
	for (int32 i = 0; i < 3; ++i)
	{
		Expect(LineRows[i].Get(), false);
		Expect(LineGlows[i].Get(), true);
		Expect(LineShines[i].Get(), true);
		Expect(LineSparks[i][0].Get(), true);
		Expect(LineSparks[i][1].Get(), true);
	}
	int32 LineLayers = 0;
	for (int32 i = 0; i < 3; ++i) LineLayers += (LineRows[i].IsValid() ? 1 : 0) + (LineGlows[i].IsValid() ? 1 : 0) + (LineShines[i].IsValid() ? 1 : 0)
		+ (LineSparks[i][0].IsValid() ? 1 : 0) + (LineSparks[i][1].IsValid() ? 1 : 0);
	OutDetail = FString::Printf(TEXT("canvas=%d linelayers=%d overlay=%d dirty=[%s]"), RerollFxCanvas ? 1 : 0, LineLayers,
		TierUpOverlay.IsValid() ? 1 : 0, *FString::Join(Dirty, TEXT(",")));
	return Dirty.IsEmpty() && RerollFxElapsed < 0.f && !TierUpOverlay.IsValid();
}
#endif

void UPotentialResetWorkspaceWidget::HandleOddsClicked()
{
	OnRequestOddsTable.ExecuteIfBound(EmployeeID);
}

void UPotentialResetWorkspaceWidget::HandleShopClicked()
{
	OnRequestShop.ExecuteIfBound();
}

void UPotentialResetWorkspaceWidget::HandleStatsChanged(int32 ChangedEmployeeID)
{
	if (ChangedEmployeeID != EmployeeID)
	{
		return;
	}

	const FEmployeeInstance* Employee = GetEmployee();
	RefreshTier(Employee);
	RefreshLines(Employee);
	// 등급이 바뀌면 명함별 상승 확률도 바뀐다
	RefreshCards(Employee);
	RefreshOddsAndAction(Employee);
}

void UPotentialResetWorkspaceWidget::HandleItemChanged(EItemType Type, int32 NewCount, int32 Delta)
{
	if (Type != CardTypes[0] && Type != CardTypes[1] && Type != CardTypes[2])
	{
		return;
	}

	const FEmployeeInstance* Employee = GetEmployee();
	RefreshCards(Employee);
	RefreshOddsAndAction(Employee);
}
