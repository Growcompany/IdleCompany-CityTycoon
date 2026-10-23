#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Panel/WorldMapBottomWidget.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "Enum/CountryMapState.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRingColorTest,
	"CGR.WorldMap.MedallionRingColor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRingColorTest::RunTest(const FString& Parameters)
{
	// 진출국 링 = 호출부가 DT(FCompanyInfoTable::AccentColor)에서 읽어 넘긴 산업색을 그대로 통과시킨다.
	// ⚠ 기대값에 **카탈로그 색을 쓰지 말 것.** 여기 반도체 #6366F1 을 쓰면
	//   `if (Owned) return FromSRGBColor(0x6366F1);` 로 되돌려도 이 단언이 통과한다 ―
	//   즉 "산업 hex 를 코드에서 뺐다" 는 이 라운드의 핵심 성질이 전혀 고정되지 않는다.
	//   임의색 **두 개**로 봐야 "특정 색을 돌려준다" 가 아니라 "넘긴 것을 통과시킨다" 가 고정된다.
	const FColor AccentA(0x12, 0x34, 0x56);
	const FColor AccentB(0xAB, 0xCD, 0xEF);
	TestEqual(TEXT("진출국은 넘겨받은 색 그대로 (A)"),
		UCountryNameWidget::ResolveRingColor(ECountryMapState::Owned, FLinearColor::FromSRGBColor(AccentA)).ToFColor(true),
		FColor(0x12, 0x34, 0x56, 0xFF));
	TestEqual(TEXT("진출국은 넘겨받은 색 그대로 (B ― 상수 반환이면 여기서 갈린다)"),
		UCountryNameWidget::ResolveRingColor(ECountryMapState::Owned, FLinearColor::FromSRGBColor(AccentB)).ToFColor(true),
		FColor(0xAB, 0xCD, 0xEF, 0xFF));

	const FLinearColor Accent = FLinearColor::FromSRGBColor(AccentA);

	// 진출 가능·잠김은 산업과 무관한 중립색 ― 특산을 미리 알려주지 않는다.
	TestEqual(TEXT("진출 가능 중립"),
		UCountryNameWidget::ResolveRingColor(ECountryMapState::Available, Accent).ToFColor(true),
		FColor(0x7F, 0x8C, 0xA3, 0xFF));
	TestEqual(TEXT("잠김 슬레이트"),
		UCountryNameWidget::ResolveRingColor(ECountryMapState::Locked, Accent).ToFColor(true),
		FColor(0x3B, 0x43, 0x54, 0xFF));

	// 특산 산업이 없는 채광 전담국 = 자원 브라스
	TestEqual(TEXT("채광국 브라스"),
		UCountryNameWidget::MiningRingColor().ToFColor(true), FColor(0xC9, 0xA5, 0x67, 0xFF));

	// ★ 값 단언만으로는 못 잡는 결함 ― 채광 5개국(브라질·사우디·호주·캐나다·남아공)은 산업색이
	//   브라스 하나뿐이라, 상태별 색이 겹치면 "진출했는지 아직 아닌지"를 링으로 구별할 수 없게 된다.
	//   세 상태가 서로 다른 색을 낸다는 것 자체를 못 박는다.
	const FLinearColor MiningAccent = UCountryNameWidget::MiningRingColor();
	const FColor OwnedRing     = UCountryNameWidget::ResolveRingColor(ECountryMapState::Owned, MiningAccent).ToFColor(true);
	const FColor AvailableRing = UCountryNameWidget::ResolveRingColor(ECountryMapState::Available, MiningAccent).ToFColor(true);
	const FColor LockedRing    = UCountryNameWidget::ResolveRingColor(ECountryMapState::Locked, MiningAccent).ToFColor(true);
	TestTrue(TEXT("채광국도 진출↔진출가능이 링으로 구별된다"), OwnedRing != AvailableRing);
	TestTrue(TEXT("진출가능↔잠김이 링으로 구별된다"), AvailableRing != LockedRing);
	TestTrue(TEXT("진출↔잠김이 링으로 구별된다"), OwnedRing != LockedRing);
	return true;
}

// Review Focus #2 — RegionMask 로드 실패 시 잠긴 국가가 밝게 보이면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRegionShadeFallbackTest,
	"CGR.WorldMap.RegionShadeFallback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRegionShadeFallbackTest::RunTest(const FString& Parameters)
{
	// 정상 경로
	TestEqual(TEXT("진출국은 안 덮는다"),
		UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Owned, true), 0.0f, 1e-4f);
	TestEqual(TEXT("진출 가능은 0.35"),
		UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Available, true), 0.35f, 1e-4f);
	TestEqual(TEXT("잠김은 0.72"),
		UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Locked, true), 0.72f, 1e-4f);

	// ★ 마스크가 없으면(쿠킹 누락 등) 덮개를 못 그린다. 그 상태로 두면 잠긴 국가가
	//   진출한 것처럼 밝게 보여 게임 상태를 거짓으로 말한다. 0 을 돌려주면 안 된다.
	TestEqual(TEXT("마스크 null 이어도 잠김은 덮는다"),
		UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Locked, false), 0.72f, 1e-4f);
	TestEqual(TEXT("마스크 null 이어도 미진출은 덮는다"),
		UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Available, false), 0.35f, 1e-4f);
	TestEqual(TEXT("마스크 null 이어도 진출국은 안 덮는다"),
		UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Owned, false), 0.0f, 1e-4f);

	// ★ 링 색과 같은 이유 ― 세 상태가 같은 알파로 수렴하면 판 위에서 상태를 구별할 수 없다.
	const float OwnedShade     = UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Owned, true);
	const float AvailableShade = UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Available, true);
	const float LockedShade    = UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState::Locked, true);
	TestFalse(TEXT("진출↔진출가능 덮개가 구별된다"), FMath::IsNearlyEqual(OwnedShade, AvailableShade, 1e-4f));
	TestFalse(TEXT("진출가능↔잠김 덮개가 구별된다"), FMath::IsNearlyEqual(AvailableShade, LockedShade, 1e-4f));
	TestFalse(TEXT("진출↔잠김 덮개가 구별된다"), FMath::IsNearlyEqual(OwnedShade, LockedShade, 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapCanvasPlacementTest,
	"CGR.WorldMap.CanvasPlacement",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapCanvasPlacementTest::RunTest(const FString& Parameters)
{
	const FVector2D Board(1840.0, 748.0);   // 2560 캔버스에서의 판 실측 크기
	const FVector2D Mid = UWorldMapPanelWidget::AnchorToCanvasPosition(FVector2D(0.5, 0.5), Board);
	TestEqual(TEXT("center X"), Mid.X, 920.0, 1e-6);
	TestEqual(TEXT("center Y"), Mid.Y, 374.0, 1e-6);

	const FVector2D Korea = UWorldMapPanelWidget::AnchorToCanvasPosition(FVector2D(0.886667, 0.286885), Board);
	TestTrue(TEXT("한국은 판 안"), Korea.X > 0 && Korea.X < Board.X && Korea.Y > 0 && Korea.Y < Board.Y);
	return true;
}

// Review Focus #4 — 패널을 연 채 시총이 올라 해금되면 그 메달리온만 즉시 갱신돼야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapUnlockRefreshTest,
	"CGR.WorldMap.UnlockRefresh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapUnlockRefreshTest::RunTest(const FString& Parameters)
{
	// 해금 판정은 순수 규칙이므로 위젯 없이 검증한다.
	TestFalse(TEXT("시총 1,200 이면 독일(1,500) 잠김"),
		UWorldMapPanelWidget::IsCountryUnlocked(1200, 1500));
	TestTrue(TEXT("시총 1,500 이면 독일 해금"),
		UWorldMapPanelWidget::IsCountryUnlocked(1500, 1500));
	TestTrue(TEXT("해금값 0 은 항상 열림"),
		UWorldMapPanelWidget::IsCountryUnlocked(0, 0));
	return true;
}

// ★ Step 0 — 상태를 만드는 곳이 하나여야 링 색·덮개 알파·자물쇠가 갈리지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapCountryStateTest,
	"CGR.WorldMap.CountryState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapCountryStateTest::RunTest(const FString& Parameters)
{
	const ECountryOwnershipRule Facility = ECountryOwnershipRule::Facility;

	// 시설 1개 이상이면 시총과 무관하게 진출 ― 해금값을 나중에 올려도 이미 지은 나라가 잠기지 않는다.
	TestEqual(TEXT("시설 1개면 진출"),
		UWorldMapPanelWidget::ResolveCountryState(Facility, 1, 0, 1500), ECountryMapState::Owned);
	TestEqual(TEXT("시설 3개면 진출"),
		UWorldMapPanelWidget::ResolveCountryState(Facility, 3, 99999, 1500), ECountryMapState::Owned);

	TestEqual(TEXT("시설 0 + 시총 충족 = 진출 가능"),
		UWorldMapPanelWidget::ResolveCountryState(Facility, 0, 1500, 1500), ECountryMapState::Available);
	TestEqual(TEXT("시설 0 + 시총 부족 = 잠김"),
		UWorldMapPanelWidget::ResolveCountryState(Facility, 0, 1499, 1500), ECountryMapState::Locked);
	TestEqual(TEXT("해금값 0 은 시설 없이도 진출 가능"),
		UWorldMapPanelWidget::ResolveCountryState(Facility, 0, 0, 0), ECountryMapState::Available);

	// ★ 이 테스트의 핵심 ― 링·덮개·자물쇠가 같은 한 값에서 나오는지. 세 표현이 같은 입력에 대해
	//   서로 다른 상태를 말하면(예: 링은 진출, 덮개는 진출 가능) 아래 세 단언 중 하나가 깨진다.
	struct FCase { ECountryOwnershipRule Rule; int32 Facilities; int64 Cap; int64 Need; };
	const FCase Cases[] = {
		{ ECountryOwnershipRule::Facility, 2, 0,    1500 },
		{ ECountryOwnershipRule::Facility, 0, 1500, 1500 },
		{ ECountryOwnershipRule::Facility, 0, 10,   1500 },
		{ ECountryOwnershipRule::TradeHub, 0, 1300, 1300 },
		{ ECountryOwnershipRule::TradeHub, 0, 10,   1300 },
	};
	for (const FCase& C : Cases)
	{
		const ECountryMapState MapState = UWorldMapPanelWidget::ResolveCountryState(C.Rule, C.Facilities, C.Cap, C.Need);
		const bool bLockedByRing  = (UCountryNameWidget::ResolveRingColor(MapState, UCountryNameWidget::MiningRingColor()).ToFColor(true)
			== FColor(0x3B, 0x43, 0x54, 0xFF));
		const bool bLockedByShade = FMath::IsNearlyEqual(
			UCountryNameWidget::ResolveRegionShadeAlpha(MapState, true), 0.72f, 1e-4f);
		const bool bLockedByState = (MapState == ECountryMapState::Locked);
		TestTrue(TEXT("링과 상태가 같은 잠금을 말한다"), bLockedByRing == bLockedByState);
		TestTrue(TEXT("덮개와 상태가 같은 잠금을 말한다"), bLockedByShade == bLockedByState);
	}
	return true;
}

// ★ 무역 허브는 시설을 못 지어 시설 수로는 영원히 진출에 도달하지 못한다. 그 경로를 못 박는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapOwnershipRuleTest,
	"CGR.WorldMap.OwnershipRule",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapOwnershipRuleTest::RunTest(const FString& Parameters)
{
	// 규칙은 "지을 수 있는가" 로만 갈린다 ― DT 의 지원 플래그가 유일한 입력이다.
	FCountryInfoTable FactoryCountry;
	FactoryCountry.bSupportsFactory = true;
	FactoryCountry.bSupportsMine = false;
	TestEqual(TEXT("공장을 지을 수 있으면 시설 규칙"),
		UWorldMapPanelWidget::ResolveOwnershipRule(FactoryCountry), ECountryOwnershipRule::Facility);

	FCountryInfoTable MineCountry;
	MineCountry.bSupportsFactory = false;
	MineCountry.bSupportsMine = true;
	TestEqual(TEXT("채광만 가능해도 시설 규칙"),
		UWorldMapPanelWidget::ResolveOwnershipRule(MineCountry), ECountryOwnershipRule::Facility);

	// ★ 미국형 ― 시설을 둘 다 지을 수 있으면서 bIsTradeHub 도 켜져 있다. 이 한 줄이 없으면
	//   판정 키를 bIsTradeHub 로 되돌려도 이 테스트가 전부 초록이다(오늘 우리가 겪은 것과 같은 모양).
	FCountryInfoTable HubWithFacilities;
	HubWithFacilities.bSupportsFactory = true;
	HubWithFacilities.bSupportsMine = true;
	HubWithFacilities.bIsTradeHub = true;
	TestEqual(TEXT("무역 허브라도 시설을 지을 수 있으면 시설 규칙"),
		UWorldMapPanelWidget::ResolveOwnershipRule(HubWithFacilities), ECountryOwnershipRule::Facility);

	// 싱가포르 ― 둘 다 False
	FCountryInfoTable HubCountry;
	HubCountry.bSupportsFactory = false;
	HubCountry.bSupportsMine = false;
	HubCountry.bIsTradeHub = true;
	TestEqual(TEXT("아무것도 못 지으면 허브 규칙"),
		UWorldMapPanelWidget::ResolveOwnershipRule(HubCountry), ECountryOwnershipRule::TradeHub);

	// ★ 이 라운드의 핵심 단언 ― 시설 0 인 허브가 해금되면 진출이다.
	TestEqual(TEXT("시설 0 인 허브도 해금되면 진출"),
		UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule::TradeHub, 0, 1300, 1300),
		ECountryMapState::Owned);
	TestEqual(TEXT("허브는 미해금이면 잠김"),
		UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule::TradeHub, 0, 1299, 1300),
		ECountryMapState::Locked);
	// 허브에는 "진출 가능" 이라는 중간 상태가 존재할 수 없다 ― 시설을 지을 방법이 없으니 다음 할 일도 없다.
	TestTrue(TEXT("허브는 어떤 시총에서도 진출 가능이 되지 않는다"),
		UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule::TradeHub, 0, 0, 1300) != ECountryMapState::Available
		&& UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule::TradeHub, 0, 99999, 1300) != ECountryMapState::Available);

	// 같은 입력이라도 규칙이 다르면 갈린다 ― 이 갈림이 사라지면 싱가포르가 다시 영구 미진출로 돌아간다.
	TestTrue(TEXT("시설 0·해금 상태에서 시설 규칙과 허브 규칙이 다르다"),
		UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule::Facility, 0, 1300, 1300)
		!= UWorldMapPanelWidget::ResolveCountryState(ECountryOwnershipRule::TradeHub, 0, 1300, 1300));
	return true;
}

// ★ 포커스가 "값은 맞는데 화면엔 안 나온" 결함을 잡는다. 상세를 push 하면 패널이 deactivate 되어
//   틱이 멈추므로, 그 직전 호출은 트윈을 남기지 않고 반환 즉시 목표 변형이어야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapFocusImmediateTest,
	"CGR.WorldMap.FocusImmediate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapFocusImmediateTest::RunTest(const FString& Parameters)
{
	const FVector2D Board = UWorldMapPanelWidget::DesignBoardSize();
	const FSlateRect FullView(FVector2D::ZeroVector, Board);
	const FVector2D Center = Board * 0.5;

	// 판 중앙을 포커스하면 이동량이 0 이고 배율만 오른다 ― 수식이 뒤집히면 여기서 갈린다.
	FVector2D CenterTranslation = FVector2D::ZeroVector;
	float CenterScale = 0.0f;
	UWorldMapPanelWidget::ComputeFocusTransform(FVector2D(0.5, 0.5), Board, FullView,
		FSlateRect(Center, Center), CenterTranslation, CenterScale);
	TestTrue(TEXT("중앙 포커스는 이동 0"), CenterTranslation.IsNearlyZero(0.01));
	TestTrue(TEXT("포커스는 확대다"), CenterScale > 1.0f);

	// 오른쪽 위(한국)를 포커스하면 판을 왼쪽·아래로 민다.
	const FVector2D KoreaAnchor(0.886667, 0.286885);
	FVector2D KoreaTranslation = FVector2D::ZeroVector;
	float KoreaScale = 0.0f;
	const FVector2D KoreaPixels = KoreaAnchor * Board;
	UWorldMapPanelWidget::ComputeFocusTransform(KoreaAnchor, Board, FullView,
		FSlateRect(KoreaPixels, KoreaPixels), KoreaTranslation, KoreaScale);
	TestTrue(TEXT("판 오른쪽을 보려면 판이 왼쪽으로 간다"), KoreaTranslation.X < 0.0);
	TestTrue(TEXT("판 위쪽을 보려면 판이 아래로 간다"), KoreaTranslation.Y > 0.0);

	// ★ 핵심 ― 즉시 경로는 반환 직후 적용이 끝나 있어야 한다. 트윈이 남으면 그 프레임에 화면은 원위치다.
	UWorldMapPanelWidget* Panel = NewObject<UWorldMapPanelWidget>(GetTransientPackage());
	if (!Panel)
	{
		AddError(TEXT("패널 인스턴스를 만들지 못했습니다."));
		return false;
	}

	TestFalse(TEXT("시작 상태에는 트윈이 없다"), Panel->IsFocusAnimating());

	Panel->FocusOnAnchorImmediate(KoreaAnchor);
	TestFalse(TEXT("즉시 포커스는 트윈을 남기지 않는다"), Panel->IsFocusAnimating());
	TestTrue(TEXT("반환 직후 이동량이 목표값"),
		Panel->GetAppliedFocusTranslation().Equals(KoreaTranslation, 0.01));
	TestEqual(TEXT("반환 직후 배율이 목표값"), Panel->GetAppliedFocusScale(), KoreaScale, 1e-3f);
	TestTrue(TEXT("포커스가 원위치가 아니다 ― 적용됐는지 자체를 본다"),
		!Panel->GetAppliedFocusTranslation().IsNearlyZero(0.01));

	// 원위치 복귀는 틱이 도는 자리라 트윈으로 둔다. 애니 없이 부르면 즉시 되돌아온다.
	Panel->ResetFocus(/*bAnimate*/ false);
	TestFalse(TEXT("즉시 복귀도 트윈을 남기지 않는다"), Panel->IsFocusAnimating());
	TestTrue(TEXT("복귀하면 이동량 0"), Panel->GetAppliedFocusTranslation().IsNearlyZero(0.01));
	TestEqual(TEXT("복귀하면 배율 1"), Panel->GetAppliedFocusScale(), 1.0f, 1e-3f);
	return true;
}

// ★ 대표 산업이 없으면 지원 목록 첫 원소로 떨어지는데, DT 가 셋 다 Electronics 로 시작해 링이 수렴했다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRepresentativeIndustryTest,
	"CGR.WorldMap.RepresentativeIndustry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRepresentativeIndustryTest::RunTest(const FString& Parameters)
{
	// 지정되면 그것 ― 지원 목록 첫 원소(Electronics)를 이긴다.
	FCountryInfoTable Designated;
	Designated.SupportedIndustries = { ECompanyType::Electronics, ECompanyType::Automobile, ECompanyType::Semiconductor };
	Designated.RepresentativeIndustry = ECompanyType::Semiconductor;
	TestEqual(TEXT("대표 산업이 지정되면 그것을 쓴다"),
		UWorldMapPanelWidget::ResolveSpecialtyIndustry(Designated), ECompanyType::Semiconductor);

	// 비어 있으면 첫 유효 원소로 폴백(호출부가 경고를 낸다)
	FCountryInfoTable Fallback;
	Fallback.SupportedIndustries = { ECompanyType::Electronics, ECompanyType::Automobile };
	TestEqual(TEXT("대표 산업이 비면 첫 유효 원소로 폴백"),
		UWorldMapPanelWidget::ResolveSpecialtyIndustry(Fallback), ECompanyType::Electronics);

	// None 이 섞여 있어도 건너뛴다
	FCountryInfoTable Padded;
	Padded.SupportedIndustries = { ECompanyType::None, ECompanyType::Automobile };
	TestEqual(TEXT("None 원소는 건너뛴다"),
		UWorldMapPanelWidget::ResolveSpecialtyIndustry(Padded), ECompanyType::Automobile);

	// 읽어올 산업이 아예 없는 국가
	FCountryInfoTable Empty;
	TestEqual(TEXT("산업이 없으면 None"),
		UWorldMapPanelWidget::ResolveSpecialtyIndustry(Empty), ECompanyType::None);

	// 산업이 없는 두 종류는 서로 다른 색이어야 한다 ― 허브와 채광국이 같은 브라스면 판에서 구별이 안 된다.
	FCountryInfoTable Hub;
	Hub.bSupportsFactory = false;
	Hub.bSupportsMine = false;
	FCountryInfoTable Miner;
	Miner.bSupportsFactory = false;
	Miner.bSupportsMine = true;

	TestEqual(TEXT("허브 링은 청록"),
		UWorldMapPanelWidget::ResolveNoIndustryRingColor(Hub).ToFColor(true), FColor(0x14, 0xB8, 0xA6, 0xFF));
	TestEqual(TEXT("채광 전담국 링은 브라스"),
		UWorldMapPanelWidget::ResolveNoIndustryRingColor(Miner).ToFColor(true), FColor(0xC9, 0xA5, 0x67, 0xFF));
	TestEqual(TEXT("허브 색 상수"),
		UCountryNameWidget::TradeHubRingColor().ToFColor(true), FColor(0x14, 0xB8, 0xA6, 0xFF));

	// 진출 링으로 통과될 때 서로, 그리고 상태 중립색들과 구별되는지 ― 값이 아니라 구별 가능성을 못 박는다.
	const FColor HubRing = UCountryNameWidget::ResolveRingColor(
		ECountryMapState::Owned, UCountryNameWidget::TradeHubRingColor()).ToFColor(true);
	const FColor MineRing = UCountryNameWidget::ResolveRingColor(
		ECountryMapState::Owned, UCountryNameWidget::MiningRingColor()).ToFColor(true);
	TestTrue(TEXT("허브와 채광국 링이 구별된다"), HubRing != MineRing);
	TestTrue(TEXT("허브 링이 진출 가능 중립색과 구별된다"), HubRing != FColor(0x7F, 0x8C, 0xA3, 0xFF));
	TestTrue(TEXT("허브 링이 잠김 슬레이트와 구별된다"), HubRing != FColor(0x3B, 0x43, 0x54, 0xFF));
	return true;
}

