// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/GlobalUtilFunctions.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/BorderSlot.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Fonts/FontMeasure.h"

FVector UGlobalUtilFunctions::SteppedPosition(const FVector& position)
{
	FVector retVal = position / 200.f;
	retVal.X = FMath::RoundToFloat(retVal.X) * 200.f;
	retVal.Y = FMath::RoundToFloat(retVal.Y) * 200.f;
	retVal.Z = 0.f;

	return retVal;
}

FString UGlobalUtilFunctions::GenerateRandomStringWithSeed(int32 Length)
{
	FString RandomString;
	const FString Alphabet = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

	for (int32 i = 0; i < Length; ++i)
	{
		int32 Index = FMath::RandRange(0, Alphabet.Len() - 1);
		RandomString.AppendChar(Alphabet[Index]);
	}

	return RandomString;
}

namespace
{
	// 프로젝트 기본 축약 스타일 = 한국(만/억/조). EN 빌드/지역설정에서 이 값만 바꾸면 전체 전환.
	// (UE 현재 컬처가 en 으로 잡혀 있어 컬처 자동판단은 한국 게임에서 오작동 -> 명시적 기본값 사용)
	ENumberAbbrevStyle GDefaultAbbrevStyle = ENumberAbbrevStyle::KoreanUnit;

	// Auto = 프로젝트 기본값(GDefaultAbbrevStyle)을 따름. 명시 스타일은 그대로 사용.
	ENumberAbbrevStyle ResolveAbbrevStyle(ENumberAbbrevStyle Style)
	{
		return (Style == ENumberAbbrevStyle::Auto) ? GDefaultAbbrevStyle : Style;
	}

	// Reduce to N significant figures using the requested rounding mode (mantissa is small, float log is fine).
	double ApplySignificant(double Value, int32 SigFigs, ENumberRoundMode Mode)
	{
		if (Value == 0.0)
		{
			return 0.0;
		}
		const double Digits = FMath::FloorToDouble(FMath::LogX(10.0f, (float)FMath::Abs(Value))) + 1.0;
		const double Power = FMath::Pow(10.0, (double)SigFigs - Digits);
		const double Scaled = Value * Power;
		double Result;
		switch (Mode)
		{
		case ENumberRoundMode::Ceil:  Result = FMath::CeilToDouble(Scaled);  break;
		case ENumberRoundMode::Round: Result = FMath::RoundToDouble(Scaled); break;
		default:                      Result = FMath::FloorToDouble(Scaled); break; // Floor
		}
		return Result / Power;
	}

	// "%.2f" then strip trailing zeros/dot: 95.60 -> "95.6", 125.00 -> "125".
	FString TrimMantissa(double Value)
	{
		FString S = FString::Printf(TEXT("%.2f"), Value);
		if (S.Contains(TEXT(".")))
		{
			while (S.EndsWith(TEXT("0"))) { S.LeftChopInline(1); }
			if (S.EndsWith(TEXT("."))) { S.LeftChopInline(1); }
		}
		return S;
	}

	uint64 PowTen(int32 Exp)
	{
		uint64 P = 1ull;
		for (int32 i = 0; i < Exp; ++i) { P *= 10ull; }
		return P;
	}

	// 4자리 그룹 단위. 인덱스 = 10^(4*i) 자리.
	static const TCHAR* const KoreanGroupUnits[] = { TEXT(""), TEXT("만"), TEXT("억"), TEXT("조"), TEXT("경") };
	constexpr int32 KoreanGroupMax = UE_ARRAY_COUNT(KoreanGroupUnits);

	// INT64_MIN 은 단순 부호반전이 오버플로하므로 2의 보수로 절댓값을 만든다.
	uint64 AbsU64(int64 Value)
	{
		return (Value < 0) ? (~static_cast<uint64>(Value) + 1ull) : static_cast<uint64>(Value);
	}

