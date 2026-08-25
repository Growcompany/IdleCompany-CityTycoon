// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/ResourceWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Enum/ResourceType.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include <Blueprint/SlateBlueprintLibrary.h>
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Engine/DataTable.h"
#include "Table/ResourceInfo.h"
#include "Components/Button.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "Enum/WidgetType.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"

namespace
{
	// 마일스톤 플래시 엔벨로프 길이
	constexpr float MilestoneDuration = 0.25f;
	// 마일스톤 사운드 최소 간격 — 치트/일괄 지급으로 여러 자릿수를 한 번에 뚫어도 1발
	constexpr double MilestoneSoundCooldown = 1.0;
}

UResourceWidget::UResourceWidget(const FObjectInitializer& FOI)	: Super(FOI)
{
}

void UResourceWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 레이아웃 설정 적용
	ApplyLayoutSettings();
	SetImage();
	SetAmountText();
}

void UResourceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	check(ResourceImage);
	check(ResourceAmountText);

	SetImage();
	// InGameLayerWidget에서 이미 SetValue로 ResourceAmount가 설정되어 있으므로
	// 이제 바인딩된 텍스트에 표시
	SetAmountText();

	// Tick 활성화 (아이콘 위치 계산을 위해)
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 투명 버튼 클릭 -> 재화 상세 툴팁 (WBP 에 InfoButton 있을 때만)
	if (InfoButton)
	{
		InfoButton->OnClicked.AddDynamic(this, &UResourceWidget::HandleInfoButtonClicked);
	}
}

void UResourceWidget::ApplyLayoutSettings()
{
	// SizeBox 크기 설정
	if (SizeBox_161)
	{
		SizeBox_161->SetMaxDesiredWidth(IconSize);
		SizeBox_161->SetMaxDesiredHeight(IconSize);

		// SizeBox 내부 패딩 설정
		if (USizeBoxSlot* SizeBoxSlot = Cast<USizeBoxSlot>(ResourceImage->Slot))
		{
			SizeBoxSlot->SetPadding(FMargin(IconPaddingLeft, IconPaddingTop, IconPaddingRight, IconPaddingBottom));
		}
	}

	// 텍스트 슬롯 설정
	if (HorizontalBox_26 && ResourceAmountText)
	{
		if (UHorizontalBoxSlot* TextSlot = Cast<UHorizontalBoxSlot>(ResourceAmountText->Slot))
		{
			TextSlot->SetPadding(FMargin(TextPaddingLeft, 0, 0, 0));

			// Fill 여부에 따라 슬롯 크기 설정
			if (bTextSlotFill)
			{
				TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				TextSlot->SetHorizontalAlignment(TextHorizontalAlignment);
			}
			else
			{
				TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				TextSlot->SetHorizontalAlignment(HAlign_Left);
			}
		}
	}

	// 폰트 크기 설정
	if (ResourceAmountText)
	{
		FSlateFontInfo FontInfo = ResourceAmountText->GetFont();
		FontInfo.Size = FontSize;
		ResourceAmountText->SetFont(FontInfo);

		// 폰트 색상은 UpdateTextColor()에서 처리
		UpdateTextColor();
	}
}

void UResourceWidget::UpdateTextColor()
{
	if (!ResourceAmountText)
	{
		return;
	}

	FLinearColor NewColor = TextColor;

	// 구매 불가능하면 빨간색으로 표시 (최우선)
	if (!bCanAfford)
	{
		NewColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); // 빨간색
	}
	// DT_Resource.UIColor 가 단일 진실 — bUseTypeColor 시 lookup
	else if (bUseTypeColor)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bOk = false;
				const FResourceInfo Info = TableMgr->GetResourceInfo(ResourceType, bOk);
				if (bOk)
				{
					NewColor = Info.UIColor;
				}
			}
		}
	}

	ResourceAmountText->SetColorAndOpacity(NewColor);
}

void UResourceWidget::SetCanAfford(bool bInCanAfford)
{
	bCanAfford = bInCanAfford;
	UpdateTextColor();
}