// ★ Step 0b — accent 조달 실패는 3상태 중 2상태에서 조용히 정상으로 보인다. 그 입구를 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapRingAccentSourcingTest,
	"CGR.WorldMap.RingAccentSourcing",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapRingAccentSourcingTest::RunTest(const FString& Parameters)
{
	// 흰색 = FCompanyInfoTable::AccentColor 의 "미지정" 기본값. 그대로 넘기면 진출국 링만 흰색이 되는데
	// 진출 가능·잠김은 accent 를 무시하므로 멀쩡해 보이고 로그도 에러도 없다.
	TestFalse(TEXT("흰색 accent 는 조달 실패"), UWorldMapPanelWidget::IsUsableAccent(FLinearColor::White));
	TestFalse(TEXT("투명 accent 는 조달 실패"),
		UWorldMapPanelWidget::IsUsableAccent(FLinearColor(0.4f, 0.6f, 0.9f, 0.0f)));
	TestFalse(TEXT("0 초기화 accent 는 조달 실패"),
		UWorldMapPanelWidget::IsUsableAccent(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("정상 산업색은 통과"),
		UWorldMapPanelWidget::IsUsableAccent(FLinearColor::FromSRGBColor(FColor(0x63, 0x66, 0xF1))));
	TestTrue(TEXT("폴백(채광 브라스)도 통과 ― 폴백이 다시 걸러지면 무한히 떨어진다"),
		UWorldMapPanelWidget::IsUsableAccent(UCountryNameWidget::MiningRingColor()));
	TestTrue(TEXT("폴백(허브 청록)도 통과"),
		UWorldMapPanelWidget::IsUsableAccent(UCountryNameWidget::TradeHubRingColor()));

	// 세 상태 어디에서도 링이 흰색으로 나오지 않는다. 진출은 넘긴 값을 통과시키므로
	// 위 게이트를 통과한 accent 만 들어오면 흰색이 될 수 없고, 나머지 둘은 중립색 상수다.
	const FLinearColor Fallback = UCountryNameWidget::MiningRingColor();
	const ECountryMapState AllStates[] = { ECountryMapState::Owned, ECountryMapState::Available, ECountryMapState::Locked };
	for (const ECountryMapState MapState : AllStates)
	{
		const FColor Ring = UCountryNameWidget::ResolveRingColor(MapState, Fallback).ToFColor(true);
		TestTrue(TEXT("링 색이 흰색이 아니다"), Ring != FColor(0xFF, 0xFF, 0xFF, 0xFF));
		TestTrue(TEXT("링 색이 투명이 아니다"), Ring.A > 0);
	}
	return true;
}

// ★ Step 4c — IsAnchorSet 이 순수 함수로만 검증돼 있었다. 배치 루프가 실제로 그 결과를 쓰는지 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapAnchorFilterTest,
	"CGR.WorldMap.AnchorFilter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapAnchorFilterTest::RunTest(const FString& Parameters)
{
	TArray<FCountryInfoTable> Rows;

	FCountryInfoTable Korea;
	Korea.CountryType = ECountryType::Korea;
	Korea.DisplayName = FText::FromString(TEXT("한국"));
	Korea.MapAnchor = FVector2D(0.886667, 0.286885);
	Rows.Add(Korea);

	// DT 에 안 채운 행. 배치하면 판 좌상단에 겹쳐 쌓인다.
	FCountryInfoTable Unset;
	Unset.CountryType = ECountryType::Japan;
	Unset.DisplayName = FText::FromString(TEXT("일본"));
	Unset.MapAnchor = FVector2D::ZeroVector;
	Rows.Add(Unset);

	FCountryInfoTable Usa;
	Usa.CountryType = ECountryType::USA;
	Usa.DisplayName = FText::FromString(TEXT("미국"));
	Usa.MapAnchor = FVector2D(0.14, 0.2786);
	Rows.Add(Usa);

	// CountryType 이 None 인 행도 배치 대상이 아니다.
	FCountryInfoTable NoType;
	NoType.CountryType = ECountryType::None;
	NoType.MapAnchor = FVector2D(0.5, 0.5);
	Rows.Add(NoType);

	const TArray<FCountryInfoTable> Placeable = UWorldMapPanelWidget::FilterPlaceableRows(Rows);
	TestEqual(TEXT("미기입 앵커 1개와 None 1개가 빠져 2개만 배치된다"), Placeable.Num(), 2);

	bool bContainsUnset = false;
	for (const FCountryInfoTable& Row : Placeable)
	{
		if (Row.CountryType == ECountryType::Japan) { bContainsUnset = true; }
	}
	TestFalse(TEXT("(0,0) 행은 배치 목록에 없다"), bContainsUnset);
	return true;
}

// ★ 이 패널에는 닫기 버튼 BindWidget 이 없다 ― 닫기 경로는 백 액션과 도크 홈 버튼 둘뿐이라
//   둘이 죽으면 열고 못 닫는다. 2D 전환 직후 실제로 둘 다 죽어 있었다(홈 버튼이 이제는 없는
//   ALegacyWorldMapPlayerController 를 캐스팅했고, 패널은 백 핸들러가 아니었다).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapClosePathTest,
	"CGR.WorldMap.ClosePath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapClosePathTest::RunTest(const FString& Parameters)
{
	UWorldMapPanelWidget* Panel = NewObject<UWorldMapPanelWidget>(GetTransientPackage());
	if (!Panel)
	{
		AddError(TEXT("패널 인스턴스를 만들지 못했습니다."));
		return false;
	}

	TestTrue(TEXT("패널이 백 액션을 받는다 ― false 면 뒤로가기 키가 이 패널에 도달조차 안 한다"),
		Panel->IsBackActionHandler());
	TestTrue(TEXT("백 액션을 처리했다고 알린다 ― false 면 같은 입력이 아래 위젯까지 내려가 연쇄로 닫는다"),
		Panel->NativeOnHandleBackAction());

	// 도크는 패널의 WidgetTree 안에 들어간다. 위젯 부모 체인은 트리를 안 벗어나므로 아우터로 거슬러야 잡힌다.
	UWidgetTree* PanelTree = NewObject<UWidgetTree>(Panel);
	UWorldMapBottomWidget* Dock = NewObject<UWorldMapBottomWidget>(PanelTree);
	if (!PanelTree || !Dock)
	{
		AddError(TEXT("도크/트리 인스턴스를 만들지 못했습니다."));
		return false;
	}
	TestTrue(TEXT("도크가 자기를 품은 패널을 찾는다 ― 못 찾으면 홈 버튼이 조용히 아무 일도 안 한다"),
		Dock->FindOwningActivatable() == Panel);

	// 구 경로(스택에 직접 push 된 도크)는 패널 안이 아니다 ― 그때만 레벨 컨트롤러 분기로 가야 한다.
	UWorldMapBottomWidget* Standalone = NewObject<UWorldMapBottomWidget>(GetTransientPackage());
	TestNull(TEXT("패널 밖 도크는 소유 패널이 없다 ― 있으면 구 레벨에서 엉뚱한 위젯을 닫는다"),
		Standalone ? Standalone->FindOwningActivatable() : nullptr);
	return true;
}

// 드로어 폭과 판 비율이 달라도 선택한 원반+라벨은 보이는 영역에 남고 빈 배경이 드러나지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapFocusSafeBoundsTest,
	"CGR.WorldMap.FocusSafeBounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapFocusSafeBoundsTest::RunTest(const FString& Parameters)
{
	for (const double Width : { 2180.736, 2423.04, 2953.08 })
	{
		const FVector2D Board(Width, Width * 122.0 / 300.0);
		const FSlateRect Visible(FVector2D::ZeroVector, FVector2D(Width - 1160.0, Board.Y));
		for (const FVector2D Anchor : { FVector2D(0.1, 0.13), FVector2D(0.52, 0.12),
			FVector2D(0.95, 0.32), FVector2D(0.88, 0.87), FVector2D(0.53, 0.85) })
		{
			const FVector2D Point = Anchor * Board;
			const bool bTopLabel = Anchor.Y > 0.8;
			const FSlateRect Bounds(FVector2D(Point.X - 90.0, Point.Y - (bTopLabel ? 178.0 : 60.0)),
				FVector2D(Point.X + 90.0, Point.Y + (bTopLabel ? 60.0 : 178.0)));
			FVector2D Translation;
			float Scale = 0.0f;
			UWorldMapPanelWidget::ComputeFocusTransform(Anchor, Board, Visible, Bounds, Translation, Scale);
			const FVector2D Center = Board * 0.5;
			const FVector2D PinMin = Center + (FVector2D(Bounds.Left, Bounds.Top) - Center) * Scale + Translation;
			const FVector2D PinMax = Center + (FVector2D(Bounds.Right, Bounds.Bottom) - Center) * Scale + Translation;
			const FVector2D MapMin = Center - Center * Scale + Translation;
			const FVector2D MapMax = Center + Center * Scale + Translation;
			TestTrue(TEXT("선택 원반과 라벨이 드로어 왼쪽 가시 영역 안에 남는다"),
				PinMin.X >= Visible.Left + 15.9 && PinMin.Y >= Visible.Top + 15.9
				&& PinMax.X <= Visible.Right - 15.9 && PinMax.Y <= Visible.Bottom - 15.9);
			TestTrue(TEXT("포커스한 판이 가시 영역을 덮어 빈 가장자리가 없다"),
				MapMin.X <= Visible.Left + 0.1 && MapMin.Y <= Visible.Top + 0.1
				&& MapMax.X >= Visible.Right - 0.1 && MapMax.Y >= Visible.Bottom - 0.1);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// ═════════════════════════════════════════════════════════════════════════════
// Task 9 — 렌더 검증. 위 단언들이 전부 "값" 이라면 여기부터는 **실제로 그려진 픽셀**을 본다.
//
// ⚠⚠ -nullrhi 에서는 이 검증이 거짓 통과한다. FWidgetRenderer 는 렌더타깃에 그려서 재는데
//    Null RHI 에는 렌더타깃이 없어 레이아웃도 캡처도 전부 0 이 된다. 그런데 여기 단언은 대부분
//    "교차 0" · "판 안에 있다" 형태라 **0 이 들어오면 초록**이 된다(2026-09-25 실측: 같은 런에서
//    FWidgetRenderer 를 쓰는 CGR.LiveOps 4건이 전부 0 으로 무너졌다). 그래서 두 겹으로 막는다:
//      ① FApp::CanEverRender() 게이트 — -nullrhi · 커맨드렛이면 시작조차 안 한다
//      ② 0 자기검증 — 판 크기 · 메달리온 크기 · 불투명 픽셀 중 하나라도 0 이면 그 런은 **판정 불가**다.
//         AddError 로 시끄럽게 실패시키고 나머지 단언을 아예 돌리지 않는다(조용히 통과하면 무의미하다).
// ═════════════════════════════════════════════════════════════════════════════
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/WorldMap/TradeRouteWidget.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Containers/Ticker.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SVirtualWindow.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 형제 파일(WorldMapProjectionTests.cpp)의 익명 ns 와 재정의 충돌
namespace WorldMapTableRender
{
	static bool ClickCountryPin(UCountryNameWidget* Pin)
	{
		UFunction* Click = Pin ? Pin->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
		if (!Click || Click->NumParms != 0) { return false; }
		// 해금은 세이브를 수정하지 않고 이 임시 렌더 인스턴스에서만 연다.
		Pin->SetLockState(false);
		Pin->ProcessEvent(Click, nullptr);
		return true;
	}

	static bool ClickDetailClose(UCountryDetailWidget* Detail)
	{
		UPanelHeaderWidget* Header = Detail ? Cast<UPanelHeaderWidget>(Detail->GetWidgetFromName(TEXT("PanelHeader"))) : nullptr;
		UCloseButtonWidget* CloseWidget = Header ? Header->GetCloseButton() : nullptr;
		UButton* CloseButton = CloseWidget ? Cast<UButton>(CloseWidget->GetWidgetFromName(TEXT("CloseBtn"))) : nullptr;
		if (!CloseButton) { return false; }
		CloseButton->OnClicked.Broadcast();
		return true;
	}

	// 가상 캔버스. UIScaleRule 이 ShortestSide 라 **세로는 어떤 기기에서도 1440 고정**이고 가로만
	// 2304(16:10 갤탭 S8) ~ 3120(19.5:9) 사이에서 변한다 (UI_CREATION_PLAYBOOK §4.5).
	// 기기 해상도(2560x1600 등)를 그대로 그리면 게임에 존재하지 않는 캔버스를 재게 된다.
	static constexpr int32 CaptureWidths[] = { 2304, 2560, 3120 };
	static constexpr int32 CaptureHeight = 1440;

	// 링·항로 차분은 비율과 무관한 성질이라 기준 폭 한 곳에서만 찍는다(렌더 횟수 = 시간).
	static constexpr int32 DetailWidth = 2560;

	// Task 6 리뷰 F-2 ― 네 레이어가 같은 사각형이어야 앵커가 비례해 어긋나지 않는다
	static constexpr double CanvasMatchTolerance = 1.0;
	// 레이아웃 패스 반올림으로 1px 안팎 흔들린다
	static constexpr double EdgeTolerance = 1.0;
	// 상태 줄 한 줄 = NEXON Regular 20pt 의 ascender-descender = 31px. 두 줄이면 62 가 되므로 그 사이에 둔다
	static constexpr double MaxStatusLineHeight = 40.0;

	// 채널 하나라도 이만큼 달라지면 "그 픽셀은 바뀌었다"
	static constexpr int32 PixelDiffThreshold = 6;
	// 링 두께 4~6px x 둘레 200px+ = 최소 수백 px. 24 는 "그려지긴 했다" 의 하한이다
	static constexpr int32 MinRingDiffPixels = 24;
	static constexpr int32 MinRoutePixels = 40;
	// 폴리라인은 제 bbox 를 거의 못 채운다. 0 길이 세그먼트가 만든 NaN 사각형은 bbox 를 통째로 칠한다
	static constexpr double MaxRouteBBoxFill = 0.30;
	static constexpr double MaxRouteBoardShare = 0.08;
	// 흰 얼룩 가드. 활성 대시(불투명 #3D9BE0) 의 휘도가 140, 브라스 위 글로우가 ~190 이다
	static constexpr double MaxRouteLuma = 240.0;
	// 색상비(정규화 chroma) 허용오차. 링 브러시가 자체 명암을 갖고 있어도 비는 보존된다
	static constexpr double ChromaTolerance = 0.12;
	static constexpr double RouteChromaTolerance = 0.10;
	static constexpr double EndpointRadiusMargin = 40.0;

	// 스펙 §4.3 기대색 ― 리터럴을 여기 한 번만 둔다
	static FColor OwnedMiningRingInk()  { return FColor(0xC9, 0xA5, 0x67); }
	static FColor AvailableRingInk()    { return FColor(0x7F, 0x8C, 0xA3); }
	static FColor ActiveRouteInk()      { return FColor(0x3D, 0x9B, 0xE0); }

	// 픽스처 상태 줄 ― `BuildStatusLine` 이 **만들 수 있는 가장 긴 문자열**. 11개 전부에 같은 값을 넣는다.
	// 짧은 나라만 골라 넣으면 "통과하도록 맞춘 픽스처" 가 되어 겹침 게이트가 제 일을 못 한다.
	// 잠금 줄 `시총 {축약}` 은 축약이 5자를 못 넘으므로(UI_TYPOGRAPHY §5) 이 형태가 상한이고,
	// 시설 줄(`시설 9/9` 등)·`진출 가능`·특산명은 모두 이보다 짧다.
	static FText FixtureStatusLine()
	{
		return FText::FromString(TEXT("시총 9990만"));
	}

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldMapValidation"));
	}

	static FString CountryLabel(ECountryType Country)
	{
		const UEnum* CountryEnum = StaticEnum<ECountryType>();
		return CountryEnum != nullptr
			? CountryEnum->GetNameStringByValue(static_cast<int64>(Country))
			: FString::Printf(TEXT("%d"), static_cast<int32>(Country));
	}

	struct FLayoutBox
	{
		bool bFound = false;
		double Left = 0.0, Top = 0.0, Width = 0.0, Height = 0.0;
		double Right() const { return Left + Width; }
		double Bottom() const { return Top + Height; }
		bool HasArea() const { return bFound && Width > 0.5 && Height > 0.5; }
	};

	static double OverlapArea(const FLayoutBox& A, const FLayoutBox& B)
	{
		const double OverlapW = FMath::Min(A.Right(), B.Right()) - FMath::Max(A.Left, B.Left);
		const double OverlapH = FMath::Min(A.Bottom(), B.Bottom()) - FMath::Max(A.Top, B.Top);
		return (OverlapW > 0.0 && OverlapH > 0.0) ? OverlapW * OverlapH : 0.0;
	}

	static bool IsInside(const FLayoutBox& Frame, const FLayoutBox& Inner, double Tolerance)
	{
		return Inner.Left   >= Frame.Left   - Tolerance
			&& Inner.Top    >= Frame.Top    - Tolerance
			&& Inner.Right() <= Frame.Right()  + Tolerance
			&& Inner.Bottom() <= Frame.Bottom() + Tolerance;
	}

	static FLayoutBox UnionBox(const FLayoutBox& A, const FLayoutBox& B)
	{
		FLayoutBox Out;
		if (!A.bFound) { return B; }
		if (!B.bFound) { return A; }
		Out.bFound = true;
		Out.Left = FMath::Min(A.Left, B.Left);
		Out.Top = FMath::Min(A.Top, B.Top);
		Out.Width = FMath::Max(A.Right(), B.Right()) - Out.Left;
		Out.Height = FMath::Max(A.Bottom(), B.Bottom()) - Out.Top;
		return Out;
	}

	static FLayoutBox Inflate(const FLayoutBox& Box, double Amount)
	{
		FLayoutBox Out = Box;
		Out.Left -= Amount;
		Out.Top -= Amount;
		Out.Width += Amount * 2.0;
		Out.Height += Amount * 2.0;
		return Out;
	}

	static FLayoutBox MeasureGeometry(const UWidget* Widget, FVector2D Origin)
	{
		FLayoutBox Out;
		if (Widget == nullptr)
		{
			return Out;
		}
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FSlateRect RenderBounds = Geometry.GetRenderBoundingRect();
		const FVector2D Position = FVector2D(RenderBounds.Left, RenderBounds.Top) - Origin;
		const FVector2D Extent(RenderBounds.Right - RenderBounds.Left, RenderBounds.Bottom - RenderBounds.Top);
		Out.bFound = true;
		Out.Left = Position.X;
		Out.Top = Position.Y;
		Out.Width = Extent.X;
		Out.Height = Extent.Y;
		return Out;
	}

	static FLayoutBox MeasureNamed(UUserWidget& Host, const TCHAR* Name, FVector2D Origin)
	{
		return MeasureGeometry(Host.GetWidgetFromName(FName(Name)), Origin);
	}

	static bool IsRendered(const UWidget* Widget)
	{
		if (Widget == nullptr)
		{
			return false;
		}
		const ESlateVisibility Vis = Widget->GetVisibility();
		return Vis != ESlateVisibility::Collapsed && Vis != ESlateVisibility::Hidden;
	}

	static TSharedPtr<FJsonObject> BoxJson(const FLayoutBox& Box)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("found"), Box.bFound);
		Out->SetNumberField(TEXT("left"), Box.Left);
		Out->SetNumberField(TEXT("top"), Box.Top);
		Out->SetNumberField(TEXT("width"), Box.Width);
		Out->SetNumberField(TEXT("height"), Box.Height);
		return Out;
	}

	// ── 픽셀 ────────────────────────────────────────────────────────────────
	static const FColor* PixelAt(const FImage& Frame, int32 X, int32 Y)
	{
		if (Frame.Format != ERawImageFormat::BGRA8 || X < 0 || Y < 0 || X >= Frame.SizeX || Y >= Frame.SizeY)
		{
			return nullptr;
		}
		return reinterpret_cast<const FColor*>(Frame.RawData.GetData()) + static_cast<int64>(Y) * Frame.SizeX + X;
	}

	static double LumaOf(const FColor& Ink)
	{
		return 0.2126 * Ink.R + 0.7152 * Ink.G + 0.0722 * Ink.B;
	}

	// 색상비. 브러시가 곱셈 틴트로 어두워지거나 밝아져도 비는 보존되므로 "무슨 색인가" 를 이걸로 본다.
	static FVector ChromaOf(double R, double G, double B)
	{
		const double Sum = R + G + B;
		if (Sum <= 1.0)
		{
			return FVector(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);
		}
		return FVector(R / Sum, G / Sum, B / Sum);
	}

	static FVector ChromaOf(const FColor& Ink)
	{
		return ChromaOf(Ink.R, Ink.G, Ink.B);
	}

	static double ChromaDistance(const FVector& A, const FVector& B)
	{
		return FMath::Max3(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y), FMath::Abs(A.Z - B.Z));
	}

	static int64 CountOpaquePixels(const FImage& Frame)
	{
		if (Frame.Format != ERawImageFormat::BGRA8)
		{
			return 0;
		}
		const FColor* Pixels = reinterpret_cast<const FColor*>(Frame.RawData.GetData());
		const int64 Total = static_cast<int64>(Frame.SizeX) * Frame.SizeY;
		int64 Count = 0;
		for (int64 Index = 0; Index < Total; ++Index)
		{
			if (Pixels[Index].A > 8)
			{
				++Count;
			}
		}
		return Count;
	}

	/**
	 * 두 프레임의 차이 = 그 사이에 바뀐 것만 남은 마스크다.
	 * 링 색만 바꿔 두 장을 찍으면 마스크가 곧 **링 픽셀**이고, 항로만 켜고 끄면 마스크가 곧 **선 픽셀**이다.
	 * 지오메트리로 영역을 추정하지 않아도 되므로 "어디를 재야 하나" 를 틀릴 여지가 없다.
	 */
	struct FDiffStats
	{
		int32 Count = 0;
		double SumAR = 0.0, SumAG = 0.0, SumAB = 0.0;
		double SumBR = 0.0, SumBG = 0.0, SumBB = 0.0;
		double MaxLumaA = 0.0;
		int32 InkMatchCount = 0;
		int32 MinX = 0, MinY = 0, MaxX = -1, MaxY = -1;
		TArray<double> ProbeMinDistance;

		double BBoxArea() const
		{
			return (MaxX >= MinX && MaxY >= MinY)
				? static_cast<double>(MaxX - MinX + 1) * static_cast<double>(MaxY - MinY + 1)
				: 0.0;
		}
		FVector MeanChromaA() const { return Count > 0 ? ChromaOf(SumAR / Count, SumAG / Count, SumAB / Count) : FVector::ZeroVector; }
		FVector MeanChromaB() const { return Count > 0 ? ChromaOf(SumBR / Count, SumBG / Count, SumBB / Count) : FVector::ZeroVector; }
	};

	static FDiffStats DiffWithin(const FImage& FrameA, const FImage& FrameB, const FLayoutBox& Region,
		const FColor& ExpectedInk, double InkTolerance, const TArray<FVector2D>& Probes)
	{
		FDiffStats Out;
		Out.ProbeMinDistance.Init(TNumericLimits<double>::Max(), Probes.Num());
		Out.MinX = TNumericLimits<int32>::Max();
		Out.MinY = TNumericLimits<int32>::Max();
		Out.MaxX = TNumericLimits<int32>::Lowest();
		Out.MaxY = TNumericLimits<int32>::Lowest();

		const int32 Left = FMath::Max(0, static_cast<int32>(FMath::FloorToDouble(Region.Left)));
		const int32 Top = FMath::Max(0, static_cast<int32>(FMath::FloorToDouble(Region.Top)));
		const int32 Right = FMath::Min(FrameA.SizeX, static_cast<int32>(FMath::CeilToDouble(Region.Right())));
		const int32 Bottom = FMath::Min(FrameA.SizeY, static_cast<int32>(FMath::CeilToDouble(Region.Bottom())));
		const FVector ExpectedChroma = ChromaOf(ExpectedInk);

		for (int32 Y = Top; Y < Bottom; ++Y)
		{
			for (int32 X = Left; X < Right; ++X)
			{
				const FColor* InkA = PixelAt(FrameA, X, Y);
				const FColor* InkB = PixelAt(FrameB, X, Y);
				if (InkA == nullptr || InkB == nullptr)
				{
					continue;
				}
				const int32 Delta = FMath::Max3(
					FMath::Abs(static_cast<int32>(InkA->R) - static_cast<int32>(InkB->R)),
					FMath::Abs(static_cast<int32>(InkA->G) - static_cast<int32>(InkB->G)),
					FMath::Abs(static_cast<int32>(InkA->B) - static_cast<int32>(InkB->B)));
				if (Delta < PixelDiffThreshold)
				{
					continue;
				}

				++Out.Count;
				Out.SumAR += InkA->R; Out.SumAG += InkA->G; Out.SumAB += InkA->B;
				Out.SumBR += InkB->R; Out.SumBG += InkB->G; Out.SumBB += InkB->B;
				Out.MaxLumaA = FMath::Max(Out.MaxLumaA, LumaOf(*InkA));
				Out.MinX = FMath::Min(Out.MinX, X); Out.MaxX = FMath::Max(Out.MaxX, X);
				Out.MinY = FMath::Min(Out.MinY, Y); Out.MaxY = FMath::Max(Out.MaxY, Y);
				if (ChromaDistance(ChromaOf(*InkA), ExpectedChroma) <= InkTolerance)
				{
					++Out.InkMatchCount;
				}
				for (int32 ProbeIndex = 0; ProbeIndex < Probes.Num(); ++ProbeIndex)
				{
					const double Distance = FVector2D::Distance(Probes[ProbeIndex],
							FVector2D(static_cast<double>(X), static_cast<double>(Y)));
					Out.ProbeMinDistance[ProbeIndex] = FMath::Min(Out.ProbeMinDistance[ProbeIndex], Distance);
				}
			}
		}
		return Out;
	}

	// ── 렌더 ────────────────────────────────────────────────────────────────
	static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage,
		float DeltaTime = 1.0f, FVector2D WindowToDesktopOffset = FVector2D::ZeroVector)
	{
		FWidgetRenderer Renderer(true);
		// raw target(false) ― true target 은 bForceLinearGamma=false 로 하드웨어 sRGB 쓰기를 켜서 Slate 셰이더 감마와 겹친다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		TSharedPtr<SWidget> OldParent;
		TSharedPtr<SVirtualWindow> OffsetWindow;
		TUniquePtr<FHittestGrid> HitTestGrid;
		if (!WindowToDesktopOffset.IsZero())
		{
			OldParent = Widget->GetParentWidget();
			OffsetWindow = SNew(SVirtualWindow).Size(DrawSize);
			OffsetWindow->SetContent(Widget);
			OffsetWindow->Resize(DrawSize);
			HitTestGrid = MakeUnique<FHittestGrid>();
		}
		// 첫 패스는 레이아웃만 세운다 ― 지오메트리가 잡힌 다음 패스라야 자식이 제자리 크기를 보고한다
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			const float PassDelta = Pass == 0 ? DeltaTime : 0.f;
			if (OffsetWindow.IsValid())
			{
				const FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize, FSlateLayoutTransform(1.f));
				const FPaintArgs PaintArgs(nullptr, *HitTestGrid, WindowToDesktopOffset,
					FApp::GetCurrentTime(), PassDelta);
				Renderer.DrawWindow(PaintArgs, Target.Get(), OffsetWindow.ToSharedRef(), WindowGeometry,
					WindowGeometry.GetLayoutBoundingRect(), PassDelta);
			}
			else
			{
				Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, PassDelta);
			}
			FlushRenderingCommands();
		}
		if (OldParent.IsValid())
		{
			Widget->AssignParentWidget(OldParent);
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 는 linear 로 태그된 타겟에 이미 감마 인코딩된 바이트를 써 둔다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static bool SavePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	static bool SaveCrop(const FImage& Frame, const FLayoutBox& Region, double Margin, const FString& Path)
	{
		if (!Region.HasArea() || Frame.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		const int32 Left = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Region.Left - Margin)), 0, Frame.SizeX - 1);
		const int32 Top = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Region.Top - Margin)), 0, Frame.SizeY - 1);
		const int32 Right = FMath::Clamp(static_cast<int32>(FMath::CeilToDouble(Region.Right() + Margin)), Left + 1, Frame.SizeX);
		const int32 Bottom = FMath::Clamp(static_cast<int32>(FMath::CeilToDouble(Region.Bottom() + Margin)), Top + 1, Frame.SizeY);
		const int32 CropW = Right - Left;
		const int32 CropH = Bottom - Top;
		if (CropW <= 0 || CropH <= 0)
		{
			return false;
		}
		FImage CropImage;
		CropImage.Init(CropW, CropH, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const FColor* Source = reinterpret_cast<const FColor*>(Frame.RawData.GetData());
		FColor* Destination = reinterpret_cast<FColor*>(CropImage.RawData.GetData());
		for (int32 Y = 0; Y < CropH; ++Y)
		{
			FMemory::Memcpy(Destination + static_cast<int64>(Y) * CropW,
				Source + static_cast<int64>(Top + Y) * Frame.SizeX + Left,
				sizeof(FColor) * static_cast<SIZE_T>(CropW));
		}
		return SavePNG(CropImage, Path);
	}

	// 활성화되면 닫힐 때 NativeOnDeactivated 가 라이브 PIE 컨트롤러의 입력 모드를 Normal 로 되돌린다 ― 검증이 세션을 건드리면 안 된다
	static bool DisableAutoActivate(UUserWidget* Widget)
	{
		FBoolProperty* AutoActivateProperty = FindFProperty<FBoolProperty>(Widget->GetClass(), TEXT("bAutoActivate"));
		if (AutoActivateProperty == nullptr)
		{
			return false;
		}
		AutoActivateProperty->SetPropertyValue_InContainer(Widget, false);
		return true;
	}

	static UWorld* FindPlayWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		// PIE 우선 2패스 ― 둘 다 떠 있을 때 어느 쪽을 잡을지 확정한다
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == Wanted && Context.World() != nullptr)
				{
					return Context.World();
				}
			}
		}
		return nullptr;
	}
}