	int32 CountGroups(uint64 Abs)
	{
		int32 Groups = 1;
		while (Abs >= 10000ull && Groups < KoreanGroupMax)
		{
			Abs /= 10000ull;
			++Groups;
		}
		return Groups;
	}

	// 같은 자릿수의 최악폭 모양 — 숫자는 전부 '8', 빈 그룹 생략 없음.
	// 실제 문자열은 이보다 넓어질 수 없어서 값이 변해도 고른 상세도가 유지된다.
	FString BuildDetailProbe(uint64 Abs, int32 TotalGroups, int32 UseGroups, bool bNegative)
	{
		const int32 TopDigits = FMath::Max(1, FString::Printf(TEXT("%llu"), Abs).Len() - 4 * (TotalGroups - 1));

		FString Probe;
		if (bNegative)
		{
			Probe.AppendChar(TEXT('-'));
		}
		for (int32 i = 0; i < UseGroups; ++i)
		{
			if (i > 0)
			{
				Probe.AppendChar(TEXT(' '));
			}
			Probe.Append(FString::ChrN((i == 0) ? TopDigits : 4, TEXT('8')));
			Probe.Append(KoreanGroupUnits[TotalGroups - 1 - i]);
		}
		return Probe;
	}
}

FText UGlobalUtilFunctions::AbbreviateNumber(int64 Value, ENumberAbbrevStyle Style, ENumberRoundMode RoundMode)
{
	const ENumberAbbrevStyle Resolved = ResolveAbbrevStyle(Style);

	// Korean groups by 10^4 (man/eok/jo/gyeong); Western by 10^3 (K/M/B/T/Qa/Qi).
	// Unit glyphs via unicode escapes: 만=man 억=eok 조=jo 경=gyeong
	static const TCHAR* const KoreanUnits[]  = { TEXT(""), TEXT("만"), TEXT("억"), TEXT("조"), TEXT("경") };
	static const TCHAR* const WesternUnits[] = { TEXT(""), TEXT("K"), TEXT("M"), TEXT("B"), TEXT("T"), TEXT("Qa"), TEXT("Qi") };

	const bool bKorean = (Resolved == ENumberAbbrevStyle::KoreanUnit);
	const int32 GroupDigits = bKorean ? 4 : 3;
	const TCHAR* const* Units = bKorean ? KoreanUnits : WesternUnits;
	const int32 UnitCount = bKorean ? UE_ARRAY_COUNT(KoreanUnits) : UE_ARRAY_COUNT(WesternUnits);

	// Sign-safe absolute value (handles INT64_MIN).
	const bool bNegative = Value < 0;
	const uint64 Abs = bNegative ? (~static_cast<uint64>(Value) + 1ull) : static_cast<uint64>(Value);

	// Below the first unit boundary, keep the raw comma-grouped number.
	const uint64 FirstUnit = bKorean ? 10000ull : 1000ull;
	if (Abs < FirstUnit)
	{
		return FText::AsNumber(Value);
	}

	// Pick the largest unit whose value does not exceed Abs.
	int32 UnitIdx = 0;
	uint64 UnitValue = 1ull;
	for (int32 i = 1; i < UnitCount; ++i)
	{
		const uint64 P = PowTen(GroupDigits * i);
		if (Abs >= P) { UnitValue = P; UnitIdx = i; }
		else { break; }
	}

	double Mantissa = ApplySignificant(static_cast<double>(Abs) / static_cast<double>(UnitValue), 3, RoundMode);

	// Rounding can push the mantissa onto the next unit (e.g. 9999.6 man -> 1 eok); promote once.
	const double GroupBoundary = static_cast<double>(PowTen(GroupDigits));
	if (Mantissa >= GroupBoundary && (UnitIdx + 1) < UnitCount)
	{
		++UnitIdx;
		UnitValue = PowTen(GroupDigits * UnitIdx);
		Mantissa = ApplySignificant(static_cast<double>(Abs) / static_cast<double>(UnitValue), 3, RoundMode);
	}

	FString Result = TrimMantissa(Mantissa) + Units[UnitIdx];
	if (bNegative)
	{
		Result = TEXT("-") + Result;
	}
	return FText::FromString(Result);
}