void UResourceWidget::NativeDestruct()
{
	if (InfoButton)
	{
		InfoButton->OnClicked.RemoveDynamic(this, &UResourceWidget::HandleInfoButtonClicked);
	}

	// 재사용 위젯이 플래시/트랜스폼이 걸린 채로 재등장하지 않도록 채널 정리
	MilestoneElapsed = -1.f;
	BumpElapsed = -1.f;
	ShakeElapsed = -1.f;
	ApplyChipLineFlash(0.f);
	if (bFeedbackXformApplied)
	{
		SetRenderTransform(FWidgetTransform());
		bFeedbackXformApplied = false;
	}

	// 펀치 대상 해석은 UpdateFeedbackAnim 과 동일해야 한다 (ScaleBox 래퍼 우선)
	if (ResourceAmountText)
	{
		UWidget* PunchTarget = ResourceAmountText;
		if (UScaleBox* Wrapper = Cast<UScaleBox>(ResourceAmountText->GetParent()))
		{
			PunchTarget = Wrapper;
		}
		PunchTarget->SetRenderScale(FVector2D(1.f, 1.f));
	}

	Super::NativeDestruct();
}

void UResourceWidget::PlayBump(float InPeak)
{
	// 연속 도착(벽돌 버스트 등) 시 재시작하면 스케일이 상승 도중 1.0으로 뚝뚝 끊겨 떨림 — 재생 중이면 완주.
	// 단 더 큰 진폭 요청(마일스톤 등)은 피크만 승격 — 타이머(BumpElapsed)는 건드리지 않아 떨림이 재발하지 않는다.
	if (BumpElapsed >= 0.f)
	{
		if (InPeak > BumpPeak)
		{
			BumpPeak = InPeak;
		}
		return;
	}
	BumpPeak = InPeak;
	BumpElapsed = 0.f;
}

void UResourceWidget::PlayAffordShake()
{
	ShakeElapsed = 0.f;
}

void UResourceWidget::UpdateFeedbackAnim(float DeltaTime)
{
	if (BumpElapsed < 0.f && ShakeElapsed < 0.f && MilestoneElapsed < 0.f && !bFeedbackXformApplied)
	{
		return;
	}

	// 펀치 타겟 = 텍스트를 감싼 ScaleBox 래퍼(있으면) — 클립/핏 판정 좌표계 밖에서 스케일돼 글자 잘림 원천 차단 (2026-07-10)
	UWidget* PunchTarget = ResourceAmountText;
	if (ResourceAmountText)
	{
		if (UScaleBox* Wrapper = Cast<UScaleBox>(ResourceAmountText->GetParent()))
		{
			PunchTarget = Wrapper;
		}
	}

	if (BumpElapsed >= 0.f)
	{
		BumpElapsed += DeltaTime;
		const float A = FMath::Clamp(BumpElapsed / 0.26f, 0.f, 1.f);
		const float Scale = 1.f + BumpPeak * FMath::Sin(A * PI);   // 1.0 -> 1+Peak -> 1.0
		if (PunchTarget)
		{
			PunchTarget->SetRenderScale(A >= 1.f ? FVector2D(1.f, 1.f) : FVector2D(Scale, Scale));
		}
		if (A >= 1.f) BumpElapsed = -1.f;
	}

	// 루트 렌더 트랜스폼은 자금부족 셰이크 이동만 담당한다. 숫자 펀치와 마일스톤 플래시는 각 대상에서 독립 처리한다.
	FWidgetTransform Xform;
	bool bXformNeeded = false;

	// 자금부족 셰이크 = 위젯 전체 감쇠 좌우 진동
	if (ShakeElapsed >= 0.f)
	{
		ShakeElapsed += DeltaTime;
		const float A = FMath::Clamp(ShakeElapsed / 0.3f, 0.f, 1.f);
		if (A >= 1.f)
		{
			ShakeElapsed = -1.f;
		}
		else
		{
			Xform.Translation = FVector2D(FMath::Sin(A * PI * 6.f) * 8.f * (1.f - A), 0.f);
			bXformNeeded = true;
		}
	}

	// 자릿수 승격 = ChipLine 화이트 플래시. 숫자 펀치는 독립 Bump 채널에서 합성된다.
	if (MilestoneElapsed >= 0.f)
	{
		MilestoneElapsed += DeltaTime;
		const float A = FMath::Clamp(MilestoneElapsed / MilestoneDuration, 0.f, 1.f);
		if (A >= 1.f)
		{
			MilestoneElapsed = -1.f;
			ApplyChipLineFlash(0.f);
		}
		else
		{
			const float Env = FMath::Sin(A * PI);
			ApplyChipLineFlash(Env);
		}
	}

	if (bXformNeeded)
	{
		SetRenderTransform(Xform);
		bFeedbackXformApplied = true;
	}
	else if (bFeedbackXformApplied)
	{
		SetRenderTransform(FWidgetTransform());   // 종료 시 1회만 리셋
		bFeedbackXformApplied = false;
	}
}