// ApplicationContextMask ― EditorContext 만 주면 -game 스탠드얼론에서 필터링돼 사람이 PIE 를 띄워야만 돈다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTableRenderTest,
	"CGR.WorldMap.TableRender",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapTableRenderTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTableRender;

	// ── 게이트 ① : 렌더할 수 없는 세션이면 판정 자체가 불가능하다 ────────────────
	if (!FApp::CanEverRender())
	{
		AddError(TEXT("[Task9] 렌더 불가 세션입니다(-nullrhi 또는 커맨드렛). 이 테스트의 단언은 대부분 ")
			TEXT("'교차 0' · '판 안' 형태라 측정값이 전부 0 이면 **조용히 전부 통과**합니다. ")
			TEXT("실제 RHI 로 띄운 에디터 또는 -game 세션에서 실행하세요."));
		return false;
	}

	UWorld* PlayWorld = FindPlayWorld();
	UGameInstance* GameInstance = PlayWorld != nullptr ? PlayWorld->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		// 메달리온은 DT_CountryInfo 를 읽어 만들어진다 ― 게임 인스턴스 없이는 판 위에 한 개도 안 선다.
		AddError(TEXT("[Task9] 플레이 월드가 없습니다 ― -game 스탠드얼론으로 띄우거나 PIE 를 시작한 뒤 실행하세요."));
		return false;
	}

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (TableManager == nullptr)
	{
		AddError(TEXT("[Task9] TableManagerSubsystem 을 못 읽었습니다."));
		return false;
	}

	const TSubclassOf<UUserWidget> PanelClass = TableManager->GetWidgetClass(EWidgetType::WorldMapPanel);
	if (!PanelClass)
	{
		AddError(TEXT("[Task9] DT_WidgetClass 에 WorldMapPanel 행이 없습니다 ― 패널을 만들 수 없습니다."));
		return false;
	}

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("fixture_status_line"), FixtureStatusLine().ToString());

	// 에디터를 갓 띄운 세션의 첫 드로우는 머티리얼·폰트 워밍업 전이라 판정에 쓰지 않는다 ― 버리는 한 판을 먼저 그린다
	{
		TStrongObjectPtr<UWorldMapPanelWidget> WarmupPanel(CreateWidget<UWorldMapPanelWidget>(GameInstance, PanelClass));
		if (WarmupPanel.IsValid() && DisableAutoActivate(WarmupPanel.Get()))
		{
			FImage Discarded;
			DrawToImage(WarmupPanel->TakeWidget(), FIntPoint(DetailWidth, CaptureHeight), Discarded);
		}
	}

	for (const int32 CaptureWidth : CaptureWidths)
	{
		const FString WidthLabel = FString::Printf(TEXT("%dx%d"), CaptureWidth, CaptureHeight);
		const FIntPoint CaptureSize(CaptureWidth, CaptureHeight);
		TSharedPtr<FJsonObject> WidthJson = MakeShared<FJsonObject>();

		// UWorldMapPanelWidget 으로 받는다 ― DT_WidgetClass 행이 엉뚱한 WBP 를 가리키면 여기서 null 로 드러난다
		TStrongObjectPtr<UWorldMapPanelWidget> Panel(CreateWidget<UWorldMapPanelWidget>(GameInstance, PanelClass));
		if (!TestTrue(FString::Printf(TEXT("%s: 월드맵 패널 인스턴스 생성"), *WidthLabel), Panel.IsValid()))
		{
			continue;
		}
		if (!TestTrue(FString::Printf(TEXT("%s: bAutoActivate 를 끄고 구성"), *WidthLabel), DisableAutoActivate(Panel.Get())))
		{
			continue;
		}

		// TakeWidget 이 NativeConstruct 를 돌리며 메달리온 11개를 세운다
		const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// ── 라이브 프레임 : 목업 대조(스펙 §10.1)용 증거 ──────────────────────
		FImage LiveFrame;
		const FString LiveName = FString::Printf(TEXT("worldmap_%d_live.png"), CaptureWidth);
		const bool bLiveOk = DrawToImage(PanelSlate, CaptureSize, LiveFrame)
			&& SavePNG(LiveFrame, Directory / LiveName);
		if (!bLiveOk)
		{
			// 캡처 저장 false 는 Null RHI 의 전형적인 증상이다 ― 여기서 멈추지 않으면 아래가 전부 0 으로 초록이 된다
			AddError(FString::Printf(TEXT("[Task9][%s] 캡처를 못 만들었습니다(저장 false). 렌더타깃이 없는 세션일 가능성이 큽니다 ― 판정 불가."), *WidthLabel));
			return false;
		}

		const FVector2D Origin = Panel->GetRootWidget() != nullptr
			? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
		const FLayoutBox BoardBox = MeasureNamed(*Panel, TEXT("MapBaseImage"), Origin);
		for (const FName RemovedName : { FName(TEXT("MapGhostLeft")), FName(TEXT("MapGhostRight")), FName(TEXT("MapDragSurface")) })
		{
			TestNull(TEXT("수평 드래그용 위젯이 실제 WBP에서 제거됐다"), Panel->GetWidgetFromName(RemovedName));
		}

		// ── 드로어 프레임 : 하위 패널이 열려도 지도가 살아 있고, 고른 국가가 드로어에 안 가리는가 ──
		// 값으로는 못 잡는다 ― "패널이 존재한다"·"메달리온이 화면 안이다" 는 드로어가 그 위를 덮고 있어도 참이다.
		{
			UCommonActivatableWidgetStack* TestDrawerStack = Cast<UCommonActivatableWidgetStack>(Panel->GetWidgetFromName(TEXT("DrawerStack")));
			if (!TestNotNull(TEXT("실제 드로어 스택이 존재한다"), TestDrawerStack)) { return false; }
			TestDrawerStack->SetTransitionDuration(0.0f);
			const TSubclassOf<UUserWidget> DetailClass = TableManager->GetWidgetClass(EWidgetType::CountryDetail);
			if (!DetailClass)
			{
				AddWarning(FString::Printf(
					TEXT("[Task9][%s] DT_WidgetClass 에 CountryDetail 행이 없어 드로어 상태를 **검증하지 못했습니다** ― 통과가 아니라 미검증입니다."),
					*WidthLabel));
			}
			else if (ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Japan)))
			{
				UCountryDetailWidget* Drawer = Cast<UCountryDetailWidget>(TestDrawerStack->GetActiveWidget());
				if (!TestNotNull(TEXT("실제 국가 클릭으로 상세가 열린다"), Drawer)) { return false; }
				TestTrue(FString::Printf(TEXT("%s: 드로어를 열면 IsDrawerOpen 이 참"), *WidthLabel), Panel->IsDrawerOpen());

				FImage DrawerFrame;
				const FString DrawerName = FString::Printf(TEXT("worldmap_%d_drawer.png"), CaptureWidth);
				TestTrue(FString::Printf(TEXT("%s: 드로어 프레임 캡처 저장 (%s)"), *WidthLabel, *DrawerName),
					DrawToImage(PanelSlate, CaptureSize, DrawerFrame) && SavePNG(DrawerFrame, Directory / DrawerName));

				// 공장만 있는 나라는 탭 줄 없이 공장 한 장으로 열린다 ― 정보 페이지가 보이면 한 장 서랍이 안 걸린 것이다.
				const UWidgetSwitcher* PageSwitcher = Cast<UWidgetSwitcher>(Drawer->GetWidgetFromName(TEXT("MainContentSwitcher")));
				const UWidget* ActivePage = PageSwitcher ? PageSwitcher->GetActiveWidget() : nullptr;
				TestEqual(FString::Printf(TEXT("%s: 일본 서랍 = 공장 한 장 (탭 %d)"), *WidthLabel, Drawer->GetCurrentTabIndex()),
					ActivePage ? ActivePage->GetName() : FString(TEXT("None")), FString(TEXT("FactoryPlate")));

				// 시설 수별 서랍 ― 탭 줄은 시설 2개 이상일 때만, 항구만 있는 나라는 판매 한 장.
				if (CaptureWidth == 2560)
				{
					struct FDrawerCase { ECountryType Country; const TCHAR* Code; const TCHAR* Page; bool bTabs; };
					const FDrawerCase DrawerCases[] = {
						{ ECountryType::USA, TEXT("US"), TEXT("PortPlate"), false },
						{ ECountryType::Singapore, TEXT("SG"), TEXT("PortPlate"), false },
						{ ECountryType::China, TEXT("CN"), TEXT("FactoryPlate"), false },
						{ ECountryType::Korea, TEXT("KR"), TEXT("FactoryPlate"), false },
						{ ECountryType::Canada, TEXT("CA"), TEXT("MinePlate"), false } };
					for (const FDrawerCase& Case : DrawerCases)
					{
						if (!TestTrue(FString::Printf(TEXT("%s 핀 클릭"), Case.Code), ClickCountryPin(Panel->GetMedallions().FindRef(Case.Country))))
						{
							continue;
						}
						UCountryDetailWidget* CaseDrawer = Cast<UCountryDetailWidget>(TestDrawerStack->GetActiveWidget());
						if (!TestNotNull(FString::Printf(TEXT("%s 서랍"), Case.Code), CaseDrawer)) { continue; }
						const UWidgetSwitcher* CaseSwitcher = Cast<UWidgetSwitcher>(CaseDrawer->GetWidgetFromName(TEXT("MainContentSwitcher")));
						const UWidget* CasePage = CaseSwitcher ? CaseSwitcher->GetActiveWidget() : nullptr;
						TestEqual(FString::Printf(TEXT("%s 서랍 첫 페이지"), Case.Code),
							CasePage ? CasePage->GetName() : FString(TEXT("None")), FString(Case.Page));
						const UWidget* CaseTabs = CaseDrawer->GetWidgetFromName(TEXT("TabBar"));
						TestEqual(FString::Printf(TEXT("%s 탭 줄 표시"), Case.Code),
							CaseTabs != nullptr && CaseTabs->GetVisibility() != ESlateVisibility::Collapsed, Case.bTabs);
						FImage CaseFrame;
						const FString CaseName = FString::Printf(TEXT("worldmap_%d_drawer_%s.png"), CaptureWidth, Case.Code);
						TestTrue(FString::Printf(TEXT("%s 서랍 캡처 저장"), Case.Code),
							DrawToImage(PanelSlate, CaptureSize, CaseFrame) && SavePNG(CaseFrame, Directory / CaseName));
					}
					// 아래 단언은 일본 서랍 상태를 전제로 한다
					ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Japan));
				}

				const FLayoutBox ViewportBox = MeasureNamed(*Panel, TEXT("MapViewport"), Origin);
				const FLayoutBox DrawerBox = MeasureNamed(*Panel, TEXT("DrawerHost"), Origin);
				const double DrawerLeft = FMath::Min(ViewportBox.Right(), DrawerBox.Left);
				UCountryNameWidget* PreviousPin = nullptr;
				FLinearColor PreviousBaseColor;
				for (const ECountryType FocusCountry : { ECountryType::Canada, ECountryType::Germany,
					ECountryType::Australia, ECountryType::SouthAfrica, ECountryType::Japan })
				{
					FImage FocusFrame;
					UCountryNameWidget* Pin = Panel->GetMedallions().FindRef(FocusCountry);
					if (!TestNotNull(TEXT("가장자리 국가 핀이 존재한다"), Pin)) { continue; }
					UImage* Ring = Cast<UImage>(Pin->GetWidgetFromName(TEXT("RingImage")));
					if (!TestNotNull(TEXT("선택 표시 링이 존재한다"), Ring)) { continue; }
					const FLinearColor BaseColor = Ring->GetColorAndOpacity();
					TestTrue(TEXT("국가 버튼 실제 클릭 경로"), ClickCountryPin(Pin));
					TestEqual(TEXT("여러 국가를 눌러도 상세 스택은 하나"), TestDrawerStack->GetNumWidgets(), 1);
					TestTrue(TEXT("국가 상세 인스턴스를 재사용한다"), TestDrawerStack->GetActiveWidget() == Drawer);
					TestEqual(TEXT("실제 상세 내용도 클릭 국가로 바뀐다"), Drawer->GetCurrentCountry(), FocusCountry);
					TestTrue(TEXT("가장자리 국가 포커스 캡처 저장"), DrawToImage(PanelSlate, CaptureSize, FocusFrame)
						&& SavePNG(FocusFrame, Directory / FString::Printf(TEXT("worldmap_%d_focus_%s.png"), CaptureWidth, ToCountryCode(FocusCountry))));
					TestEqual(TEXT("포커스 국가와 선택 국가가 일치한다"), Panel->GetSelectedCountry(), FocusCountry);
					TestTrue(TEXT("CommonButton 선택 상태가 켜진다"), Pin->GetSelected());
					UPanelWidget* DemandContainer = Cast<UPanelWidget>(Drawer->GetWidgetFromName(TEXT("MarketDemandContainer")));
					const TArray<UWidget*> DemandCards = DemandContainer ? DemandContainer->GetAllChildren() : TArray<UWidget*>();
					TestTrue(TEXT("재클릭 보존 검증에 실제 본문 카드가 있다"), !DemandCards.IsEmpty());
					const int32 TabBeforeReclick = Drawer->GetCurrentTabIndex();
					const bool bReducedBeforeReclick = FMotionGate::IsReduced();
					FMotionGate::SetReduced(false);
					Pin->SetFocusHighlight(false);
					Pin->SetFocusHighlight(true);
					Pin->UpdateFocusHighlight(0.8f);
					const FLinearColor PulseBeforeReclick = Ring->GetColorAndOpacity();
					TestFalse(TEXT("재클릭 전 지도 포커스가 정착했다"), Panel->IsFocusAnimating());
					TestTrue(TEXT("같은 국가 재클릭도 실제 버튼 경로"), ClickCountryPin(Pin));
					TestEqual(TEXT("같은 국가 재클릭도 상세 스택은 하나"), TestDrawerStack->GetNumWidgets(), 1);
					TestTrue(TEXT("동일 국가 재클릭은 본문 카드 인스턴스를 보존한다"),
						DemandContainer && DemandContainer->GetAllChildren() == DemandCards);
					TestEqual(TEXT("동일 국가 재클릭은 현재 탭을 보존한다"), Drawer->GetCurrentTabIndex(), TabBeforeReclick);
					TestTrue(TEXT("동일 국가 재클릭은 선택 강조를 유지한다"), Pin->GetSelected());
					TestTrue(TEXT("동일 국가 재클릭은 선택 효과의 위상을 보존한다"), Ring->GetColorAndOpacity().Equals(PulseBeforeReclick));
					TestFalse(TEXT("동일 국가 재클릭은 끝난 지도 포커스를 재시작하지 않는다"), Panel->IsFocusAnimating());
					FMotionGate::SetReduced(bReducedBeforeReclick);
					if (PreviousPin)
					{
						TestFalse(TEXT("이전 핀은 선택 해제된다"), PreviousPin->GetSelected());
						UImage* PreviousRing = Cast<UImage>(PreviousPin->GetWidgetFromName(TEXT("RingImage")));
						TestTrue(TEXT("이전 국가 산업색이 정확히 복원된다"), PreviousRing && PreviousRing->GetColorAndOpacity().Equals(PreviousBaseColor));
					}
					const FLayoutBox FocusedMap = MeasureNamed(*Panel, TEXT("MapBaseImage"), Origin);
					TestTrue(TEXT("포커스 후 지도 가장자리에 빈 공간이 없다"), FocusedMap.Left <= ViewportBox.Left + EdgeTolerance
						&& FocusedMap.Top <= ViewportBox.Top + EdgeTolerance && FocusedMap.Right() >= DrawerLeft - EdgeTolerance
						&& FocusedMap.Bottom() >= ViewportBox.Bottom() - EdgeTolerance);
					for (UWidget* Part : { static_cast<UWidget*>(Pin), Pin->GetWidgetFromName(TEXT("StatusPlate")) })
					{
						const FLayoutBox PartBox = MeasureGeometry(Part, Origin);
						TestTrue(FString::Printf(TEXT("%s: %s 원반과 라벨 전체가 가시 영역 안"), *WidthLabel, ToCountryCode(FocusCountry)),
							PartBox.HasArea() && PartBox.Left >= ViewportBox.Left - EdgeTolerance && PartBox.Top >= ViewportBox.Top - EdgeTolerance
							&& PartBox.Right() <= DrawerLeft + EdgeTolerance && PartBox.Bottom() <= ViewportBox.Bottom() + EdgeTolerance);
					}
					const bool bOriginalReduced = FMotionGate::IsReduced();
					FMotionGate::SetReduced(false);
					Pin->SetFocusHighlight(false);
					Pin->SetFocusHighlight(true);
					const FLinearColor PulseStart = Ring->GetColorAndOpacity();
					Pin->UpdateFocusHighlight(0.8f);
					TestFalse(TEXT("선택 링 밝기가 시간에 따라 변한다"), Ring->GetColorAndOpacity().Equals(PulseStart));
					FMotionGate::SetReduced(true);
					Pin->UpdateFocusHighlight(0.0f);
					const FLinearColor ReducedColor = Ring->GetColorAndOpacity();
					Pin->UpdateFocusHighlight(0.8f);
					TestTrue(TEXT("연출 감소에서는 선택 표시가 정적이다"), Ring->GetColorAndOpacity().Equals(ReducedColor));
					FMotionGate::SetReduced(bOriginalReduced);
					const FLinearColor RefreshedStateColor(0.15f, 0.31f, 0.44f, 0.73f);
					Pin->SetRingColor(RefreshedStateColor);
					Pin->UpdateFocusHighlight(0.2f);
					UCommonTextBlock* Code = Cast<UCommonTextBlock>(Pin->GetWidgetFromName(TEXT("CodeText")));
					TestTrue(TEXT("선택해도 링 알파와 코드의 상태색 일치를 유지한다"),
						FMath::IsNearlyEqual(Ring->GetColorAndOpacity().A, RefreshedStateColor.A)
						&& Code && Code->GetColorAndOpacity().GetSpecifiedColor().Equals(Ring->GetColorAndOpacity()));
					Pin->SetFocusHighlight(false);
					TestTrue(TEXT("선택 중 상태 갱신 뒤에도 최신 산업색을 복원한다"), Ring->GetColorAndOpacity().Equals(RefreshedStateColor));
					Pin->SetRingColor(BaseColor);
					Pin->SetFocusHighlight(true);
					if (CaptureWidth == DetailWidth && FocusCountry == ECountryType::Japan)
					{
						FImage UnselectedFrame;
						FImage SelectedFrame;
						Pin->SetFocusHighlight(false);
						const bool bUnselectedRendered = DrawToImage(PanelSlate, CaptureSize, UnselectedFrame);
						Pin->SetFocusHighlight(true);
						const bool bSelectedRendered = DrawToImage(PanelSlate, CaptureSize, SelectedFrame);
						TestTrue(TEXT("선택 표시 비교 프레임을 렌더했다"), bUnselectedRendered && bSelectedRendered);
						if (bUnselectedRendered && bSelectedRendered)
						{
							const FLayoutBox PinBox = MeasureGeometry(Pin, Origin);
							const double RenderScale = PinBox.Width / Pin->GetDesiredSize().X;
							const FVector2D PinCenter(PinBox.Left + PinBox.Width * 0.5, PinBox.Top + PinBox.Height * 0.5);
							const double InnerRadius = PinBox.Width * 0.5 + 2.0 * RenderScale;
							const double CoreOuterRadius = PinBox.Width * 0.5 + 8.0 * RenderScale;
							const double OuterRadius = PinBox.Width * 0.5 + 16.0 * RenderScale;
						int32 GoldCorePixels = 0;
						int32 GoldHaloPixels = 0;
						FVector2D GoldCorePositionSum = FVector2D::ZeroVector;
							for (int32 Y = FMath::FloorToInt32(PinCenter.Y - OuterRadius); Y <= FMath::CeilToInt32(PinCenter.Y + OuterRadius); ++Y)
							{
								for (int32 X = FMath::FloorToInt32(PinCenter.X - OuterRadius); X <= FMath::CeilToInt32(PinCenter.X + OuterRadius); ++X)
								{
									const double Distance = FVector2D::Distance(FVector2D(X + 0.5, Y + 0.5), PinCenter);
									if (Distance < InnerRadius || Distance > OuterRadius) { continue; }
									const FColor* Before = PixelAt(UnselectedFrame, X, Y);
									const FColor* After = PixelAt(SelectedFrame, X, Y);
									if (!Before || !After) { continue; }
									if (Distance <= CoreOuterRadius)
									{
										if (After->R > After->B + 30 && After->G > After->B + 20
											&& LumaOf(*After) > 160.0 && LumaOf(*After) - LumaOf(*Before) > 20.0)
										{
											++GoldCorePixels;
											GoldCorePositionSum += FVector2D(X + 0.5, Y + 0.5);
										}
									}
									else if (After->R > After->B + 8 && After->G > After->B + 5
										&& int32(After->R) - int32(Before->R) > int32(After->B) - int32(Before->B) + 4)
									{
										++GoldHaloPixels;
									}
								}
							}
						TestTrue(TEXT("선택하면 산업링 바깥에 밝은 금색 코어가 그려진다"), GoldCorePixels >= 24);
						TestTrue(TEXT("금색 코어 바깥에 노란빛 후광이 그려진다"), GoldHaloPixels >= 24);
						// PaintArgs 오프셋은 논리 desktop 좌표다. RT 픽셀 원점은 계속 (0,0)이며,
						// 같은 위젯의 금색 링은 창을 옮겨도 동일한 RT 위치에 남아야 한다.
						const FVector2D WindowToDesktopOffset(350.0, 180.0);
						FImage OffsetUnselectedFrame;
						FImage OffsetSelectedFrame;
						Pin->SetFocusHighlight(false);
						const bool bOffsetUnselectedRendered = DrawToImage(PanelSlate, CaptureSize,
							OffsetUnselectedFrame, 0.0f, WindowToDesktopOffset);
						Pin->SetFocusHighlight(true);
						const bool bOffsetSelectedRendered = DrawToImage(PanelSlate, CaptureSize,
							OffsetSelectedFrame, 0.0f, WindowToDesktopOffset);
						TestTrue(TEXT("창 원점 (350,180) 선택 비교 프레임을 렌더했다"),
							bOffsetUnselectedRendered && bOffsetSelectedRendered);
						if (bOffsetUnselectedRendered && bOffsetSelectedRendered)
						{
							int32 OffsetGoldCorePixels = 0;
							FVector2D OffsetGoldCorePositionSum = FVector2D::ZeroVector;
							for (int32 Y = FMath::FloorToInt32(PinCenter.Y - CoreOuterRadius);
								Y <= FMath::CeilToInt32(PinCenter.Y + CoreOuterRadius); ++Y)
							{
								for (int32 X = FMath::FloorToInt32(PinCenter.X - CoreOuterRadius);
									X <= FMath::CeilToInt32(PinCenter.X + CoreOuterRadius); ++X)
								{
									const double Distance = FVector2D::Distance(FVector2D(X + 0.5, Y + 0.5), PinCenter);
									if (Distance < InnerRadius || Distance > CoreOuterRadius) { continue; }
									const FColor* Before = PixelAt(OffsetUnselectedFrame, X, Y);
									const FColor* After = PixelAt(OffsetSelectedFrame, X, Y);
									if (Before && After && After->R > After->B + 30 && After->G > After->B + 20
										&& LumaOf(*After) > 160.0 && LumaOf(*After) - LumaOf(*Before) > 20.0)
									{
										++OffsetGoldCorePixels;
										OffsetGoldCorePositionSum += FVector2D(X + 0.5, Y + 0.5);
									}
								}
							}
							TestTrue(TEXT("창 원점이 달라도 일본 메달리온 주변에 금색 코어가 남는다"),
								OffsetGoldCorePixels >= 24);
							if (GoldCorePixels >= 24 && OffsetGoldCorePixels >= 24)
							{
								const FVector2D CoreCenter = GoldCorePositionSum / GoldCorePixels;
								const FVector2D OffsetCoreCenter = OffsetGoldCorePositionSum / OffsetGoldCorePixels;
								TestTrue(TEXT("창 원점을 옮겨도 금색 코어의 RT 픽셀 중심은 같다"),
									CoreCenter.Equals(OffsetCoreCenter, 1.0));
							}
						}
						SavePNG(UnselectedFrame, Directory / TEXT("worldmap_selection_off.png"));
							SavePNG(SelectedFrame, Directory / TEXT("worldmap_selection_on.png"));
						}
					}
					PreviousPin = Pin;
					PreviousBaseColor = BaseColor;
				}

				TestTrue(TEXT("공용 헤더 닫기 버튼을 한 번 누른다"), ClickDetailClose(Drawer));
				TestEqual(TEXT("닫기 한 번으로 국가 상세 스택이 비워진다"), TestDrawerStack->GetNumWidgets(), 0);
				FImage Rebaseline;
				DrawToImage(PanelSlate, CaptureSize, Rebaseline);
				TestEqual(TEXT("닫기 후 지도 배율이 원래 1로 돌아온다"), Panel->GetAppliedFocusScale(), 1.0f, 1e-3f);
				TestTrue(TEXT("닫기 후 지도 위치가 원점으로 돌아온다"), Panel->GetAppliedFocusTranslation().IsNearlyZero(0.01));
				TestEqual(TEXT("포커스 해제하면 선택 국가도 비운다"), Panel->GetSelectedCountry(), ECountryType::None);
				if (PreviousPin)
				{
					UImage* PreviousRing = Cast<UImage>(PreviousPin->GetWidgetFromName(TEXT("RingImage")));
					TestTrue(TEXT("마지막 선택도 원래 색으로 복원된다"), PreviousRing && PreviousRing->GetColorAndOpacity().Equals(PreviousBaseColor));
				}
				TestTrue(TEXT("닫은 후 다시 국가 클릭으로 상세를 연다"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Canada)));
				DrawToImage(PanelSlate, CaptureSize, Rebaseline);
				TestTrue(TEXT("백 액션 한 번으로도 상세를 닫는다"), Panel->NativeOnHandleBackAction());
				DrawToImage(PanelSlate, CaptureSize, Rebaseline);
				TestEqual(TEXT("백 액션 뒤 상세 스택이 비었다"), TestDrawerStack->GetNumWidgets(), 0);
				TestEqual(TEXT("백 액션 뒤 배율도 1"), Panel->GetAppliedFocusScale(), 1.0f, 1e-3f);
				TestTrue(TEXT("백 액션 뒤 위치도 원점"), Panel->GetAppliedFocusTranslation().IsNearlyZero(0.01));
				TestEqual(TEXT("백 액션 뒤 선택도 해제"), Panel->GetSelectedCountry(), ECountryType::None);
			}
			else
			{
				AddError(FString::Printf(TEXT("[Task9][%s] PushDrawer 가 nullptr ― DrawerStack 이 트리에 없습니다."), *WidthLabel));
			}
		}

		TArray<ECountryType> Countries;
		Panel->GetMedallions().GetKeys(Countries);
		Countries.Sort([](const ECountryType& Lhs, const ECountryType& Rhs)
			{ return static_cast<int32>(Lhs) < static_cast<int32>(Rhs); });

		int32 SizedPins = 0;
		for (const ECountryType Country : Countries)
		{
			const TObjectPtr<UCountryNameWidget>* Found = Panel->GetMedallions().Find(Country);
			const UCountryNameWidget* Pin = Found != nullptr ? Found->Get() : nullptr;
			if (Pin != nullptr && MeasureGeometry(Pin, Origin).HasArea())
			{
				++SizedPins;
			}
		}
		const int64 OpaquePixels = CountOpaquePixels(LiveFrame);

		WidthJson->SetObjectField(TEXT("board"), BoxJson(BoardBox));
		WidthJson->SetNumberField(TEXT("medallions"), Countries.Num());
		WidthJson->SetNumberField(TEXT("sized_medallions"), SizedPins);
		WidthJson->SetNumberField(TEXT("opaque_pixels"), static_cast<double>(OpaquePixels));

		// ── 게이트 ② : 0 자기검증 ──────────────────────────────────────────
		// 판 크기 · 메달리온 크기 · 불투명 픽셀 중 하나라도 0 이면 **판정 불가**다. 통과가 아니다.
		if (!BoardBox.HasArea() || Countries.Num() == 0 || SizedPins == 0 || OpaquePixels == 0)
		{
			AddError(FString::Printf(
				TEXT("[Task9][%s] 0 자기검증 실패 ― 판 %.1fx%.1f · 메달리온 %d개(크기 잡힌 것 %d) · 불투명 픽셀 %.0f. ")
				TEXT("이 런은 **판정 불가**이며 통과가 아닙니다. 아래 비겹침·포함 단언은 0 을 초록으로 읽으므로 돌리지 않습니다. ")
				TEXT("-nullrhi 없이 실제 RHI 세션에서 다시 실행하세요."),
				*WidthLabel, BoardBox.Width, BoardBox.Height, Countries.Num(), SizedPins,
				static_cast<double>(OpaquePixels)));
			Report->SetObjectField(WidthLabel, WidthJson);
			FString AbortJson;
			const TSharedRef<TJsonWriter<>> AbortWriter = TJsonWriterFactory<>::Create(&AbortJson);
			FJsonSerializer::Serialize(Report.ToSharedRef(), AbortWriter);
			FFileHelper::SaveStringToFile(AbortJson, *(Directory / TEXT("worldmap_table_render.json")));
			return false;
		}

		// ── ① 캔버스 ≡ 판 (Task 6 리뷰 F-2) ───────────────────────────────
		// 네 레이어가 같은 사각형이 아니면 앵커가 **조용히 비례해서** 어긋난다. 에러도 로그도 안 남는 종류다.
		const TCHAR* LayerNames[] = { TEXT("MedallionCanvas"), TEXT("RegionShadeCanvas"), TEXT("RouteLayer") };
		TArray<TSharedPtr<FJsonValue>> LayerRecords;
		for (const TCHAR* LayerName : LayerNames)
		{
			const FLayoutBox LayerBox = MeasureNamed(*Panel, LayerName, Origin);
			const double DeltaW = FMath::Abs(LayerBox.Width - BoardBox.Width);
			const double DeltaH = FMath::Abs(LayerBox.Height - BoardBox.Height);
			const double DeltaX = FMath::Abs(LayerBox.Left - BoardBox.Left);
			const double DeltaY = FMath::Abs(LayerBox.Top - BoardBox.Top);
			const bool bMatched = LayerBox.HasArea()
				&& DeltaW <= CanvasMatchTolerance && DeltaH <= CanvasMatchTolerance
				&& DeltaX <= CanvasMatchTolerance && DeltaY <= CanvasMatchTolerance;

			if (!bMatched)
			{
				// Warning 이 아니라 Error ― 눈으로는 "선이 좀 어긋났네" 로만 보이는 종류다
				UE_LOG(LogTemp, Error,
					TEXT("[WorldMapPanel] %s 가 판(MapBaseImage)과 같은 사각형이 아닙니다. 크기차 (%.2f, %.2f) 위치차 (%.2f, %.2f). ")
					TEXT("같은 Overlay 의 Fill/Fill · 패딩 0 형제로 두세요 ― 어긋나면 앵커가 비례해서 조용히 틀립니다."),
					LayerName, DeltaW, DeltaH, DeltaX, DeltaY);
			}
			TestTrue(FString::Printf(TEXT("%s: %s ≡ 판 (크기차 %.2f/%.2f · 위치차 %.2f/%.2f ≤ %.1fpx)"),
				*WidthLabel, LayerName, DeltaW, DeltaH, DeltaX, DeltaY, CanvasMatchTolerance), bMatched);

			TSharedPtr<FJsonObject> LayerJson = BoxJson(LayerBox);
			LayerJson->SetStringField(TEXT("name"), LayerName);
			LayerJson->SetNumberField(TEXT("delta_w"), DeltaW);
			LayerJson->SetNumberField(TEXT("delta_h"), DeltaH);
			LayerJson->SetNumberField(TEXT("delta_x"), DeltaX);
			LayerJson->SetNumberField(TEXT("delta_y"), DeltaY);
			LayerRecords.Add(MakeShared<FJsonValueObject>(LayerJson));
		}
		WidthJson->SetArrayField(TEXT("layers"), LayerRecords);

		// ── ⑦ 수요 닷 3종 ────────────────────────────────────────────────
		// DT_CountryDemand 가 오라클이다(위젯이 읽는 MarketManager 와 **다른 출처**라야 검증이 성립한다).
		// 한 국가라도 3개 전부 안 보이면 「닷이 트리에 없다」는 옛 버그의 재발이다.
		const TCHAR* DotNames[] = { TEXT("DotSemiconductor"), TEXT("DotElectronics"), TEXT("DotAutomobile") };
		const ECompanyType DotIndustries[] = { ECompanyType::Semiconductor, ECompanyType::Electronics, ECompanyType::Automobile };
		FLinearColor IndustryAccents[UE_ARRAY_COUNT(DotIndustries)];
		for (int32 DotIndex = 0; DotIndex < static_cast<int32>(UE_ARRAY_COUNT(DotIndustries)); ++DotIndex)
		{
			bool bCompanyOk = false;
			const FCompanyInfoTable CompanyInfo = TableManager->GetCompanyInfo(DotIndustries[DotIndex], bCompanyOk);
			TestTrue(*FString::Printf(TEXT("%s: %s 의 DT_CompanyInfo.AccentColor 가 유효하다"),
				*WidthLabel, DotNames[DotIndex]),
				bCompanyOk && CompanyInfo.AccentColor.A >= KINDA_SMALL_NUMBER);
			IndustryAccents[DotIndex] = CompanyInfo.AccentColor;
		}
		int32 TotalDefinedCells = 0;
		int32 TotalVisibleDots = 0;
		TArray<TSharedPtr<FJsonValue>> DotRecords;
		for (const ECountryType Country : Countries)
		{
			const TObjectPtr<UCountryNameWidget>* FoundPin = Panel->GetMedallions().Find(Country);
			UCountryNameWidget* Pin = FoundPin != nullptr ? FoundPin->Get() : nullptr;
			if (Pin == nullptr)
			{
				continue;
			}
			const FString Label = CountryLabel(Country);
			int32 DefinedHere = 0;
			int32 VisibleHere = 0;
			for (int32 DotIndex = 0; DotIndex < 3; ++DotIndex)
			{
				bool bCellDefined = false;
				TableManager->GetCountryDemand(Country, DotIndustries[DotIndex], bCellDefined);
				DefinedHere += bCellDefined ? 1 : 0;
				TotalDefinedCells += bCellDefined ? 1 : 0;

				UWidget* Dot = Pin->GetWidgetFromName(FName(DotNames[DotIndex]));
				// ★ 트리에 없으면 값 로직이 아무리 살아 있어도 화면엔 아무것도 안 뜬다(몇 달간 조용히 죽어 있던 그 버그)
				TestNotNull(*FString::Printf(TEXT("%s: %s 의 %s 가 메달리온 트리에 있다"),
					*WidthLabel, *Label, DotNames[DotIndex]), Dot);
				if (Dot == nullptr)
				{
					continue;
				}
				const bool bDotVisible = IsRendered(Dot);
				VisibleHere += bDotVisible ? 1 : 0;
				TotalVisibleDots += bDotVisible ? 1 : 0;
				TestTrue(*FString::Printf(TEXT("%s: %s %s 가시성(%s) == DT 정의 여부(%s)"),
					*WidthLabel, *Label, DotNames[DotIndex],
					bDotVisible ? TEXT("보임") : TEXT("접힘"), bCellDefined ? TEXT("정의됨") : TEXT("없음")),
					bDotVisible == bCellDefined);
				if (bDotVisible)
				{
					TestTrue(*FString::Printf(TEXT("%s: %s %s 가 크기를 갖는다"), *WidthLabel, *Label, DotNames[DotIndex]),
						MeasureGeometry(Dot, Origin).HasArea());
					const UImage* DotImage = Cast<UImage>(Dot);
					TestTrue(*FString::Printf(TEXT("%s: %s %s 색은 DT_CompanyInfo.AccentColor"),
						*WidthLabel, *Label, DotNames[DotIndex]),
						DotImage && DotImage->GetColorAndOpacity().Equals(IndustryAccents[DotIndex], KINDA_SMALL_NUMBER));
				}
			}
			// DT 에 셀이 하나라도 있는데 닷이 하나도 안 보이면 그 국가는 수요를 말하지 못한다
			TestTrue(*FString::Printf(TEXT("%s: %s 수요 닷 (DT 정의 %d / 보이는 닷 %d)"),
				*WidthLabel, *Label, DefinedHere, VisibleHere),
				DefinedHere == 0 || VisibleHere > 0);

			TSharedPtr<FJsonObject> DotJson = MakeShared<FJsonObject>();
			DotJson->SetStringField(TEXT("country"), Label);
			DotJson->SetNumberField(TEXT("defined"), DefinedHere);
			DotJson->SetNumberField(TEXT("visible"), VisibleHere);
			DotRecords.Add(MakeShared<FJsonValueObject>(DotJson));
		}
		WidthJson->SetArrayField(TEXT("demand_dots"), DotRecords);
		WidthJson->SetNumberField(TEXT("demand_cells_defined"), TotalDefinedCells);
		WidthJson->SetNumberField(TEXT("demand_dots_visible"), TotalVisibleDots);
		TestTrue(FString::Printf(TEXT("%s: 판 전체에서 보이는 수요 닷 %d개 (DT 정의 %d셀)"),
			*WidthLabel, TotalVisibleDots, TotalDefinedCells),
			TotalDefinedCells == 0 || TotalVisibleDots > 0);

		// ── ④ 라벨 사각형 교차 0 (Task 3 리뷰 I-3) ────────────────────────
		// AnchorDistance 는 중심간 거리만 잰다. 메달리온은 원반 + 한쪽으로 튀어나온 라벨이라
		// LabelSide 가 그걸 엇갈리게 하라고 있는데 **강제하는 단언이 없었다.** 여기가 그 단언이다.
		for (const ECountryType Country : Countries)
		{
			const TObjectPtr<UCountryNameWidget>* FoundPin = Panel->GetMedallions().Find(Country);
			if (UCountryNameWidget* Pin = FoundPin != nullptr ? FoundPin->Get() : nullptr)
			{
				Pin->SetStatusLine(FixtureStatusLine());
				// 알림 도트는 실플레이에서 포화 라인이 생겨야 뜨는데 그 조건이 캡처에 한 번도 안 걸렸다 ―
				// 라벨 픽스처에서 11개 전부 켜서 위치·크기를 눈으로 볼 수 있게 한다.
				Pin->SetAlertVisible(true);
			}
		}

		FImage LabelFrame;
		const FString LabelName = FString::Printf(TEXT("worldmap_%d_labels.png"), CaptureWidth);
		const bool bLabelOk = DrawToImage(PanelSlate, CaptureSize, LabelFrame)
			&& SavePNG(LabelFrame, Directory / LabelName);
		TestTrue(FString::Printf(TEXT("%s: 라벨 픽스처 캡처 저장 (%s)"), *WidthLabel, *LabelName), bLabelOk);

		const FLayoutBox LabelBoardBox = MeasureNamed(*Panel, TEXT("MapBaseImage"), Origin);
		TArray<FLayoutBox> PinBoxes;
		TArray<FLayoutBox> StatusBoxes;
		// ★ 겹침을 만드는 것은 글자가 아니라 **불투명한 라벨판** 이다. StatusText 만 재면 판이 이름 줄만큼
		//   더 크고 좌우로 17px 더 넓다는 사실이 통째로 빠져, 이웃 원반을 덮고 판 밖으로 나가도 0 이 나온다
		//   (2026-09-25 실측: 중국 라벨이 싱가포르 원반을 덮는데 이 게이트는 통과했다).
		TArray<FLayoutBox> PlateBoxes;
		TArray<FLayoutBox> DotBoxes;
		PinBoxes.SetNum(Countries.Num());
		StatusBoxes.SetNum(Countries.Num());
		PlateBoxes.SetNum(Countries.Num());
		DotBoxes.SetNum(Countries.Num());
		// 사용자가 "오른쪽 위 메뉴랑 겹친다" 고 지적한 그 묶음. 못 찾으면 단언이 조용히 건너뛰므로 먼저 큰소리로 본다
		const FLayoutBox WalletBox = MeasureNamed(*Panel, TEXT("WalletRow"), Origin);
		TestTrue(*FString::Printf(TEXT("%s: WalletRow 를 트리에서 찾았다 (%.1f,%.1f %.1fx%.1f)"),
			*WidthLabel, WalletBox.Left, WalletBox.Top, WalletBox.Width, WalletBox.Height), WalletBox.bFound);
		TArray<TSharedPtr<FJsonValue>> PinRecords;

		bool bAllPinsSized = true;
		for (int32 Index = 0; Index < Countries.Num(); ++Index)
		{
			const ECountryType Country = Countries[Index];
			const FString Label = CountryLabel(Country);
			const TObjectPtr<UCountryNameWidget>* FoundPin = Panel->GetMedallions().Find(Country);
			UCountryNameWidget* Pin = FoundPin != nullptr ? FoundPin->Get() : nullptr;
			if (Pin == nullptr)
			{
				AddError(FString::Printf(TEXT("[Task9][%s] %s 메달리온이 null 입니다."), *WidthLabel, *Label));
				bAllPinsSized = false;
				continue;
			}

			// Collapsed 를 먼저 거른 뒤 0 을 본다 (UI_CREATION_PLAYBOOK §8-12 ― 0 은 세 가지가 뭉친 값이다)
			if (!IsRendered(Pin))
			{
				AddError(FString::Printf(TEXT("[Task9][%s] %s 메달리온이 Collapsed/Hidden 입니다 ― 판에서 사라진 국가입니다."), *WidthLabel, *Label));
				bAllPinsSized = false;
				continue;
			}
			PinBoxes[Index] = MeasureGeometry(Pin, Origin);
			if (!PinBoxes[Index].HasArea())
			{
				AddError(FString::Printf(
					TEXT("[Task9][%s] %s 메달리온 지오메트리가 0 입니다 ― Visible 인데 크기가 없습니다(실제 0 크기 또는 미페인트). 판정 불가."),
					*WidthLabel, *Label));
				bAllPinsSized = false;
				continue;
			}

			UWidget* Status = Pin->GetWidgetFromName(TEXT("StatusText"));
			TestNotNull(*FString::Printf(TEXT("%s: %s StatusText 가 트리에 있다"), *WidthLabel, *Label), Status);
			if (Status != nullptr)
			{
				// 픽스처로 비지 않은 문자열을 넣었으므로 **반드시** 보여야 한다
				TestTrue(*FString::Printf(TEXT("%s: %s 상태 줄이 보인다"), *WidthLabel, *Label), IsRendered(Status));
				StatusBoxes[Index] = MeasureGeometry(Status, Origin);
				TestTrue(*FString::Printf(TEXT("%s: %s 상태 줄이 크기를 갖는다 (%.1fx%.1f)"),
					*WidthLabel, *Label, StatusBoxes[Index].Width, StatusBoxes[Index].Height),
					StatusBoxes[Index].HasArea());
			}

			UWidget* Plate = Pin->GetWidgetFromName(TEXT("StatusPlate"));
			TestNotNull(*FString::Printf(TEXT("%s: %s StatusPlate 가 트리에 있다"), *WidthLabel, *Label), Plate);
			if (Plate != nullptr)
			{
				PlateBoxes[Index] = MeasureGeometry(Plate, Origin);
				TestTrue(*FString::Printf(TEXT("%s: %s 라벨판이 크기를 갖는다 (%.1fx%.1f)"),
					*WidthLabel, *Label, PlateBoxes[Index].Width, PlateBoxes[Index].Height),
					PlateBoxes[Index].HasArea());

				// 라벨판이 **한 줄짜리 상태 줄 + 이름 줄** 을 넘지 않는지. 상태 줄이 두 줄로 감기면 판이
				// 31px 자라 16:10 에서 판 위아래로 넘친다 ― 그 원인을 결과(넘침)보다 먼저 이름 붙여 잡는다.
				TestTrue(*FString::Printf(TEXT("%s: %s 상태 줄이 한 줄 (실측 높이 %.1fpx, 상한 %.1f)"),
					*WidthLabel, *Label, StatusBoxes[Index].Height, MaxStatusLineHeight),
					!StatusBoxes[Index].HasArea() || StatusBoxes[Index].Height <= MaxStatusLineHeight);

				// 판 사각형이 지도 판 안에 있다 ― 위로 넘치면 우상단 지갑/메뉴와, 아래로 넘치면 하단 도크와 싸운다
				TestTrue(*FString::Printf(TEXT("%s: %s 라벨판이 판 안 (판 %.1f,%.1f~%.1f,%.1f / 지도 %.1f,%.1f~%.1f,%.1f)"),
					*WidthLabel, *Label, PlateBoxes[Index].Left, PlateBoxes[Index].Top,
					PlateBoxes[Index].Right(), PlateBoxes[Index].Bottom(),
					LabelBoardBox.Left, LabelBoardBox.Top, LabelBoardBox.Right(), LabelBoardBox.Bottom()),
					!PlateBoxes[Index].HasArea() || IsInside(LabelBoardBox, PlateBoxes[Index], EdgeTolerance));

				if (WalletBox.HasArea() && PlateBoxes[Index].HasArea())
				{
					const double WalletOverlap = OverlapArea(PlateBoxes[Index], WalletBox);
					TestTrue(*FString::Printf(TEXT("%s: %s 라벨판이 우상단 지갑/메뉴와 교차 0 (실측 %.1fpx²)"),
						*WidthLabel, *Label, WalletOverlap), WalletOverlap <= 0.0);
				}
			}

			// 수요 글리프 행 ― 원반 사각형과 교차하면 8px 링을 덮어 진출 상태 신호가 끊긴다
			UWidget* Dots = Pin->GetWidgetFromName(TEXT("DotRow"));
			TestNotNull(*FString::Printf(TEXT("%s: %s DotRow 가 트리에 있다"), *WidthLabel, *Label), Dots);
			if (Dots != nullptr)
			{
				// DT 에 수요 셀이 하나도 없는 국가는 닷 3개가 전부 Collapsed 라 행이 0 이다 ―
				// 그건 결함이 아니므로 크기는 단언하지 않고, 크기가 있을 때만 위치를 본다(수량 단언은 ③이 한다).
				DotBoxes[Index] = MeasureGeometry(Dots, Origin);
				if (DotBoxes[Index].HasArea())
				{
					const double RingOverlap = OverlapArea(DotBoxes[Index], PinBoxes[Index]);
					TestTrue(*FString::Printf(TEXT("%s: %s 수요 글리프가 링 바깥 (행 %.1fx%.1f · 원반과 교차 %.1fpx²)"),
						*WidthLabel, *Label, DotBoxes[Index].Width, DotBoxes[Index].Height, RingOverlap),
						RingOverlap <= EdgeTolerance * DotBoxes[Index].Width);

					if (PlateBoxes[Index].HasArea())
					{
						// ★ 계약이 뒤집혔다 (2026-09-26). 닷은 원반 옆이 아니라 **라벨판 3번째 줄**이다 ―
						//   원반 폭 88 이 행 폭을 묶고 있어 폰에서 글리프가 1.24mm 였고, 판(≈170) 안으로
						//   들어오며 48px(2.3mm)이 됐다. 구 단언("판에 안 깔림")은 지금 상태를 결함으로 읽는다.
						//   이제 요구는 **완전 포함**이다 ― 걸치면 판 밖으로 삐져 잘린다.
						const double PlateDotOverlap = OverlapArea(DotBoxes[Index], PlateBoxes[Index]);
						const double DotArea = DotBoxes[Index].Width * DotBoxes[Index].Height;
						TestTrue(*FString::Printf(TEXT("%s: %s 수요 글리프가 제 라벨판 **안에** 들어 있다 (행 %.1fx%.1f · 포함 %.1f/%.1fpx²)"),
							*WidthLabel, *Label, DotBoxes[Index].Width, DotBoxes[Index].Height, PlateDotOverlap, DotArea),
							DotArea > 0.0 && PlateDotOverlap >= DotArea - EdgeTolerance * DotBoxes[Index].Width);
					}
				}
			}

			// Step 1 ― 라벨을 포함한 메달리온이 판 사각형 안에 있다
			TestTrue(*FString::Printf(TEXT("%s: %s 메달리온이 판 안 (핀 %.1f,%.1f~%.1f,%.1f / 판 %.1f,%.1f~%.1f,%.1f)"),
				*WidthLabel, *Label, PinBoxes[Index].Left, PinBoxes[Index].Top, PinBoxes[Index].Right(), PinBoxes[Index].Bottom(),
				LabelBoardBox.Left, LabelBoardBox.Top, LabelBoardBox.Right(), LabelBoardBox.Bottom()),
				IsInside(LabelBoardBox, PinBoxes[Index], EdgeTolerance));

			TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
			PinJson->SetStringField(TEXT("country"), Label);
			PinJson->SetObjectField(TEXT("pin"), BoxJson(PinBoxes[Index]));
			PinJson->SetObjectField(TEXT("status"), BoxJson(StatusBoxes[Index]));
			PinJson->SetObjectField(TEXT("plate"), BoxJson(PlateBoxes[Index]));
			PinJson->SetObjectField(TEXT("dots"), BoxJson(DotBoxes[Index]));
			PinRecords.Add(MakeShared<FJsonValueObject>(PinJson));
		}
		WidthJson->SetArrayField(TEXT("pins"), PinRecords);

		if (!bAllPinsSized)
		{
			// 하나라도 0 이면 그 국가가 빠진 채로 "겹침 0" 이 나온다 ― 위에서 이미 AddError 했고 여기서는 쌍 비교를 건너뛴다
			AddError(FString::Printf(TEXT("[Task9][%s] 크기가 0 이거나 접힌 메달리온이 있어 쌍 교차 판정을 신뢰할 수 없습니다."), *WidthLabel));
		}

		double WorstPinOverlap = 0.0;
		double WorstStatusOverlap = 0.0;
		double WorstPlateOverlap = 0.0;
		FString WorstPinPair;
		FString WorstStatusPair;
		FString WorstPlatePair;
		for (int32 First = 0; First < Countries.Num(); ++First)
		{
			for (int32 Second = First + 1; Second < Countries.Num(); ++Second)
			{
				const FString PairLabel = FString::Printf(TEXT("%s<->%s"),
					*CountryLabel(Countries[First]), *CountryLabel(Countries[Second]));

				if (PinBoxes[First].HasArea() && PinBoxes[Second].HasArea())
				{
					const double PinOverlap = OverlapArea(PinBoxes[First], PinBoxes[Second]);
					if (PinOverlap > WorstPinOverlap) { WorstPinOverlap = PinOverlap; WorstPinPair = PairLabel; }
					TestTrue(*FString::Printf(TEXT("%s: %s 메달리온 교차 0 (실측 %.1fpx²)"), *WidthLabel, *PairLabel, PinOverlap),
						PinOverlap <= 0.0);
				}
				if (StatusBoxes[First].HasArea() && StatusBoxes[Second].HasArea())
				{
					const double StatusOverlap = OverlapArea(StatusBoxes[First], StatusBoxes[Second]);
					if (StatusOverlap > WorstStatusOverlap) { WorstStatusOverlap = StatusOverlap; WorstStatusPair = PairLabel; }
					TestTrue(*FString::Printf(TEXT("%s: %s 라벨 사각형 교차 0 (실측 %.1fpx²)"), *WidthLabel, *PairLabel, StatusOverlap),
						StatusOverlap <= 0.0);
				}
				if (PlateBoxes[First].HasArea() && PlateBoxes[Second].HasArea())
				{
					const double PlateOverlap = OverlapArea(PlateBoxes[First], PlateBoxes[Second]);
					if (PlateOverlap > WorstPlateOverlap) { WorstPlateOverlap = PlateOverlap; WorstPlatePair = PairLabel; }
					TestTrue(*FString::Printf(TEXT("%s: %s 라벨판 교차 0 (실측 %.1fpx²)"), *WidthLabel, *PairLabel, PlateOverlap),
						PlateOverlap <= 0.0);
				}
				// 불투명한 라벨판이 **남의 원반** 을 덮으면 그 국가는 판에서 읽히지 않는다
				for (int32 Direction = 0; Direction < 2; ++Direction)
				{
					const int32 PlateIndex = Direction == 0 ? First : Second;
					const int32 DiscIndex  = Direction == 0 ? Second : First;
					if (!PlateBoxes[PlateIndex].HasArea() || !PinBoxes[DiscIndex].HasArea())
					{
						continue;
					}
					const double DiscOverlap = OverlapArea(PlateBoxes[PlateIndex], PinBoxes[DiscIndex]);
					TestTrue(*FString::Printf(TEXT("%s: %s 라벨판이 %s 원반을 안 덮음 (실측 %.1fpx²)"),
						*WidthLabel, *CountryLabel(Countries[PlateIndex]), *CountryLabel(Countries[DiscIndex]), DiscOverlap),
						DiscOverlap <= 0.0);
				}
			}
		}
		WidthJson->SetNumberField(TEXT("worst_pin_overlap_px2"), WorstPinOverlap);
		WidthJson->SetStringField(TEXT("worst_pin_pair"), WorstPinPair);
		WidthJson->SetNumberField(TEXT("worst_label_overlap_px2"), WorstStatusOverlap);
		WidthJson->SetStringField(TEXT("worst_label_pair"), WorstStatusPair);
		WidthJson->SetNumberField(TEXT("worst_plate_overlap_px2"), WorstPlateOverlap);
		WidthJson->SetStringField(TEXT("worst_plate_pair"), WorstPlatePair);
		WidthJson->SetObjectField(TEXT("wallet"), BoxJson(WalletBox));

		// ── ⑤ 최근접 쌍 크롭 ─────────────────────────────────────────────
		// 실측으로 여유가 가장 적은 두 쌍(미국↔캐나다 0.0626 · 한국↔일본 0.0655, 하한 0.051)은 반드시 그림으로 남긴다
		struct FClosePair { ECountryType A; ECountryType B; const TCHAR* Name; };
		const FClosePair ClosePairs[] = {
			{ ECountryType::USA, ECountryType::Canada, TEXT("USA_Canada") },
			{ ECountryType::Korea, ECountryType::Japan, TEXT("Korea_Japan") } };
		for (const FClosePair& Pair : ClosePairs)
		{
			const int32 IndexA = Countries.IndexOfByKey(Pair.A);
			const int32 IndexB = Countries.IndexOfByKey(Pair.B);
			if (IndexA == INDEX_NONE || IndexB == INDEX_NONE)
			{
				AddError(FString::Printf(TEXT("[Task9][%s] 최근접 쌍 %s 중 하나가 판에 없습니다 ― 크롭을 남기지 못했습니다."),
					*WidthLabel, Pair.Name));
				continue;
			}
			const FString CropName = FString::Printf(TEXT("worldmap_%d_pair_%s.png"), CaptureWidth, Pair.Name);
			const bool bCropOk = SaveCrop(LabelFrame, UnionBox(PinBoxes[IndexA], PinBoxes[IndexB]), 60.0, Directory / CropName);
			TestTrue(FString::Printf(TEXT("%s: 최근접 쌍 크롭 저장 (%s)"), *WidthLabel, *CropName), bCropOk);
		}

		// ── ②③ 링 픽셀 · ⑥ 항로 : 비율과 무관하므로 기준 폭에서만 ───────────
		if (CaptureWidth == DetailWidth)
		{
			// ★ 값 단언으로는 "Visible 인데 브러시 null · ImageSize 0 · 알파 0" 셋을 구분할 수 없다.
			//    링 색만 바꿔 두 장을 찍고 **달라진 픽셀**을 보면 셋 다 한 번에 잡힌다(안 그려지면 차이가 0).
			const FColor OwnedInk = OwnedMiningRingInk();
			const FColor AvailableInk = AvailableRingInk();

			for (const ECountryType Country : Countries)
			{
				const TObjectPtr<UCountryNameWidget>* FoundPin = Panel->GetMedallions().Find(Country);
				if (UCountryNameWidget* Pin = FoundPin != nullptr ? FoundPin->Get() : nullptr)
				{
					Pin->SetRingColor(FLinearColor::FromSRGBColor(OwnedInk));
				}
			}
			FImage RingOwnedFrame;
			const bool bRingAOk = DrawToImage(PanelSlate, CaptureSize, RingOwnedFrame)
				&& SavePNG(RingOwnedFrame, Directory / FString::Printf(TEXT("worldmap_%d_ring_owned.png"), CaptureWidth));
			TestTrue(TEXT("링 진출색 캡처 저장 (worldmap_2560_ring_owned.png)"), bRingAOk);

			for (const ECountryType Country : Countries)
			{
				const TObjectPtr<UCountryNameWidget>* FoundPin = Panel->GetMedallions().Find(Country);
				if (UCountryNameWidget* Pin = FoundPin != nullptr ? FoundPin->Get() : nullptr)
				{
					Pin->SetRingColor(FLinearColor::FromSRGBColor(AvailableInk));
				}
			}
			FImage RingAvailableFrame;
			const bool bRingBOk = DrawToImage(PanelSlate, CaptureSize, RingAvailableFrame)
				&& SavePNG(RingAvailableFrame, Directory / FString::Printf(TEXT("worldmap_%d_ring_available.png"), CaptureWidth));
			TestTrue(TEXT("링 진출가능색 캡처 저장 (worldmap_2560_ring_available.png)"), bRingBOk);

			const FVector OwnedChroma = ChromaOf(OwnedInk);
			const FVector AvailableChroma = ChromaOf(AvailableInk);
			TArray<TSharedPtr<FJsonValue>> RingRecords;

			if (bRingAOk && bRingBOk)
			{
				for (int32 Index = 0; Index < Countries.Num(); ++Index)
				{
					const ECountryType Country = Countries[Index];
					const FString Label = CountryLabel(Country);
					const TObjectPtr<UCountryNameWidget>* FoundPin = Panel->GetMedallions().Find(Country);
					UCountryNameWidget* Pin = FoundPin != nullptr ? FoundPin->Get() : nullptr;
					UWidget* Ring = Pin != nullptr ? Pin->GetWidgetFromName(TEXT("RingImage")) : nullptr;

					TestNotNull(*FString::Printf(TEXT("%s RingImage 가 메달리온 트리에 있다"), *Label), Ring);
					if (Ring == nullptr)
					{
						continue;
					}
					// ① 가시성 ② 크기 ― 둘 다 필요조건일 뿐이라 아래 픽셀 단언이 진짜 판정이다
					TestTrue(*FString::Printf(TEXT("%s RingImage 가 Collapsed/Hidden 이 아니다"), *Label), IsRendered(Ring));
					const FLayoutBox RingBox = MeasureGeometry(Ring, Origin);
					if (!RingBox.HasArea())
					{
						AddError(FString::Printf(
							TEXT("[Task9] %s RingImage 지오메트리가 0 입니다 ― 판정 불가(실제 0 크기 또는 미페인트)."), *Label));
						continue;
					}

					const FDiffStats RingDiff = DiffWithin(RingOwnedFrame, RingAvailableFrame,
						Inflate(RingBox, 6.0), OwnedInk, ChromaTolerance, TArray<FVector2D>());

					// ★ ② 실제로 그려지는가 ― 브러시 null · ImageSize 0 · 알파 0 이면 두 장이 똑같아 여기서 0 이 된다
					TestTrue(*FString::Printf(TEXT("%s 링이 실제로 그려진다 (색을 바꾸면 달라지는 픽셀 %d개 ≥ %d)"),
						*Label, RingDiff.Count, MinRingDiffPixels), RingDiff.Count >= MinRingDiffPixels);

					TSharedPtr<FJsonObject> RingJson = MakeShared<FJsonObject>();
					RingJson->SetStringField(TEXT("country"), Label);
					RingJson->SetObjectField(TEXT("ring_box"), BoxJson(RingBox));
					RingJson->SetNumberField(TEXT("diff_pixels"), RingDiff.Count);

					if (RingDiff.Count >= MinRingDiffPixels)
					{
						const FVector MeasuredOwned = RingDiff.MeanChromaA();
						const FVector MeasuredAvailable = RingDiff.MeanChromaB();
						const double OwnedToOwned = ChromaDistance(MeasuredOwned, OwnedChroma);
						const double OwnedToAvailable = ChromaDistance(MeasuredOwned, AvailableChroma);
						const double AvailableToAvailable = ChromaDistance(MeasuredAvailable, AvailableChroma);
						const double AvailableToOwned = ChromaDistance(MeasuredAvailable, OwnedChroma);

						RingJson->SetNumberField(TEXT("owned_chroma_r"), MeasuredOwned.X);
						RingJson->SetNumberField(TEXT("owned_chroma_g"), MeasuredOwned.Y);
						RingJson->SetNumberField(TEXT("owned_chroma_b"), MeasuredOwned.Z);
						RingJson->SetNumberField(TEXT("available_chroma_r"), MeasuredAvailable.X);
						RingJson->SetNumberField(TEXT("available_chroma_g"), MeasuredAvailable.Y);
						RingJson->SetNumberField(TEXT("available_chroma_b"), MeasuredAvailable.Z);
						RingJson->SetNumberField(TEXT("owned_dist"), OwnedToOwned);
						RingJson->SetNumberField(TEXT("available_dist"), AvailableToAvailable);

						// ★ ③ 진출 ↔ 진출 가능이 링에서 구별되는가. 채광 5개국은 산업색이 브라스 하나뿐이라
						//    두 상태가 같은 색으로 떨어지면 플레이어는 어디에 진출할 수 있는지 알 수 없다.
						TestTrue(*FString::Printf(TEXT("%s 진출 링이 진출가능 링보다 진출 기대색에 가깝다 (%.3f < %.3f)"),
							*Label, OwnedToOwned, OwnedToAvailable), OwnedToOwned < OwnedToAvailable);
						TestTrue(*FString::Printf(TEXT("%s 진출가능 링이 진출 링보다 중립 기대색에 가깝다 (%.3f < %.3f)"),
							*Label, AvailableToAvailable, AvailableToOwned), AvailableToAvailable < AvailableToOwned);

						// §4.3 기대색과의 절대 대조 ― 링 브러시가 제 색을 섞으면 여기서 갈린다
						TestTrue(*FString::Printf(TEXT("%s 링 픽셀이 §4.3 진출 기대색 #C9A567 (색상비 오차 %.3f ≤ %.2f)"),
							*Label, OwnedToOwned, ChromaTolerance), OwnedToOwned <= ChromaTolerance);
						TestTrue(*FString::Printf(TEXT("%s 링 픽셀이 §4.3 진출가능 기대색 #7F8CA3 (색상비 오차 %.3f ≤ %.2f)"),
							*Label, AvailableToAvailable, ChromaTolerance), AvailableToAvailable <= ChromaTolerance);
					}
					RingRecords.Add(MakeShared<FJsonValueObject>(RingJson));
				}
			}
			WidthJson->SetArrayField(TEXT("rings"), RingRecords);

			// ── ⑥ 항로 ───────────────────────────────────────────────────
			// GetRoutes().Num() > 0 인 **픽스처가 필수**다. 없으면 "선 0개" 를 정상으로 넘긴다(그게 이번에 고친 결함이다).
			UTradeRouteWidget* RouteWidget = Cast<UTradeRouteWidget>(Panel->GetWidgetFromName(TEXT("RouteLayer")));
			TestNotNull(TEXT("RouteLayer 가 UTradeRouteWidget 이다"), RouteWidget);
			if (RouteWidget != nullptr && Countries.Num() >= 2)
			{
				auto MakeFixtureRoute = [&Countries](ECountryType PreferredFrom, ECountryType PreferredTo,
					int32 FallbackFrom, int32 FallbackTo, ECompanyType Industry)
				{
					FTradeRoute Route;
					Route.From = Countries.Contains(PreferredFrom) ? PreferredFrom : Countries[FallbackFrom];
					Route.To = Countries.Contains(PreferredTo) ? PreferredTo : Countries[FallbackTo];
					Route.Industry = Industry;
					Route.bValid = (Route.From != Route.To);
					return Route;
				};

				const FTradeRoute ActiveFixture = MakeFixtureRoute(ECountryType::Korea, ECountryType::USA,
					0, Countries.Num() - 1, ECompanyType::Semiconductor);
				const FTradeRoute IdleFixture = MakeFixtureRoute(ECountryType::Japan, ECountryType::Germany,
					1, Countries.Num() - 2, ECompanyType::Electronics);

				const FLayoutBox RouteBox = MeasureNamed(*Panel, TEXT("RouteLayer"), Origin);

				// 픽스처 없이 "선 0개" 를 보면 정상으로 읽힌다 ― 픽스처가 실제로 실렸는지 먼저 못 박는다
				// 생산 항로의 흐르는 점이 점선 픽셀 단언을 혼자 채우지 않게 함께 비운다.
				RouteWidget->SetProductionRoutes({});
				RouteWidget->SetRoutes({});
				RouteWidget->SetIdleRoutes({});
				FImage RouteNoneFrame;
				const bool bNoneOk = DrawToImage(PanelSlate, CaptureSize, RouteNoneFrame)
					&& SavePNG(RouteNoneFrame, Directory / FString::Printf(TEXT("worldmap_%d_route_none.png"), CaptureWidth));
				TestTrue(TEXT("항로 없음 기준 프레임 저장"), bNoneOk);

				RouteWidget->SetRoutes({ ActiveFixture });
				RouteWidget->SetIdleRoutes({});
				TestTrue(FString::Printf(TEXT("활성 항로 픽스처가 실렸다 (선 %d개 > 0)"), RouteWidget->GetRoutes().Num()),
					RouteWidget->GetRoutes().Num() > 0);
				// 끝점이 배치된 국가여야 선이 메달리온에 꽂힌다
				TestTrue(FString::Printf(TEXT("활성 항로 출발 %s 가 판에 배치된 국가"), *CountryLabel(ActiveFixture.From)),
					Countries.Contains(ActiveFixture.From));
				TestTrue(FString::Printf(TEXT("활성 항로 도착 %s 가 판에 배치된 국가"), *CountryLabel(ActiveFixture.To)),
					Countries.Contains(ActiveFixture.To));

				FImage RouteActiveFrame;
				const bool bActiveOk = DrawToImage(PanelSlate, CaptureSize, RouteActiveFrame)
					&& SavePNG(RouteActiveFrame, Directory / FString::Printf(TEXT("worldmap_%d_route_active.png"), CaptureWidth));
				TestTrue(TEXT("활성 항로 캡처 저장 (worldmap_2560_route_active.png)"), bActiveOk);

				RouteWidget->SetRoutes({});
				RouteWidget->SetIdleRoutes({ IdleFixture });
				TestTrue(FString::Printf(TEXT("점선 항로 픽스처가 실렸다 (선 %d개 > 0)"), RouteWidget->GetIdleRoutes().Num()),
					RouteWidget->GetIdleRoutes().Num() > 0);
				FImage RouteIdleFrame;
				const bool bIdleOk = DrawToImage(PanelSlate, CaptureSize, RouteIdleFrame)
					&& SavePNG(RouteIdleFrame, Directory / FString::Printf(TEXT("worldmap_%d_route_idle.png"), CaptureWidth));
				TestTrue(TEXT("점선 항로 캡처 저장 (worldmap_2560_route_idle.png)"), bIdleOk);

				if (bNoneOk && bActiveOk && bIdleOk && RouteBox.HasArea())
				{
					// 끝점 기대 위치 = 앵커 x 판 크기. ResolveEndpoints 가 min(46, 현길이x0.3) 만큼 안쪽에서 시작한다
					TArray<FVector2D> Probes;
					TArray<double> ProbeInsets;
					auto AddProbe = [&](ECountryType Country, ECountryType Partner)
					{
						bool bRowOk = false;
						const FCountryInfoTable Row = TableManager->GetCountryInfo(Country, bRowOk);
						bool bPartnerOk = false;
						const FCountryInfoTable PartnerRow = TableManager->GetCountryInfo(Partner, bPartnerOk);
						if (!bRowOk || !bPartnerOk)
						{
							return;
						}
						const FVector2D Point(RouteBox.Left + Row.MapAnchor.X * RouteBox.Width,
							RouteBox.Top + Row.MapAnchor.Y * RouteBox.Height);
						const FVector2D PartnerPoint(RouteBox.Left + PartnerRow.MapAnchor.X * RouteBox.Width,
							RouteBox.Top + PartnerRow.MapAnchor.Y * RouteBox.Height);
						Probes.Add(Point);
						ProbeInsets.Add(FMath::Min(46.0, FVector2D::Distance(Point, PartnerPoint) * 0.3) + EndpointRadiusMargin);
					};
					AddProbe(ActiveFixture.From, ActiveFixture.To);
					AddProbe(ActiveFixture.To, ActiveFixture.From);

					const FDiffStats ActiveDiff = DiffWithin(RouteActiveFrame, RouteNoneFrame, RouteBox,
						ActiveRouteInk(), RouteChromaTolerance, Probes);
					const FDiffStats IdleDiff = DiffWithin(RouteIdleFrame, RouteNoneFrame, RouteBox,
						ActiveRouteInk(), RouteChromaTolerance, TArray<FVector2D>());
					const double BoardArea = RouteBox.Width * RouteBox.Height;

					TSharedPtr<FJsonObject> RouteJson = MakeShared<FJsonObject>();
					RouteJson->SetStringField(TEXT("active_from"), CountryLabel(ActiveFixture.From));
					RouteJson->SetStringField(TEXT("active_to"), CountryLabel(ActiveFixture.To));
					RouteJson->SetStringField(TEXT("idle_from"), CountryLabel(IdleFixture.From));
					RouteJson->SetStringField(TEXT("idle_to"), CountryLabel(IdleFixture.To));
					RouteJson->SetNumberField(TEXT("active_pixels"), ActiveDiff.Count);
					RouteJson->SetNumberField(TEXT("active_blue_pixels"), ActiveDiff.InkMatchCount);
					RouteJson->SetNumberField(TEXT("active_bbox_fill"), ActiveDiff.BBoxArea() > 0.0 ? ActiveDiff.Count / ActiveDiff.BBoxArea() : 0.0);
					RouteJson->SetNumberField(TEXT("active_max_luma"), ActiveDiff.MaxLumaA);
					RouteJson->SetNumberField(TEXT("idle_pixels"), IdleDiff.Count);
					RouteJson->SetNumberField(TEXT("idle_max_luma"), IdleDiff.MaxLumaA);

					// 선이 실제로 그려진다 ― NativePaint 가 죽거나 판 사각형을 못 받으면 여기서 0 이다
					TestTrue(FString::Printf(TEXT("활성 항로가 실제로 그려진다 (켜고 끌 때 달라지는 픽셀 %d ≥ %d)"),
						ActiveDiff.Count, MinRoutePixels), ActiveDiff.Count >= MinRoutePixels);
					TestTrue(FString::Printf(TEXT("점선 항로가 실제로 그려진다 (달라지는 픽셀 %d ≥ %d)"),
						IdleDiff.Count, MinRoutePixels), IdleDiff.Count >= MinRoutePixels);
					// 활성선은 카탈로그 기능색 블루다
					TestTrue(FString::Printf(TEXT("활성 항로 픽셀에 #3D9BE0 가 %d개 (≥ %d)"),
						ActiveDiff.InkMatchCount, MinRoutePixels), ActiveDiff.InkMatchCount >= MinRoutePixels);

					// 끝점이 메달리온에 꽂힌다 ― 비례해서 어긋나면 여기서 거리로 드러난다
					for (int32 ProbeIndex = 0; ProbeIndex < Probes.Num(); ++ProbeIndex)
					{
						const double Measured = ActiveDiff.ProbeMinDistance.IsValidIndex(ProbeIndex)
							? ActiveDiff.ProbeMinDistance[ProbeIndex] : TNumericLimits<double>::Max();
						TestTrue(FString::Printf(TEXT("활성 항로 끝점 %d 이 국가 앵커 근처에서 시작한다 (최근접 %.1fpx ≤ %.1f)"),
							ProbeIndex, Measured, ProbeInsets[ProbeIndex]), Measured <= ProbeInsets[ProbeIndex]);
					}

					// 0 길이 세그먼트가 만든 스트립 분할은 폴리라인이 아니라 **면**으로 그려진다 ―
					// 선은 제 bbox 를 거의 못 채우고 판의 한 자릿수 %만 먹는다.
					const double ActiveFill = ActiveDiff.BBoxArea() > 0.0 ? ActiveDiff.Count / ActiveDiff.BBoxArea() : 0.0;
					TestTrue(FString::Printf(TEXT("활성 항로가 면이 아니라 선이다 (bbox 채움 %.3f ≤ %.2f)"),
						ActiveFill, MaxRouteBBoxFill), ActiveFill <= MaxRouteBBoxFill);
					TestTrue(FString::Printf(TEXT("활성 항로가 판을 덮지 않는다 (판 대비 %.3f ≤ %.2f)"),
						BoardArea > 0.0 ? ActiveDiff.Count / BoardArea : 0.0, MaxRouteBoardShare),
						BoardArea > 0.0 && (ActiveDiff.Count / BoardArea) <= MaxRouteBoardShare);
					// 밝은 얼룩 가드 ― 불투명 대시가 휘도 140, 브라스 위 글로우가 ~190 이다
					TestTrue(FString::Printf(TEXT("활성 항로에 밝은 얼룩이 없다 (최대 휘도 %.0f ≤ %.0f)"),
						ActiveDiff.MaxLumaA, MaxRouteLuma), ActiveDiff.MaxLumaA <= MaxRouteLuma);
					TestTrue(FString::Printf(TEXT("점선 항로에 밝은 얼룩이 없다 (최대 휘도 %.0f ≤ %.0f)"),
						IdleDiff.MaxLumaA, MaxRouteLuma), IdleDiff.MaxLumaA <= MaxRouteLuma);

					WidthJson->SetObjectField(TEXT("routes"), RouteJson);
				}

				RouteWidget->SetRoutes({});
				RouteWidget->SetIdleRoutes({});
			}
		}

		Report->SetObjectField(WidthLabel, WidthJson);
	}

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Serialized, *(Directory / TEXT("worldmap_table_render.json")));

	UE_LOG(LogTemp, Display, TEXT("[Task9] 월드맵 렌더 증거 = %s"), *(Directory / TEXT("worldmap_table_render.json")));
	return true;
}