FText UGlobalUtilFunctions::FormatFundsAmount(int64 Value, bool bShowPositiveSign, ENumberRoundMode RoundMode)
{
	const FText FormattedAmount = AbbreviateNumber(Value, ENumberAbbrevStyle::KoreanUnit, RoundMode);
	return Value > 0 && bShowPositiveSign
		? FText::FromString(TEXT("+") + FormattedAmount.ToString())
		: FormattedAmount;
}

FText UGlobalUtilFunctions::AbbreviateNumberFloat(double Value, ENumberAbbrevStyle Style, ENumberRoundMode RoundMode)
{
	const ENumberAbbrevStyle Resolved = ResolveAbbrevStyle(Style);
	const double FirstUnit = (Resolved == ENumberAbbrevStyle::KoreanUnit) ? 10000.0 : 1000.0;

	if (FMath::Abs(Value) >= FirstUnit)
	{
		// 큰 값은 정수 경로로. Floor 모드면 절단 오차로 한 단위 줄지 않게 미리 같은 방향 보정.
		int64 IntValue;
		switch (RoundMode)
		{
		case ENumberRoundMode::Ceil:  IntValue = (int64)FMath::CeilToDouble(Value);  break;
		case ENumberRoundMode::Round: IntValue = (int64)FMath::RoundToDouble(Value); break;
		default:                      IntValue = (int64)FMath::FloorToDouble(Value); break;
		}
		return AbbreviateNumber(IntValue, Resolved, RoundMode);
	}

	// Small values: comma grouping with up to one fractional digit.
	FNumberFormattingOptions Opts;
	Opts.MinimumFractionalDigits = 0;
	Opts.MaximumFractionalDigits = 1;
	return FText::AsNumber(Value, &Opts);
}

void UGlobalUtilFunctions::SplitAbbreviatedNumber(int64 Value, FString& OutNumber, FString& OutUnit,
	ENumberAbbrevStyle Style, ENumberRoundMode RoundMode)
{
	OutNumber = AbbreviateNumber(Value, Style, RoundMode).ToString();
	OutUnit.Empty();

	if (OutNumber.IsEmpty())
	{
		return;
	}

	// 한글 단위는 항상 끝 한 글자. 서양식 접미사(K/M/Qa)는 자릿수가 달라 분리 대상이 아니다 —
	// 분리 조판 자체가 한글 단위 전제의 레이아웃이므로 미일치 시 통짜로 둔다.
	static const TCHAR* const SplitUnits[] = { TEXT("만"), TEXT("억"), TEXT("조"), TEXT("경") };
	for (const TCHAR* const Unit : SplitUnits)
	{
		if (OutNumber.EndsWith(Unit, ESearchCase::CaseSensitive))
		{
			OutUnit = Unit;
			OutNumber.LeftChopInline(FCString::Strlen(Unit));
			return;
		}
	}
}

FText UGlobalUtilFunctions::FormatExactNumber(int64 Value)
{
	// 콤마 그룹핑된 전체 수치 (예: 9,555,438,138). 축약 표기 클릭 시 "자세히" 표시용.
	return FText::AsNumber(Value);
}

FText UGlobalUtilFunctions::FormatKoreanUnitsDetailed(int64 Value, int32 MaxGroups)
{
	const uint64 Abs = AbsU64(Value);
	if (Abs < 10000ull)
	{
		return FText::AsNumber(Value);
	}

	const int32 TotalGroups = CountGroups(Abs);
	const int32 UseGroups = FMath::Clamp(MaxGroups, 1, TotalGroups);

	FString Out;
	int32 Printed = 0;
	for (int32 GroupIdx = TotalGroups - 1; GroupIdx >= 0 && Printed < UseGroups; --GroupIdx)
	{
		const uint64 GroupValue = (Abs / PowTen(4 * GroupIdx)) % 10000ull;
		if (GroupValue == 0ull)
		{
			continue; // "1억 0만 2342" 를 만들지 않는다 — 빈 그룹은 상세도를 소모하지 않고 건너뛴다
		}
		if (!Out.IsEmpty())
		{
			Out.AppendChar(TEXT(' '));
		}
		Out.Appendf(TEXT("%llu%s"), GroupValue, KoreanGroupUnits[GroupIdx]);
		++Printed;
	}

	return FText::FromString(Value < 0 ? TEXT("-") + Out : Out);
}