void UResourceWidget::ApplyChipLineFlash(float Intensity)
{
	// ChipLine 이 없는 계보(UIE_Resource 원본 등)는 펀치만 — 플래시는 조용히 스킵
	if (!ChipLine)
	{
		return;
	}

	if (!bChipLineBaseCached)
	{
		ChipLineBaseColor = ChipLine->GetColorAndOpacity();
		bChipLineBaseCached = true;
	}

	// MID 스칼라 파라미터 대신 위젯 틴트 — 파라미터명이 어긋나도 조용히 죽지 않는다
	if (Intensity <= 0.f)
	{
		ChipLine->SetColorAndOpacity(ChipLineBaseColor);
		return;
	}
	ChipLine->SetColorAndOpacity(ChipLineBaseColor + FLinearColor(1.6f, 1.6f, 1.6f, 0.f) * Intensity);
}

int32 UResourceWidget::GetDigitTier(int64 Value)
{
	// log10 정수부 = 자릿수-1. 부동소수 경계 오차를 피하려고 정수 나눗셈으로 센다.
	int64 Remain = (Value < 0) ? -Value : Value;
	int32 Tier = 0;
	while (Remain >= 10)
	{
		Remain /= 10;
		++Tier;
	}
	return Tier;
}

float UResourceWidget::ComputeRollupDuration(int64 Delta)
{
	const float LogDelta = FMath::LogX(10.f, static_cast<float>(FMath::Max<int64>(Delta, 1)));
	// 상한 0.6초 — 1초 드립 케이던스보다 짧아야 숫자가 영구히 구르지 않는다
	return FMath::Clamp(0.35f + 0.08f * LogDelta, 0.35f, 0.6f);
}

bool UResourceWidget::ShouldRollup(int64 NewAmount) const
{
	// bShouldCalculateIconPos = HUD 등록 경로(InGameLayerWidget) — WBP 플래그 없이도 상단 HUD 칩은 롤업 대상
	if (!bLiveCounterRollup && !bShouldCalculateIconPos)
	{
		return false;
	}
	// 벽돌은 2페이즈 분출/흡수 연출이 이미 피드백을 담당
	if (ResourceType != EResourceType::Money && ResourceType != EResourceType::Diamond)
	{
		return false;
	}
	if (bShowAsFraction)
	{
		return false;
	}
	// 최초 채움(0 -> 실보유)은 즉시 — 레벨 진입마다 HUD 가 도는 걸 막는다
	if (PeakDigitTier == INDEX_NONE)
	{
		return false;
	}

	const int64 Delta = NewAmount - ResourceAmount;
	if (Delta < 1000)
	{
		return false;   // 소액 유입과 하강(지출)은 즉시 대입
	}
	const int64 TwoPercent = (ResourceAmount < 0 ? -ResourceAmount : ResourceAmount) / 50;
	return Delta >= TwoPercent;
}