namespace WorldMapTableRender
{
	class FCountryClickTransitionCommand : public IAutomationLatentCommand
	{
	public:
		FCountryClickTransitionCommand(FAutomationTestBase* InTest, UWorldMapPanelWidget* InPanel)
			: Test(InTest), Panel(InPanel), StartedAt(FPlatformTime::Seconds())
		{
			PanelSlate = Panel->TakeWidget();
			Stack = Cast<UCommonActivatableWidgetStack>(Panel->GetWidgetFromName(TEXT("DrawerStack")));
		}

		virtual ~FCountryClickTransitionCommand() override
		{
			Panel->ResetFocus(false);
			if (Stack) { Stack->SetTransitionDuration(0.0f); Stack->ClearWidgets(); }
		}

		virtual bool Update() override
		{
			if (!Stack || FPlatformTime::Seconds() - StartedAt > 10.0)
			{
				Test->AddError(TEXT("국가 연속 클릭 전환 검증이 스택 없음 또는 10초 제한으로 끝났습니다."));
				return true;
			}
			FImage Frame;
			if (!DrawToImage(PanelSlate.ToSharedRef(), FIntPoint(2560, CaptureHeight), Frame))
			{
				Test->AddError(TEXT("국가 클릭 전환 검증을 렌더하지 못했습니다."));
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			UCountryDetailWidget* Detail = Cast<UCountryDetailWidget>(Stack->GetActiveWidget());
			if (Stage == 0)
			{
				Stack->SetTransitionDuration(0.4f);
				Test->TestTrue(TEXT("전환 중 최초 일본 클릭"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Japan)));
				FirstDetail = Cast<UCountryDetailWidget>(Stack->GetWidgetList().Num() ? Stack->GetWidgetList().Last() : nullptr);
				Test->TestTrue(TEXT("첫 열림 전환 중 캐나다 클릭"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Canada)));
				Test->TestTrue(TEXT("첫 열림 전환 중 같은 국가 재클릭"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Canada)));
				Test->TestEqual(TEXT("운영 전환시간에서도 최초 연타 스택 하나"), Stack->GetNumWidgets(), 1);
				Test->TestTrue(TEXT("최초 연타가 같은 상세 인스턴스를 쓴다"), Stack->GetWidgetList().Num() == 1 && Stack->GetWidgetList().Last() == FirstDetail);
				Stage = 1;
				StageStartedAt = Now;
			}
			else if (Stage == 1 && Now - StageStartedAt > 0.6)
			{
				if (!Test->TestNotNull(TEXT("연타 뒤 활성 상세"), Detail)) { return true; }
				Test->TestEqual(TEXT("연타의 마지막 국가가 표시된다"), Detail->GetCurrentCountry(), ECountryType::Canada);
				const bool bReducedBeforeReclick = FMotionGate::IsReduced();
				FMotionGate::SetReduced(false);
				Test->TestTrue(TEXT("레이아웃 정착 후 독일로 포커스 이동"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Germany)));
				Test->TestTrue(TEXT("포커스 진행 검증의 첫 0.1초"), DrawToImage(PanelSlate.ToSharedRef(), FIntPoint(2560, CaptureHeight), Frame, 0.1f));
				Test->TestTrue(TEXT("재클릭 시점에는 지도 포커스가 진행 중"), Panel->IsFocusAnimating());
				Test->TestTrue(TEXT("포커스 이동 중 동일 독일 재클릭"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Germany)));
				Test->TestTrue(TEXT("포커스 진행 검증의 남은 0.15초"), DrawToImage(PanelSlate.ToSharedRef(), FIntPoint(2560, CaptureHeight), Frame, 0.15f));
				Test->TestFalse(TEXT("동일 국가 재클릭 후에도 최초 0.25초 안에 포커스가 끝난다"), Panel->IsFocusAnimating());
				FMotionGate::SetReduced(bReducedBeforeReclick);
				Test->TestTrue(TEXT("닫기 실제 버튼 경로"), ClickDetailClose(Detail));
				Test->TestFalse(TEXT("닫기 전환 시작 시 상세는 비활성"), Detail->IsActivated());
				Test->TestTrue(TEXT("닫기 전환 중 일본 클릭"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Japan)));
				Test->TestTrue(TEXT("닫기 전환 중 마지막 호주 클릭"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Australia)));
				Test->TestEqual(TEXT("닫기 전환 중에도 중복 상세를 추가하지 않는다"), Stack->GetNumWidgets(), 1);
				Stage = 2;
				StageStartedAt = Now;
			}
			else if (Stage == 2 && Now - StageStartedAt > 0.8 && Detail && Detail->IsActivated()
				&& Detail->GetCurrentCountry() == ECountryType::Australia)
			{
				Test->TestEqual(TEXT("닫기 후 최신 국가만 한 번 다시 열린다"), Stack->GetNumWidgets(), 1);
				Test->TestEqual(TEXT("재오픈 후 마지막 국가만 선택된다"), Panel->GetSelectedCountry(), ECountryType::Australia);
				int32 SelectedCount = 0;
				for (const auto& Entry : Panel->GetMedallions()) { SelectedCount += Entry.Value && Entry.Value->GetSelected() ? 1 : 0; }
				Test->TestEqual(TEXT("선택 핀은 정확히 하나"), SelectedCount, 1);
				Test->TestTrue(TEXT("재오픈한 상세도 닫기 한 번"), ClickDetailClose(Detail));
				Stage = 3;
				StageStartedAt = Now;
			}
			else if (Stage == 3 && Now - StageStartedAt > 0.8 && Stack->GetNumWidgets() == 0)
			{
				Test->TestEqual(TEXT("닫기 완결 후 원래 배율"), Panel->GetAppliedFocusScale(), 1.0f, 1e-3f);
				Test->TestTrue(TEXT("닫기 완결 후 원래 위치"), Panel->GetAppliedFocusTranslation().IsNearlyZero(0.01));
				Test->TestEqual(TEXT("닫기 완결 후 선택 없음"), Panel->GetSelectedCountry(), ECountryType::None);
				Test->TestEqual(TEXT("닫기 완결 후 드로어 셸도 숨김"), Panel->GetWidgetFromName(TEXT("DrawerHost"))->GetVisibility(), ESlateVisibility::Collapsed);
				Test->TestTrue(TEXT("취소 시나리오 국가 재오픈"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Canada)));
				UCountryDetailWidget* OpeningDetail = Cast<UCountryDetailWidget>(Stack->GetActiveWidget());
				Test->TestTrue(TEXT("열림 전환 도중 즉시 닫기"), ClickDetailClose(OpeningDetail));
				Test->TestTrue(TEXT("열림 중 닫힌 상세에 재클릭 예약"), ClickCountryPin(Panel->GetMedallions().FindRef(ECountryType::Japan)));
				Test->TestTrue(TEXT("뒤로가기로 예약 취소"), Panel->NativeOnHandleBackAction());
				Stage = 4;
				StageStartedAt = Now;
			}
			else if (Stage == 4 && Now - StageStartedAt > 0.8 && Stack->GetNumWidgets() == 0)
			{
				Test->TestEqual(TEXT("취소한 예약이 상세를 다시 열지 않는다"), Stack->GetNumWidgets(), 0);
				Test->TestEqual(TEXT("예약 취소 뒤 배율 복원"), Panel->GetAppliedFocusScale(), 1.0f, 1e-3f);
				Test->TestTrue(TEXT("예약 취소 뒤 위치 복원"), Panel->GetAppliedFocusTranslation().IsNearlyZero(0.01));
				Test->TestEqual(TEXT("예약 취소 뒤 선택 해제"), Panel->GetSelectedCountry(), ECountryType::None);
				return true;
			}
			return false;
		}

	private:
		FAutomationTestBase* Test;
		TStrongObjectPtr<UWorldMapPanelWidget> Panel;
		TSharedPtr<SWidget> PanelSlate;
		UCommonActivatableWidgetStack* Stack = nullptr;
		UCountryDetailWidget* FirstDetail = nullptr;
		int32 Stage = 0;
		double StartedAt = 0.0;
		double StageStartedAt = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapCountryClickLifecycleTest,
	"CGR.WorldMap.CountryClickLifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapCountryClickLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* PlayWorld = WorldMapTableRender::FindPlayWorld();
	UGameInstance* GameInstance = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableManager = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!FApp::CanEverRender() || !TableManager)
	{
		AddError(TEXT("국가 클릭 전환 검증에는 렌더 가능한 플레이 월드가 필요합니다."));
		return false;
	}
	UWorldMapPanelWidget* Panel = CreateWidget<UWorldMapPanelWidget>(GameInstance, TableManager->GetWidgetClass(EWidgetType::WorldMapPanel));
	if (!TestNotNull(TEXT("국가 클릭 전환용 독립 패널"), Panel) || !WorldMapTableRender::DisableAutoActivate(Panel)) { return false; }
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<WorldMapTableRender::FCountryClickTransitionCommand>(this, Panel));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// ═════════════════════════════════════════════════════════════════════════════
// Task 10 — 진입점 왕복. 위의 값·렌더 단언은 전부 **패널을 직접 만들어** 본 것이라
// 「버튼 → push → 닫기 → 입력모드 복귀」라는 실제 경로는 아무도 안 본다. 여기서 보는 건 조립이다:
// 해금 게이트 → PushPromptClass → GoToUIMode → 중복 push 가드 → 닫기 2종 → GoToNormalMode.
//
// ⚠ 라이브 플레이 월드가 필요하다(-game 스탠드얼론 또는 PIE). 전제(UIBase·TableMgr·PC·DT 행)가
//    하나라도 없으면 이 테스트의 단언은 **조용히 전부 통과**한다 ― 스택 0 → 0 은 "안 늘었다"로도
//    읽히기 때문이다. 그래서 전제를 명명된 체크로 먼저 확인하고, 하나라도 없으면 AddError 후
//    즉시 return false 한다. "늘었다"는 상대값(Before+1)과 절대값(After > 0) 둘 다로 못 박는다.
// ═════════════════════════════════════════════════════════════════════════════
#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonActivatableWidget.h"
#include "Core/CGGameInstance.h"
#include "Data/WorldMapTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Enum/CompanyType.h"
#include "Enum/WidgetType.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "Player/LegacyWorldMapPlayerController.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Panel/BuildOpenWidget.h"
#include "UI/UIBase.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 형제 파일의 익명 ns 와 재정의 충돌
namespace WorldMapEntryRoundTrip
{
	static const TCHAR* InputModeLabel(EInputMode Mode)
	{
		switch (Mode)
		{
		case EInputMode::Normal:        return TEXT("Normal");
		case EInputMode::BuildPlace:    return TEXT("BuildPlace");
		case EInputMode::BuildingClick: return TEXT("BuildingClick");
		case EInputMode::Factory:       return TEXT("Factory");
		default:                        return TEXT("UI");
		}
	}