int32 UGlobalUtilFunctions::SelectAmountDetailGroups(int64 Value, float AvailableWidthPx,
	TFunctionRef<float(const FString&)> MeasureWidth)
{
	const uint64 Abs = AbsU64(Value);
	if (Abs < 10000ull || AvailableWidthPx <= 0.f)
	{
		return 0; // 단위가 없으면 상세표기와 축약이 같은 문자열 — 폴백이 곧 정답
	}

	const int32 TotalGroups = CountGroups(Abs);
	// 하한은 2그룹 — 1그룹("1억")은 축약("1.34억")보다 정보가 적다.
	for (int32 Groups = TotalGroups; Groups >= 2; --Groups)
	{
		if (MeasureWidth(BuildDetailProbe(Abs, TotalGroups, Groups, Value < 0)) <= AvailableWidthPx)
		{
			return Groups;
		}
	}
	return 0;
}

FText UGlobalUtilFunctions::FitAmountToWidth(int64 Value, float AvailableWidthPx, const FSlateFontInfo& Font,
	ENumberRoundMode FallbackRoundMode)
{
	// 상세표기는 한국식 단위 전제 — 서양식으로 전환된 빌드에서는 축약만 쓴다.
	const bool bCanMeasure = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer();
	if (!bCanMeasure || ResolveAbbrevStyle(ENumberAbbrevStyle::Auto) != ENumberAbbrevStyle::KoreanUnit)
	{
		return AbbreviateNumber(Value, ENumberAbbrevStyle::Auto, FallbackRoundMode);
	}

	const TSharedRef<FSlateFontMeasure> Measurer = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const int32 Groups = SelectAmountDetailGroups(Value, AvailableWidthPx,
		[&Measurer, &Font](const FString& Candidate)
		{
			return static_cast<float>(Measurer->Measure(Candidate, Font).X);
		});

	return (Groups > 0)
		? FormatKoreanUnitsDetailed(Value, Groups)
		: AbbreviateNumber(Value, ENumberAbbrevStyle::Auto, FallbackRoundMode);
}

FText UGlobalUtilFunctions::FormatDurationKorean(double Seconds)
{
	// 올림 — 남은 시간을 실제보다 짧게 약속하지 않는다.
	const int64 Total = static_cast<int64>(FMath::CeilToDouble(FMath::Max(0.0, Seconds)));

	if (Total < 60)
	{
		return FText::FromString(FString::Printf(TEXT("%lld초"), Total));
	}
	if (Total < 3600)
	{
		const int64 Mins = Total / 60;
		const int64 Secs = Total % 60;
		return FText::FromString(Secs > 0
			? FString::Printf(TEXT("%lld분 %lld초"), Mins, Secs)
			: FString::Printf(TEXT("%lld분"), Mins));
	}

	const int64 Hours = Total / 3600;
	const int64 RemMins = (Total % 3600) / 60;
	return FText::FromString(RemMins > 0
		? FString::Printf(TEXT("%lld시간 %lld분"), Hours, RemMins)
		: FString::Printf(TEXT("%lld시간"), Hours));
}

FText UGlobalUtilFunctions::FormatEstimateDurationKorean(double Seconds)
{
	double Rounded = FMath::Max(0.0, Seconds);
	if (Rounded < 600.0)        { Rounded = FMath::Max(10.0, FMath::RoundToDouble(Rounded / 10.0) * 10.0); }
	else if (Rounded < 3600.0)  { Rounded = FMath::RoundToDouble(Rounded / 60.0) * 60.0; }
	else                        { Rounded = FMath::RoundToDouble(Rounded / 600.0) * 600.0; }

	return FormatDurationKorean(Rounded);
}