void UResourceWidget::EvaluateDigitMilestone(int64 DisplayedValue)
{
	const int32 NewTier = GetDigitTier(DisplayedValue);

	if (PeakDigitTier == INDEX_NONE)
	{
		PeakDigitTier = NewTier;   // 최초 표시값으로 시딩 — 세이브 로드 직후 발화 방지
		return;
	}
	// 하강은 전부 무음·무연출. 지출로 내려간 뒤 되올라오는 재교차도 무시(피크 초과만 1회 발화).
	if (NewTier <= PeakDigitTier)
	{
		return;
	}

	PeakDigitTier = NewTier;
	PlayMilestoneFeedback(NewTier);
}

void UResourceWidget::PlayMilestoneFeedback(int32 NewTier)
{
	PlayBump(0.10f);
	MilestoneElapsed = 0.f;

	// 한글 단위 경계(만 1e4 / 억 1e8 / 조 1e12 / 경 1e16)에서만 사운드 — 그 외 자릿수는 시각 연출만
	const bool bUnitBoundary = (NewTier == 4 || NewTier == 8 || NewTier == 12 || NewTier == 16);
	if (!bUnitBoundary)
	{
		return;
	}

	const UWorld* WorldPtr = GetWorld();
	const double Now = WorldPtr ? WorldPtr->GetTimeSeconds() : 0.0;
	if (Now - LastMilestoneSoundTime < MilestoneSoundCooldown)
	{
		return;
	}
	LastMilestoneSoundTime = Now;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlayUISound(CGUISoundTags::ResourceDigitMilestone);
		}
	}
}

void UResourceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateChipMaterialSize(MyGeometry);

	UpdateFeedbackAnim(InDeltaTime);

	if (CountUpAnim.IsPlaying())
	{
		const float Alpha = CountUpAnim.Tick(InDeltaTime);
		ResourceAmount = CountFrom + (int64)FMath::RoundToDouble((double)(CountTo - CountFrom) * (double)Alpha);
		SetAmountText();
		// 마일스톤은 표시값이 실제로 경계를 넘는 순간에 터져야 해서 롤업 틱에서 평가
		EvaluateDigitMilestone(ResourceAmount);
	}

	// 아이콘 위치 계산이 필요없거나 이미 계산되었으면 더 이상 Tick 하지 않음
	if (!bShouldCalculateIconPos || bIconPosCalculated)
	{
		return;
	}

	// MyGeometry로 직접 크기 확인 (더 효율적)
	FVector2D LocalSize = MyGeometry.GetLocalSize();

	// 레이아웃 완료 확인 (부모 위젯이 레이아웃됨)
	if (LocalSize.X > 0.0f && LocalSize.Y > 0.0f && ResourceImage)
	{
		// 위치 계산 시도
		bool bSuccess = CalculateIconScreenPos();

		// 성공했을 때만 Tick 비활성화
		if (bSuccess)
		{
			bIconPosCalculated = true;
			UE_LOG(LogTemp, Log, TEXT("[ResourceWidget] Icon position calculated via Tick (DeltaTime=%.4f), disabling Tick"),
				InDeltaTime);
		}
		// 실패하면 다음 프레임에 재시도 (bIconPosCalculated는 false 유지)
	}
}

void UResourceWidget::UpdateChipMaterialSize(const FGeometry& MyGeometry)
{
	if (!ChipBG && !ChipLine)
	{
		return;
	}

	// SDF 머티리얼은 Wpx/Hpx 가 실제 위젯 크기와 일치해야 코너가 정합 — 크기 변화 시에만 재주입
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastChipMatSize, 0.5f))
	{
		return;
	}
	LastChipMatSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UImage* ChipImage : { ChipBG, ChipLine })
	{
		if (!ChipImage)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = ChipImage->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(WpxParam, LocalSize.X);
			MID->SetScalarParameterValue(HpxParam, LocalSize.Y);
		}
	}
}