	static UWorld* FindEntryPlayWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		// PIE 우선 2패스 ― 둘 다 떠 있을 때 어느 쪽을 잡을지 확정한다(TableRender 와 같은 규칙)
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Candidate = Context.World();
				if (Context.WorldType == Wanted && Candidate != nullptr && Candidate->GetGameInstance() != nullptr)
				{
					return Candidate;
				}
			}
		}
		return nullptr;
	}

	// 버튼 핸들러는 protected 라 테스트에서 C++ 로 못 부른다 ― UFUNCTION 이므로 리플렉션으로 그대로 탄다.
	static bool CallNoArgHandler(UObject* Target, const TCHAR* FunctionName)
	{
		if (Target == nullptr)
		{
			return false;
		}
		UFunction* Handler = Target->FindFunction(FName(FunctionName));
		if (Handler == nullptr || Handler->NumParms != 0)
		{
			return false;
		}
		Target->ProcessEvent(Handler, nullptr);
		return true;
	}

	// 라이브 인스턴스 우선 ― 실제로 화면에 붙어 있는 도크의 버튼 주인이 진짜 진입점이다.
	static UBuildOpenWidget* FindLiveEntryWidget(UWorld* PlayWorld)
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayWorld, FoundWidgets, UBuildOpenWidget::StaticClass(), false);

		UBuildOpenWidget* Fallback = nullptr;
		for (UUserWidget* Candidate : FoundWidgets)
		{
			UBuildOpenWidget* Entry = Cast<UBuildOpenWidget>(Candidate);
			if (Entry == nullptr)
			{
				continue;
			}
			if (Entry->IsActivated())
			{
				return Entry;
			}
			if (Fallback == nullptr)
			{
				Fallback = Entry;
			}
		}
		return Fallback;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapEntryRoundTripTest,
	"CGR.WorldMap.EntryRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapEntryRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapEntryRoundTrip;

	// ── 전제 ① 플레이 월드 ────────────────────────────────────────────────────
	UWorld* PlayWorld = FindEntryPlayWorld();
	UGameInstance* GameInstance = PlayWorld != nullptr ? PlayWorld->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		AddError(TEXT("[Task10] 플레이 월드가 없습니다 ― -game 스탠드얼론으로 띄우거나 PIE 를 시작한 뒤 실행하세요. ")
			TEXT("월드 없이 돌면 스택이 항상 0 이라 '안 늘었다'와 '늘 수 없었다'를 구별할 수 없습니다."));
		return false;
	}

	// ── 전제 ② UIBase ─────────────────────────────────────────────────────────
	UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager != nullptr ? UIManager->GetUIBase() : nullptr;
	if (UIBase == nullptr)
	{
		AddError(TEXT("[Task10] UIBase 가 없습니다 ― PushPromptClass 가 갈 곳이 없어 모든 스택 단언이 0 → 0 으로 조용히 통과합니다."));
		return false;
	}

	// ── 전제 ③ TableManager + DT_WidgetClass 행 ──────────────────────────────
	UTableManagerSubsystem* TableMgr = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (TableMgr == nullptr)
	{
		AddError(TEXT("[Task10] TableManagerSubsystem 을 못 읽었습니다 ― 진입점이 위젯 클래스를 못 구해 조용히 return 합니다."));
		return false;
	}
	const TSubclassOf<UUserWidget> PanelClass = TableMgr->GetWidgetClass(EWidgetType::WorldMapPanel);
	if (!PanelClass)
	{
		AddError(TEXT("[Task10] DT_WidgetClass 에 WorldMapPanel 행이 없습니다 ― 진입점이 조용히 아무것도 push 하지 않습니다."));
		return false;
	}
	if (!PanelClass->IsChildOf(UWorldMapPanelWidget::StaticClass()))
	{
		AddError(FString::Printf(TEXT("[Task10] DT_WidgetClass 의 WorldMapPanel 행이 월드맵 패널이 아닙니다(%s). ")
			TEXT("push 는 되고 클래스 단언만 깨지는 형태라 원인을 여기서 먼저 못 박습니다."), *PanelClass->GetName()));
		return false;
	}

	// ── 전제 ④ MainMap 플레이어 컨트롤러 ─────────────────────────────────────
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(PlayWorld->GetFirstPlayerController());
	if (PC == nullptr)
	{
		AddError(TEXT("[Task10] AMainMapPlayerController 가 없습니다 ― 입력 모드 쌍(UI ↔ Normal)을 잴 수 없어 ")
			TEXT("그 단언들이 전부 공허해집니다. MainMap 에서 실행하세요."));
		return false;
	}
	// ALegacyWorldMapPlayerController 는 AMainMapPlayerController 를 상속한다 ― 구 월드맵 레벨에서 돌면
	// 도크 홈 버튼이 ReturnToMainMap() 으로 빠져 테스트 도중 레벨 전환이 난다.
	if (Cast<ALegacyWorldMapPlayerController>(PC) != nullptr)
	{
		AddError(TEXT("[Task10] 구 월드맵 레벨(ALegacyWorldMapPlayerController)에서 실행 중입니다 ― ")
			TEXT("홈 버튼이 레벨 전환으로 빠지므로 판정할 수 없습니다. MainMap 에서 실행하세요."));
		return false;
	}
	// 열기는 GetFirstPlayerController, 닫기(패널)는 CGGameInstance::GetCurrentPlayerController 로 컨트롤러를 찾는다.
	// 둘이 갈리면 UI 로 올린 컨트롤러와 Normal 로 내리는 컨트롤러가 달라져 모드가 영구히 UI 로 남는다.
	UCGGameInstance* CGGameInstance = Cast<UCGGameInstance>(GameInstance);
	TestNotNull(TEXT("게임 인스턴스가 UCGGameInstance 다 ― 아니면 패널이 닫힐 때 컨트롤러를 못 찾아 입력 모드를 안 되돌린다"),
		CGGameInstance);
	TestTrue(TEXT("입력 모드 쌍이 같은 컨트롤러를 본다 (열기=GetFirstPlayerController · 닫기=GetCurrentPlayerController)"),
		CGGameInstance != nullptr && CGGameInstance->GetCurrentPlayerController() == PC);

	// ── 전제 ⑤ 스택이 비어 있다 ──────────────────────────────────────────────
	// 진입점은 `GetPromptStackCount() > 0` 이면 먼저 return 한다 ― 다른 패널이 열려 있으면
	// push 가 아예 시도되지 않아 이 테스트 전체가 "코드가 깨졌다"로 잘못 보고된다.
	const int32 BeforeStack = UIBase->GetPromptStackCount();
	if (BeforeStack != 0)
	{
		AddError(FString::Printf(TEXT("[Task10] 프롬프트 스택에 이미 %d 개가 열려 있습니다 ― 월드맵 진입 가드가 먼저 막아 ")
			TEXT("push 를 시도조차 하지 않습니다. 열린 패널을 모두 닫고 다시 실행하세요."), BeforeStack));
		return false;
	}

	// ── 전제 ⑥ 시작 입력 모드가 UI 가 아니다 ─────────────────────────────────
	// 이미 UI 면 "push 가 UI 로 바꿨다"를 아무것도 안 해도 통과시킬 수 있다(0 자기검증과 같은 함정).
	const EInputMode BeforeMode = PC->GetCurrentInputMode();
	if (BeforeMode == EInputMode::UI)
	{
		AddError(TEXT("[Task10] 시작부터 입력 모드가 UI 입니다 ― 'push 가 UI 로 올렸다'를 '원래 UI 였다'와 구별할 수 없습니다. ")
			TEXT("열린 UI 를 닫고(Normal 상태에서) 다시 실행하세요."));
		return false;
	}
	if (BeforeMode != EInputMode::Normal)
	{
		AddInfo(FString::Printf(TEXT("[Task10] 시작 입력 모드가 %s 입니다 ― 왕복 후 복귀 목적지는 코드 규약대로 Normal 입니다."),
			InputModeLabel(BeforeMode)));
	}

	// ── 전제 ⑦ 스택 전이 애니를 0 으로 고정 ──────────────────────────────────
	// SCommonAnimatedSwitcher 는 기본 0.4s 전이가 끝난 뒤에야 WidgetList 에서 위젯을 빼낸다.
	// 자동화 테스트는 한 프레임 안에서 끝나므로 그대로 두면 "닫았는데 스택이 안 줄었다"가 거짓으로 뜬다.
	UCommonActivatableWidgetStack* PromptStack = nullptr;
	if (FObjectProperty* StackProperty = FindFProperty<FObjectProperty>(UUIBase::StaticClass(), TEXT("PromptStack")))
	{
		PromptStack = Cast<UCommonActivatableWidgetStack>(StackProperty->GetObjectPropertyValue_InContainer(UIBase));
	}
	if (PromptStack == nullptr)
	{
		PromptStack = Cast<UCommonActivatableWidgetStack>(UIBase->GetWidgetFromName(TEXT("PromptStack")));
	}
	if (PromptStack == nullptr)
	{
		AddError(TEXT("[Task10] UIBase 의 PromptStack 을 못 잡았습니다 ― 전이 애니를 0 으로 고정하지 못하면 ")
			TEXT("닫기 단언이 '아직 애니 중'과 '안 닫혔다'를 구별할 수 없습니다."));
		return false;
	}
	const float OriginalTransition = PromptStack->GetTransitionDuration();

	// ── 전제 ⑧ 진입점 위젯 ───────────────────────────────────────────────────
	UBuildOpenWidget* Entry = FindLiveEntryWidget(PlayWorld);
	TStrongObjectPtr<UBuildOpenWidget> CreatedEntry;
	if (Entry == nullptr)
	{
		// 라이브 도크가 없으면(다른 화면 등) DT 에서 새로 만들어 같은 핸들러를 탄다 ― 조용히 건너뛰지 않는다.
		const TSubclassOf<UUserWidget> EntryClass = TableMgr->GetWidgetClass(EWidgetType::BuildOpen);
		if (EntryClass && EntryClass->IsChildOf(UBuildOpenWidget::StaticClass()))
		{
			CreatedEntry.Reset(CreateWidget<UBuildOpenWidget>(GameInstance, EntryClass));
			Entry = CreatedEntry.Get();
		}
		AddInfo(TEXT("[Task10] 라이브 BuildOpen 위젯이 없어 DT_WidgetClass 로 새 인스턴스를 만들어 호출합니다."));
	}
	if (Entry == nullptr)
	{
		AddError(TEXT("[Task10] BuildOpen 진입점 위젯을 못 구했습니다(라이브도 없고 DT_WidgetClass 의 BuildOpen 행으로도 못 만듦) ― ")
			TEXT("버튼 핸들러를 부를 대상이 없습니다."));
		return false;
	}

	// ── 전제 ⑨ 해금 게이트 픽스처 ────────────────────────────────────────────
	// OnWorldMapButtonClicked 는 IsWorldMapUnlocked() 를 먼저 탄다. 잠겨 있으면 토스트만 뜨고 push 가 없다.
	// 판정 소스는 위젯의 CachedSaveMgr 이므로 그 한 칸만 픽스처로 고정한다(세이브는 건드리지 않는다).
	FObjectProperty* CachedSaveProperty = FindFProperty<FObjectProperty>(UBuildOpenWidget::StaticClass(), TEXT("CachedSaveMgr"));
	if (CachedSaveProperty == nullptr)
	{
		AddError(TEXT("[Task10] UBuildOpenWidget::CachedSaveMgr 프로퍼티를 못 찾았습니다 ― 해금 상태를 픽스처로 만들 수 없습니다. ")
			TEXT("잠긴 채로 돌리면 push 가 전혀 안 일어나므로 조용히 건너뛰지 않고 여기서 실패시킵니다."));
		return false;
	}
	USaveLoadManager* SaveMgr = GameInstance->GetSubsystem<USaveLoadManager>();
	const bool bLiveUnlocked = SaveMgr != nullptr
		&& SaveMgr->GetHQLevel() >= WorldMapConstants::WorldMapRequiredHQLevel;
	UObject* const OriginalCachedSave = CachedSaveProperty->GetObjectPropertyValue_InContainer(Entry);
	// 해금이면 진짜 매니저를 그대로 꽂아 실경로를 타고, 아직 잠겼으면 "매니저가 없으면 열어 둔다"는
	// IsWorldMapUnlocked 의 명시적 폴백 경로로 게이트를 연다(잠금 규칙 자체는 이 테스트의 대상이 아니다).
	CachedSaveProperty->SetObjectPropertyValue_InContainer(Entry, bLiveUnlocked ? SaveMgr : nullptr);
	AddInfo(bLiveUnlocked
		? TEXT("[Task10] 해금 픽스처 = 실제 해금 상태(월드맵 HQ 레벨 충족). 게이트를 실경로로 통과합니다.")
		: TEXT("[Task10] 해금 픽스처 = CachedSaveMgr 비움. 세이브가 아직 잠겨 있어 IsWorldMapUnlocked 의 폴백(매니저 없으면 개방)으로 게이트를 엽니다."));

	// 라이브 세션을 건드리는 테스트다 ― 어디서 빠져나가도 판을 열어 둔 채 떠나지 않는다.
	ON_SCOPE_EXIT
	{
		if (UIBase->GetPromptStackCount() > BeforeStack)
		{
			if (UCommonActivatableWidget* Top = UIBase->GetActivePromptWidget())
			{
				Top->DeactivateWidget();
			}
		}
		PromptStack->SetTransitionDuration(OriginalTransition);
		CachedSaveProperty->SetObjectPropertyValue_InContainer(Entry, OriginalCachedSave);
	};

	PromptStack->SetTransitionDuration(0.0f);

	// ── ① 진입점이 패널을 push 한다 ──────────────────────────────────────────
	if (!CallNoArgHandler(Entry, TEXT("OnWorldMapButtonClicked")))
	{
		AddError(TEXT("[Task10] OnWorldMapButtonClicked UFUNCTION 을 못 찾았습니다 ― 이름이 바뀌었거나 UFUNCTION 이 빠졌습니다."));
		return false;
	}
	const int32 AfterPush = UIBase->GetPromptStackCount();
	TestEqual(TEXT("버튼 핸들러가 프롬프트 스택을 정확히 1 늘린다 (실패 = 해금 게이트가 막았거나 push 가 안 돈다)"),
		AfterPush, BeforeStack + 1);
	// 절대값 단언 ― 0 → 0 을 "안 늘었다"로 읽지 않게 스택이 실제로 차 있는지도 본다.
	TestTrue(TEXT("push 후 스택이 절대값으로도 비어 있지 않다"), AfterPush > 0);

	UWorldMapPanelWidget* Panel = Cast<UWorldMapPanelWidget>(UIBase->GetActivePromptWidget());
	TestNotNull(TEXT("push 된 위젯이 UWorldMapPanelWidget 이다 ― 이름이 아니라 클래스로 본다"), Panel);
	if (Panel == nullptr)
	{
		AddError(TEXT("[Task10] push 된 최상단 위젯이 월드맵 패널이 아닙니다 ― 이후 닫기·입력모드 단언을 돌리면 ")
			TEXT("엉뚱한 위젯을 검사하게 되므로 여기서 멈춥니다."));
		return false;
	}

	// ── ② 입력 모드 ― 열면 UI ────────────────────────────────────────────────
	const EInputMode PushedMode = PC->GetCurrentInputMode();
	TestTrue(FString::Printf(TEXT("push 후 입력 모드가 UI 다 (실제=%s)"), InputModeLabel(PushedMode)),
		PushedMode == EInputMode::UI);

	// ── ③ 중복 push 가드 ─────────────────────────────────────────────────────
	if (!CallNoArgHandler(Entry, TEXT("OnWorldMapButtonClicked")))
	{
		AddError(TEXT("[Task10] 중복 호출에서 OnWorldMapButtonClicked 를 못 불렀습니다."));
		return false;
	}
	TestEqual(TEXT("이미 열려 있으면 중복 push 가 막힌다 (GetPromptStackCount() > 0 가드)"),
		UIBase->GetPromptStackCount(), AfterPush);
	TestTrue(TEXT("중복 호출 뒤에도 최상단이 같은 패널 인스턴스다 ― 새 인스턴스로 갈리면 가드가 샌 것"),
		UIBase->GetActivePromptWidget() == Panel);

	// ── ④ 닫기 경로 A ― 백 액션 ──────────────────────────────────────────────
	// ④-a 전제. 이게 없으면 액션 라우터가 행 이름 None 을 받고 바인딩 자체를 만들지 않아
	//     ESC·안드로이드 뒤로가기가 NativeOnHandleBackAction 까지 **도달하지 않는다**.
	//     2026-09-26 까지 미설정이었고 그날 UCGCommonUIInputData + DT_CommonInputActions 로 배선했다.
	FString CommonInputDataPath;
	const bool bCommonInputConfigured = GConfig != nullptr
		&& GConfig->GetString(TEXT("/Script/CommonInput.CommonInputSettings"), TEXT("InputData"), CommonInputDataPath, GGameIni)
		&& !CommonInputDataPath.IsEmpty()
		&& CommonInputDataPath != TEXT("None");
	TestTrue(TEXT("CommonInput InputData 가 설정돼 있다 ― 빠지면 ESC·안드로이드 뒤로가기가 핸들러에 도달하지 못한다"),
		bCommonInputConfigured);

	// ④-b ★ ESC 를 액션 라우터에 실제로 흘린다. 핸들러 직접 호출은 라우팅을 한 칸도 덮지 않는다.
	//     ⚠ 덮는 범위: 「행 조회 → 입력모드 게이트 → 바인딩 실행 → 핸들러」.
	//        덮지 않는 범위: 「SViewport 포커스 → FSceneViewport::OnKeyDown → GameViewportClient::InputKey」 한 칸.
	//        그 한 칸은 `DefaultEngine.ini` 의 `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`
	//        로 보장되고 아래에서 설정 자체를 단언한다.
	//     ★ `FSlateApplication::ProcessKeyDownEvent` 를 쓰면 안 된다 ― 자동화 하네스에서는 뷰포트 위젯이
	//        포커스를 갖지 않아 키가 라우터까지 내려가지 않고, 빨강이 "ESC 가 깨졌다" 가 아니라
	//        "내가 키를 그 경로로 못 보냈다" 가 된다(2026-09-26 실측: 그 형태로 한 번 오판했다).
	FString ViewportClientClass;
	const bool bCommonViewportClient = GConfig != nullptr
		&& GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("GameViewportClientClassName"), ViewportClientClass, GEngineIni)
		&& ViewportClientClass.Contains(TEXT("CommonUI.CommonGameViewportClient"));
	TestTrue(TEXT("GameViewportClient 가 CommonGameViewportClient 다 ― 아니면 키가 액션 라우터에 도달하지 못한다"),
		bCommonViewportClient);

	// ★ 한 틱 펌프. 라우터는 활성화된 위젯을 `RebuiltWidgetsPendingNodeAssignment` 에 모아 두고
	//   `ProcessRebuiltWidgets()` 에서 노드를 조립하는데, 그게 `FTSTicker` 코어 티커에서 돈다
	//   (CommonUIActionRouterBase.cpp: AddTicker -> Tick -> ProcessRebuiltWidgets).
	//   테스트는 push 와 키 주입이 같은 프레임이라 펌프하지 않으면 **패널의 노드가 아직 없다** ―
	//   그러면 라우터가 BlockGameInput 을 돌려주고, 그 빨강은 제품 결함이 아니라 하네스 결함이다.
	FTSTicker::GetCoreTicker().Tick(0.016f);

	bool bClosedByRealKey = false;
	if (UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(*Panel))
	{
		const ERouteUIInputResult EscResult = Router->ProcessInput(EKeys::Escape, IE_Pressed);
		Router->ProcessInput(EKeys::Escape, IE_Released);
		bClosedByRealKey = !Panel->IsActivated();

		// 이 두 줄이 서는 데 층이 넷 필요했다(2026-09-26). 하나라도 빠지면 ESC 는 조용히 죽는다:
		//   ① CommonInput InputData 설정        ― 없으면 바인딩이 아예 안 만들어진다
		//   ② GameViewportClient = CommonUI 것  ― 없으면 키가 라우터에 도달하지 않는다
		//   ③ 패널의 GetDesiredInputConfig      ― 없으면 라우터가 게임 입력 모드에 머문다
		//   ④ UI 루트(UUIBase)의 ActivateWidget ― 없으면 ActiveRootNode 가 null 이라 바인딩을 조회조차 안 한다
		// ①②③은 위에서 따로 단언하고, ④는 이 두 줄로만 드러난다.
		TestTrue(FString::Printf(TEXT("★ESC 가 액션 라우터에서 Handled 로 처리된다 (실제=%d ― 0=Handled 1=BlockGameInput 2=Unhandled)"),
			static_cast<int32>(EscResult)), EscResult == ERouteUIInputResult::Handled);
		TestTrue(TEXT("★그 ESC 로 패널이 실제로 닫힌다 ― 라우팅을 포함하는 유일한 단언"), bClosedByRealKey);

		if (!bClosedByRealKey)
		{
			// 어디서 막혔는지 말해 준다. 엔진 `CanWidgetReceiveInput` 은 바인딩된 위젯의 **모든 슬레이트
			// 조상**에 대해 Enabled + IsVisible + AreChildrenHitTestVisible 를 요구하는데, 실패하면
			// 로그에 한 줄도 남지 않는다 ― `HitTestInvisible` 조상 하나가 조용히 전체를 막는다.
			FString Chain;
			for (TSharedPtr<SWidget> Cursor = Panel->GetCachedWidget(); Cursor.IsValid(); Cursor = Cursor->GetParentWidget())
			{
				const EVisibility Vis = Cursor->GetVisibility();
				const bool bBlocks = !Cursor->IsEnabled() || !Vis.IsVisible() || !Vis.AreChildrenHitTestVisible();
				Chain += FString::Printf(TEXT("\n    %s %s [%s]%s"),
					bBlocks ? TEXT("★막음") : TEXT("     ok"),
					*Cursor->GetType().ToString(),
					*Vis.ToString(),
					Cursor->IsEnabled() ? TEXT("") : TEXT(" (Disabled)"));
			}
			AddError(FString::Printf(
				TEXT("[Task10] ESC 가 바인딩에 도달하지 못했습니다(라우터=%d). 슬레이트 부모 사슬 ― ★막음 이 붙은 층이 원인입니다:%s"),
				static_cast<int32>(EscResult), *Chain));
		}
	}
	else
	{
		AddError(TEXT("[Task10] 액션 라우터를 못 얻어 ESC 라우팅을 검증하지 못했습니다 ― 통과가 아니라 미검증입니다."));
	}

	// 실키가 못 닫았으면 핸들러만이라도 지킨다 ― 뒤따르는 입력 모드·재오픈 단언이 닫힌 상태를 전제로 한다.
	if (!bClosedByRealKey)
	{
		TestTrue(TEXT("백 핸들러가 처리했다고 알린다 (실키 폴백 ― 이 줄이 돌면 라우팅은 미검증이다)"),
			Panel->NativeOnHandleBackAction());
	}
	TestFalse(TEXT("백 액션 뒤 패널이 비활성이다"), Panel->IsActivated());
	TestEqual(TEXT("백 액션 뒤 스택이 원래 높이로 내려온다"),
		UIBase->GetPromptStackCount(), BeforeStack);

	// ── ⑤ 입력 모드 ― 닫으면 Normal ─────────────────────────────────────────
	// ★ 이번 작업에서 실제로 났던 Critical. CommonUI 스택이 패널을 대신 deactivate 하면 GoToNormalMode() 가
	//   돌고, 복귀 때 아무도 UI 모드를 되돌리지 않아 풀스크린 판 뒤로 MainMap 터치가 통과했다.
	const EInputMode BackClosedMode = PC->GetCurrentInputMode();
	TestTrue(FString::Printf(TEXT("백 액션으로 닫으면 입력 모드가 Normal 로 돌아온다 ― ★실패 = 판 뒤로 MainMap 터치가 통과한다 (실제=%s)"),
		InputModeLabel(BackClosedMode)), BackClosedMode == EInputMode::Normal);

	// ── ⑥ 닫기 경로 B ― 도크 홈 버튼 ─────────────────────────────────────────
	if (!CallNoArgHandler(Entry, TEXT("OnWorldMapButtonClicked")))
	{
		AddError(TEXT("[Task10] 두 번째 push 에서 OnWorldMapButtonClicked 를 못 불렀습니다."));
		return false;
	}
	const int32 SecondPush = UIBase->GetPromptStackCount();
	TestEqual(TEXT("한 번 닫은 뒤 같은 버튼으로 다시 열린다 ― 닫기가 스택을 못 비웠으면 가드에 막혀 여기서 갈린다"),
		SecondPush, BeforeStack + 1);
	UWorldMapPanelWidget* SecondPanel = Cast<UWorldMapPanelWidget>(UIBase->GetActivePromptWidget());
	TestNotNull(TEXT("두 번째 push 도 UWorldMapPanelWidget 이다"), SecondPanel);
	if (SecondPanel == nullptr)
	{
		return false;
	}

	// 도크가 빠진 뒤 화면에 남는 닫기 수단은 오른쪽 위 공용 X 하나다.
	UCloseButtonWidget* MapClose = Cast<UCloseButtonWidget>(SecondPanel->GetWidgetFromName(TEXT("CloseButton")));
	TestNotNull(TEXT("패널 오른쪽 위에 공용 X(CloseButton)가 있다 ― 없으면 닫기 경로가 백 액션 하나뿐이다"), MapClose);
	if (MapClose != nullptr)
	{
		// ★ 범위 고지: X 도 **델리게이트 직접 방송**이다 ― UButton 클릭 라우팅(히트테스트)은 이 단언이 덮지 않는다.
		MapClose->OnCloseClicked.Broadcast();
		TestFalse(TEXT("X 경로가 패널을 비활성화한다 (DeactivateWidget) (★클릭 라우팅 미검증 ― 델리게이트 직접 방송)"),
			SecondPanel->IsActivated());
		TestEqual(TEXT("X 뒤 스택이 원래 높이로 내려온다 (★클릭 라우팅 미검증 ― 델리게이트 직접 방송)"),
			UIBase->GetPromptStackCount(), BeforeStack);
		const EInputMode CloseMode = PC->GetCurrentInputMode();
		TestTrue(FString::Printf(TEXT("X 로 닫아도 입력 모드가 Normal 로 돌아온다 (실제=%s)"),
			InputModeLabel(CloseMode)), CloseMode == EInputMode::Normal);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