FString UGlobalUtilFunctions::FormatRatePerBestUnit(int64 PerSecond, float SecondsPerEvent)
{
	// 말한 기간 안에 적립이 한 번은 들어오는 가장 작은 단위를 고른다.
	const float Interval = FMath::Max(SecondsPerEvent, 0.f);
	const TCHAR* Label = TEXT("초당");
	int64 Scaled = PerSecond;
	if (Interval > 60.f)     { Label = TEXT("시간당"); Scaled = PerSecond * 3600; }
	else if (Interval > 1.f) { Label = TEXT("분당");   Scaled = PerSecond * 60; }

	return FString::Printf(TEXT("%s +%s"), Label, *AbbreviateNumber(Scaled).ToString());
}

void UGlobalUtilFunctions::ApplyWidgetIcon(UImage* Icon, const TSoftObjectPtr<UTexture2D>& Texture,
	FVector2D IconSize, FMargin IconPadding, bool bCollapseWhenEmpty)
{
	if (!Icon)
	{
		return;
	}

	UTexture2D* Tex = Texture.IsNull() ? nullptr : Texture.LoadSynchronous();
	if (!Tex)
	{
		Icon->SetVisibility(bCollapseWhenEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::Hidden);
		return;
	}

	// ImageSize 가 (0,0) 이면 WBP 저작 크기를 그대로 둔다 (CGIconBrush 규약)
	CGIconBrush::ApplyResource(Icon, Tex, IconSize);
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);

	UPanelSlot* IconSlot = Icon->Slot;
	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(IconSlot))   { HSlot->SetPadding(IconPadding); }
	else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(IconSlot))  { VSlot->SetPadding(IconPadding); }
	else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(IconSlot))          { OSlot->SetPadding(IconPadding); }
	else if (UBorderSlot* BSlot = Cast<UBorderSlot>(IconSlot))            { BSlot->SetPadding(IconPadding); }
}

UTexture2D* UGlobalUtilFunctions::GetDisciplineIcon(int32 SlotIdx)
{
	// 인덱스 = EProductionDiscipline 순서(Plan/Dev/Graphics/Sound/Server/QA).
	// 파일명만 Graphics 가 아니라 Art 라 3번째만 이름이 어긋나 보이는 게 정상이다.
	static const TCHAR* DiscIconPaths[6] = {
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Plan.T_UIIcon_Disc_Plan"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Dev.T_UIIcon_Disc_Dev"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Art.T_UIIcon_Disc_Art"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Sound.T_UIIcon_Disc_Sound"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Server.T_UIIcon_Disc_Server"),
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_QA.T_UIIcon_Disc_QA"),
	};
	if (SlotIdx < 0 || SlotIdx >= UE_ARRAY_COUNT(DiscIconPaths))
	{
		return nullptr;
	}
	return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(DiscIconPaths[SlotIdx])).LoadSynchronous();
}

FLinearColor UGlobalUtilFunctions::GetStepColor(int32 Step)
{
	// 6직능 스트립 위치색 (orb·플로팅 텍스트·바 라벨 단일 출처). 1파랑/2초록/3주황/4보라/5청록/6로즈.
	static const FLinearColor StepColors[] = {
		FLinearColor(0.3f, 0.6f, 1.0f, 1.0f),
		FLinearColor(0.3f, 0.9f, 0.4f, 1.0f),
		FLinearColor(1.0f, 0.7f, 0.2f, 1.0f),
		FLinearColor(0.75f, 0.45f, 1.0f, 1.0f),
		FLinearColor(0.2f, 0.85f, 0.85f, 1.0f),
		FLinearColor(1.0f, 0.45f, 0.6f, 1.0f),
	};
	const int32 Idx = FMath::Clamp(Step - 1, 0, 5);
	return StepColors[Idx];
}