void UResourceWidget::SetAmountText() const
{
	if (IsValid(ResourceAmountText))
	{
		if (bShowAsFraction && ResourceMaxAmount >= 0)
		{
			// 분수 형식: 현재/최대 (축약 표기). 분모(최대)는 항상 Floor — 실제 용량 표현(Ceil 비용 모드여도 분모는 올리지 않음).
			const FString FractionStr = UGlobalUtilFunctions::AbbreviateNumber(ResourceAmount, ENumberAbbrevStyle::Auto, AmountRoundMode).ToString()
				+ TEXT("/") + UGlobalUtilFunctions::AbbreviateNumber(ResourceMaxAmount, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString();
			ResourceAmountText->SetText(FText::FromString(FractionStr));
		}
		else
		{
			// 일반 형식: 값만 표시 (AmountRoundMode 적용 — 보유=Floor, 비용=Ceil)
			ResourceAmountText->SetText(UGlobalUtilFunctions::AbbreviateNumber(ResourceAmount, ENumberAbbrevStyle::Auto, AmountRoundMode));
		}
	}
}

void UResourceWidget::SetImage()
{
	if (!IsValid(ResourceImage))
	{
		return;
	}

	if (ResourceType == EResourceType::None)
	{
		return;
	}

	// 런타임: TableManagerSubsystem 경유
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
		if (TableManager)
		{
			bool bSuccess = false;
			FResourceInfo ResourceInfo = TableManager->GetResourceInfo(ResourceType, bSuccess);
			if (bSuccess && ResourceInfo.Icon.ToSoftObjectPath().IsValid())
			{
				UTexture2D* IconTexture = ResourceInfo.Icon.LoadSynchronous();
				if (IconTexture)
				{
					ResourceImage->SetBrushFromTexture(IconTexture);
					return;
				}
			}
		}
	}

	// 에디터 디자인 타임 fallback: DataTable을 직접 로드하여 아이콘 설정
	UDataTable* ResourceDT = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/DT_Resource.DT_Resource"));
	if (!ResourceDT)
	{
		return;
	}

	TArray<FName> RowNames = ResourceDT->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FResourceInfo* Row = ResourceDT->FindRow<FResourceInfo>(RowName, TEXT("ResourceWidget::SetImage"));
		if (Row && Row->ResourceType == ResourceType)
		{
			if (Row->Icon.ToSoftObjectPath().IsValid())
			{
				UTexture2D* IconTexture = Row->Icon.LoadSynchronous();
				if (IconTexture)
				{
					ResourceImage->SetBrushFromTexture(IconTexture);
				}
			}
			return;
		}
	}
}

void UResourceWidget::SetResourceType(EResourceType type, bool bInUseTypeColor)
{
	ResourceType = type;
	bUseTypeColor = bInUseTypeColor;
	SetImage();
	UpdateTextColor();
}

void UResourceWidget::SetValue(int64 amount)
{
	// 굴러가는 중 소액이 겹쳐 들어오면 트윈을 끊지 말고 목표만 갱신 — 매번 스냅하면 롤업이 잘려 보인다
	if (CountUpAnim.IsPlaying() && amount > ResourceAmount)
	{
		CountTo = amount;
		return;
	}

	if (ShouldRollup(amount))
	{
		SetValueAnimated(amount, ComputeRollupDuration(amount - ResourceAmount));
		return;
	}

	// 롤업 조건 미달 = 즉시 대입. 굴러가는 중이었으면 끊고 목표값으로 스냅.
	if (CountUpAnim.IsPlaying())
	{
		CountUpAnim.Finish();
	}

	if (ResourceAmount != amount)
	{
		ResourceAmount = amount;
		SetAmountText();
		EvaluateDigitMilestone(ResourceAmount);
	}
}

void UResourceWidget::SetValueAnimated(int64 amount, float Duration)
{
	// 재생 중 재호출은 목표값만 갱신 — 매번 재시작하면 도착이 계속 밀려 숫자가 영구히 구른다
	if (CountUpAnim.IsPlaying())
	{
		CountTo = amount;
		return;
	}
	if (amount == ResourceAmount)
	{
		return;
	}
	CountFrom = ResourceAmount;
	CountTo = amount;
	CountUpAnim.Start(0.f, 1.f, Duration);
}

void UResourceWidget::SetValueWithMax(int64 current, int64 max)
{
	bShowAsFraction = true;
	ResourceAmount = current;
	ResourceMaxAmount = max;
	SetAmountText();
}

void UResourceWidget::SetItemDisplay(UTexture2D* IconTexture, int64 Count)
{
	// 재화가 아닌 아이템(뽑기권) 표시. ResourceType=None 으로 두어 이후 SetImage() 가 재화 아이콘으로 덮지 않게 하고,
	// 타입색 대신 중립 TextColor 를 쓴다(bUseTypeColor=false).
	ResourceType = EResourceType::None;
	bUseTypeColor = false;

	if (IsValid(ResourceImage) && IsValid(IconTexture))
	{
		ResourceImage->SetBrushFromTexture(IconTexture);
	}

	ResourceAmount = Count;
	SetAmountText();
	UpdateTextColor();
}

void UResourceWidget::HandleInfoButtonClicked()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		return;
	}

	bool bOk = false;
	const FResourceInfo Info = TableMgr->GetResourceInfo(ResourceType, bOk);
	if (!bOk)
	{
		return;
	}

	// "보유 {정확수치}" + 설명(있으면 한 줄 띄움). 설명 비면 보유줄만.
	// 유량(원/초)은 이 툴팁의 담당이 아니다 — HUD 수익 칩이 상시 표시한다(같은 숫자에 두 집을 주지 않는다).
	const FText Exact = UGlobalUtilFunctions::FormatExactNumber(ResourceAmount);
	const FText Held = FText::Format(FText::FromString(TEXT("보유 {0}")), Exact);

	const FText Desc = Info.Description.IsEmpty()
		? Held
		: FText::Format(FText::FromString(TEXT("{0}\n{1}")), Held, Info.Description);

	UTexture2D* Icon = Info.Icon.IsNull() ? nullptr : Info.Icon.LoadSynchronous();

	// 앵커 = 유저가 누른 지점. 그 지점에서 그대로 아래로 드롭 (Slate absolute = ShowAt 좌표계 동일).
	const FVector2D AbsPos = UGlobalUtilFunctions::GetPointerAbsolutePosition();

	// 툴팁 인스턴스 재사용 (없으면 1회 생성). ShowAt 이 AddToViewport/위치/자동dismiss 처리.
	if (!ActiveTooltip)
	{
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr->GetWidgetClass(EWidgetType::ItemTooltip);
		if (!TooltipClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ResourceWidget] EWidgetType::ItemTooltip 미등록"));
			return;
		}
		ActiveTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
	}

	if (ActiveTooltip)
	{
		ActiveTooltip->ShowAt(AbsPos, Info.DisplayName, Icon, Desc, /*BasePrice*/0, /*Duration*/2.5f, ETooltipAnchor::BelowAnchor);
	}
}

bool UResourceWidget::CalculateIconScreenPos()
{
	if (!ResourceImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ResourceWidget] CalculateIconScreenPos: ResourceImage is null!"));
		return false;
	}

	const FGeometry& Geo = ResourceImage->GetCachedGeometry();
	FVector2D LocalSize = Geo.GetLocalSize();

	// 위젯이 아직 레이아웃되지 않았으면 실패 반환 (NativeTick에서 자동 재시도됨)
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ResourceWidget] CalculateIconScreenPos: ResourceImage not laid out yet (Size: %s)"),
			*LocalSize.ToString());
		return false;
	}

	// 1) 로컬 공간에서 위젯(아이콘) 크기의 절반을 구해서 '센터' 위치로 삼음
	FVector2D LocalCenter = LocalSize * 0.5f;

	// 2) Absolute (스크린) 좌표로 변환 (ScaleBox 등 모든 변환 적용됨)
	FVector2D AbsolutePos = Geo.LocalToAbsolute(LocalCenter);

	// 3) Absolute 좌표로 저장 (정석 방식)
	CachedIconScreenPos = AbsolutePos;

	UE_LOG(LogTemp, Log, TEXT("[ResourceWidget] Icon position calculated successfully: Type=%d, LocalSize=%s, AbsolutePos=%s"),
		(int32)ResourceType, *LocalSize.ToString(), *CachedIconScreenPos.ToString());

	return true;  // 성공
}