void UGlobalUtilFunctions::InitProgressHead(UImage* Head)
{
	if (Head)
	{
		Head->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGlobalUtilFunctions::UpdateProgressHead(UProgressBar* Bar, UImage* Head, float Percent01, bool bSyncTint, float HeadSpill)
{
	if (!Bar || !Head)
	{
		return;
	}

	const float Percent = FMath::Clamp(Percent01, 0.0f, 1.0f);

	if (Percent <= KINDA_SMALL_NUMBER)
	{
		Head->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 앵커는 정규화 좌표라 바 픽셀 폭이 필요 없다 — 레이아웃 전(프레임 1)에도 정확.
	// 지오메트리를 읽는 방식은 첫 호출에서 폭 0 이라 헤드가 안 뜨는 버그가 있었다.
	UCanvasPanelSlot* HeadSlot = Cast<UCanvasPanelSlot>(Head->Slot);
	if (!HeadSlot)
	{
		return;
	}
	HeadSlot->SetAnchors(FAnchors(Percent, 0.5f, Percent, 0.5f));

	// 크기는 바 두께에 맞춰 자동 산출 — 패널마다 손으로 숫자를 맞추면 새 바가 생길 때마다 반복된다.
	float BarHeight = Bar->GetCachedGeometry().GetLocalSize().Y;
	if (BarHeight <= KINDA_SMALL_NUMBER)
	{
		// 레이아웃 전(첫 표시) 폴백 — 배선 구조상 바의 조부모 SizeBox HeightOverride 가 두께의 SOT.
		// 접근자가 엔진 버전에 따라 없어(직접 멤버는 deprecate 경고 = 빌드 실패) 리플렉션으로 읽는다.
		if (const UPanelWidget* HeadCanvas = Bar->GetParent())
		{
			if (const USizeBox* Box = Cast<USizeBox>(HeadCanvas->GetParent()))
			{
				static const FFloatProperty* HProp = FindFProperty<FFloatProperty>(USizeBox::StaticClass(), TEXT("HeightOverride"));
				static const FBoolProperty* HFlag = FindFProperty<FBoolProperty>(USizeBox::StaticClass(), TEXT("bOverride_HeightOverride"));
				if (HProp && (!HFlag || HFlag->GetPropertyValue_InContainer(Box)))
				{
					BarHeight = HProp->GetPropertyValue_InContainer(Box);
				}
			}
		}
	}
	if (BarHeight > KINDA_SMALL_NUMBER)
	{
		const FVector2D Authored = Head->GetBrush().GetImageSize();
		const float Aspect = (Authored.Y > KINDA_SMALL_NUMBER) ? (Authored.X / Authored.Y) : 1.0f;
		const float NewHeight = BarHeight + 2.0f * HeadSpill;
		HeadSlot->SetSize(FVector2D(NewHeight * Aspect, NewHeight));
	}

	Head->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bSyncTint)
	{
		// 골드/그린/블루 바를 헤드 텍스처 한 장으로 커버 — 색은 바가 단일 출처
		FLinearColor Tint = Bar->GetFillColorAndOpacity();
		Tint.A = 1.0f;
		Head->SetColorAndOpacity(Tint);
	}
}

FVector2D UGlobalUtilFunctions::GetPointerAbsolutePosition()
{
	if (!FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	if (TSharedPtr<FSlateUser> User = Slate.GetUser(0))
	{
		// 터치 좌표는 눌린 동안만 등록되고 release 시 제거(미등록 = ZeroVector) —
		// IsTouchPointerActive 는 SLATE_SCOPE(모듈 외부 protected)라 좌표 유무로 동일 판정
		const FVector2D TouchPos = User->GetPointerPosition(0);
		if (!TouchPos.IsNearlyZero())
		{
			return TouchPos;
		}
	}

	// 데스크톱 마우스 경로 (모바일에선 커서가 없어 (0,0) — 위 터치 분기가 진실)
	return Slate.GetCursorPos();
}
