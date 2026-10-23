#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Enum/WidgetType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ResourceInfo.h"
#include "Table/WidgetDataTable.h"
#include "UI/Element/Common/CoinFlyoutContainerWidget.h"
#include "UI/Element/Notifications/NotificationElementWidget.h"

#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

namespace CGRFundsToastPresentation
{
void Resolve(
	const FResourceInfo& MoneyInfo,
	bool bResourceInfoOk,
	UTexture2D*& OutMoneyIcon,
	FLinearColor& OutMoneyColor);
}

namespace
{
template <typename TWidget, typename = void>
struct THasIncomeTextIconApi : std::false_type
{
};

template <typename TWidget>
struct THasIncomeTextIconApi<TWidget, std::void_t<decltype(
	static_cast<void (TWidget::*)(FVector2D, int64, UTexture2D*, FLinearColor)>(&TWidget::SpawnIncomeText))>>
	: std::true_type
{
};

template <typename TWidget>
void TestIncomeTextIconBehavior(FAutomationTestBase& Test, TWidget* CoinFlyout, UTexture2D* TestTexture)
{
	if constexpr (!THasIncomeTextIconApi<TWidget>::value)
	{
		Test.AddError(TEXT(
			"CoinFlyout must expose SpawnIncomeText(FVector2D, int64, UTexture2D*, FLinearColor) so income rows can receive DT Money presentation data."));
	}
	else
	{
		UCanvasPanel* Canvas = Cast<UCanvasPanel>(CoinFlyout->WidgetTree->RootWidget);
		if (!Test.TestNotNull(TEXT("직원 수익 캔버스"), Canvas)) return;
		const FLinearColor FirstColor(0.17f, 0.43f, 0.71f, 0.83f);
		const FLinearColor SecondColor(0.71f, 0.28f, 0.15f, 0.83f);
		CoinFlyout->SpawnIncomeText(FVector2D::ZeroVector, 124000, TestTexture, FirstColor);
		if (!Test.TestEqual(TEXT("Money 아이콘과 +12.4만 행이 생성된다"), Canvas->GetChildrenCount(), 1)) return;
		UHorizontalBox* FirstRow = Cast<UHorizontalBox>(Canvas->GetChildAt(0));
		if (!Test.TestNotNull(TEXT("첫 수익 행"), FirstRow)) return;
		UImage* IncomeIcon = Cast<UImage>(FirstRow->GetChildAt(0));
		if (Test.TestNotNull(TEXT("전달한 Money 아이콘이 행에 있다"), IncomeIcon))
		{
			Test.TestTrue(TEXT("Money 아이콘 텍스처가 유지된다"),
				IncomeIcon->GetBrush().GetResourceObject() == TestTexture);
			Test.TestTrue(TEXT("직원 수익 Money 아이콘은 48x48이다"),
				IncomeIcon->GetBrush().ImageSize.Equals(FVector2D(48.0f, 48.0f), KINDA_SMALL_NUMBER));
			const UHorizontalBoxSlot* IconBoxSlot = Cast<UHorizontalBoxSlot>(IncomeIcon->Slot);
			if (Test.TestNotNull(TEXT("Money 아이콘의 HorizontalBox 슬롯"), IconBoxSlot))
			{
				Test.TestEqual(TEXT("직원 수익 아이콘 오른쪽 간격은 8px이다"),
					IconBoxSlot->GetPadding().Right, 8.0f);
			}
		}

		auto FindFirstGlyphMID = [&Test](UHorizontalBox* IncomeRow) -> UMaterialInstanceDynamic*
		{
			if (!Test.TestNotNull(TEXT("수익 행 글리프 폭 컨테이너"),
				IncomeRow->GetChildAt(IncomeRow->GetChildrenCount() - 1))) return nullptr;
			USizeBox* Bounds = Cast<USizeBox>(IncomeRow->GetChildAt(IncomeRow->GetChildrenCount() - 1));
			if (!Test.TestNotNull(TEXT("글리프 SizeBox"), Bounds)) return nullptr;
			UCanvasPanel* GlyphCanvas = Cast<UCanvasPanel>(Bounds->GetChildAt(0));
			if (!Test.TestNotNull(TEXT("글리프 Canvas"), GlyphCanvas)) return nullptr;
			Test.TestEqual(TEXT("+12.4만은 6개 글리프 이미지다"), GlyphCanvas->GetChildrenCount(), 6);
			UImage* FirstGlyph = Cast<UImage>(GlyphCanvas->GetChildAt(0));
			if (!Test.TestNotNull(TEXT("글리프 첫 글자 +"), FirstGlyph)) return nullptr;
			return Cast<UMaterialInstanceDynamic>(FirstGlyph->GetBrush().GetResourceObject());
		};
		UMaterialInstanceDynamic* FirstMID = FindFirstGlyphMID(FirstRow);
		FLinearColor FirstMidColor = FLinearColor::White;
		if (Test.TestNotNull(TEXT("+ 글리프가 동적 재질을 사용한다"), FirstMID))
		{
			Test.TestTrue(TEXT("+ 글리프가 Plus 스프라이트를 사용한다"),
				FirstMID->K2_GetTextureParameterValue(TEXT("Glyph"))
				&& FirstMID->K2_GetTextureParameterValue(TEXT("Glyph"))->GetName() == TEXT("T_UI_IncomeGlyph_Plus"));
			Test.TestTrue(TEXT("전달한 Money 색이 글리프 램프에 도달한다"),
				FirstMID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("MidColor")), FirstMidColor)
				&& !FirstMidColor.Equals(FLinearColor::White));
		}

		CoinFlyout->SpawnIncomeText(FVector2D::ZeroVector, 124000, nullptr, SecondColor);
		Test.TestEqual(TEXT("아이콘이 null이어도 두 번째 숫자 행이 추가된다"), Canvas->GetChildrenCount(), 2);
		if (Canvas->GetChildrenCount() != 2) return;
		UHorizontalBox* SecondRow = Cast<UHorizontalBox>(Canvas->GetChildAt(1));
		if (!Test.TestNotNull(TEXT("아이콘 없는 수익 행"), SecondRow)) return;
		Test.TestEqual(TEXT("아이콘이 없으면 글리프 컨테이너만 있다"), SecondRow->GetChildrenCount(), 1);
		UMaterialInstanceDynamic* SecondMID = FindFirstGlyphMID(SecondRow);
		if (Test.TestNotNull(TEXT("재사용된 + 글리프 재질"), SecondMID))
		{
			Test.TestTrue(TEXT("같은 글리프는 MID를 재사용한다"), SecondMID == FirstMID);
			FLinearColor SecondMidColor;
			Test.TestTrue(TEXT("새 Money 색은 재사용 MID의 램프를 갱신한다"),
				SecondMID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("MidColor")), SecondMidColor)
				&& !SecondMidColor.Equals(FirstMidColor));
		}
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFundsDisplayAmountContractTest,
	"CGR.FundsDisplay.AmountContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFundsDisplayAmountContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("기본 양수 표기"),
		UGlobalUtilFunctions::FormatFundsAmount(124000, false).ToString(), FString(TEXT("12.4만")));
	TestEqual(TEXT("양수 부호 표기"),
		UGlobalUtilFunctions::FormatFundsAmount(124000, true).ToString(), FString(TEXT("+12.4만")));
	TestEqual(TEXT("음수는 기존 부호 유지"),
		UGlobalUtilFunctions::FormatFundsAmount(-124000, true).ToString(), FString(TEXT("-12.4만")));
	TestEqual(TEXT("0에는 양수 부호 없음"),
		UGlobalUtilFunctions::FormatFundsAmount(0, true).ToString(), FString(TEXT("0")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFundsDisplayEmployeeVisualContractTest,
	"CGR.FundsDisplay.EmployeeVisualContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFundsDisplayEmployeeVisualContractTest::RunTest(const FString& Parameters)
{
	UCoinFlyoutContainerWidget* CoinFlyout =
		NewObject<UCoinFlyoutContainerWidget>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("CoinFlyout 테스트 인스턴스를 생성한다"), CoinFlyout))
	{
		return true;
	}

	const bool bInitialized = static_cast<UUserWidget*>(CoinFlyout)->Initialize();
	if (!TestTrue(TEXT("CoinFlyout 테스트 인스턴스가 초기화된다"), bInitialized)
		|| !TestNotNull(TEXT("CoinFlyout 초기화가 WidgetTree를 만든다"), CoinFlyout->WidgetTree.Get()))
	{
		return true;
	}

	UTexture2D* TestTexture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("행동 검사에 사용할 transient Money 아이콘을 생성한다"), TestTexture))
	{
		return true;
	}

	TestIncomeTextIconBehavior(*this, CoinFlyout, TestTexture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFundsDisplayFundsToastVisualContractTest,
	"CGR.FundsDisplay.FundsToastVisualContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFundsDisplayFundsToastVisualContractTest::RunTest(const FString& Parameters)
{
	const FLinearColor ExpectedMoneyColor(0.21f, 0.47f, 0.73f, 0.91f);
	FResourceInfo MoneyInfoWithoutIcon;
	MoneyInfoWithoutIcon.UIColor = ExpectedMoneyColor;
	UTexture2D* ResolvedMoneyIcon = nullptr;
	FLinearColor ResolvedMoneyColor = FLinearColor::White;
	CGRFundsToastPresentation::Resolve(
		MoneyInfoWithoutIcon, true, ResolvedMoneyIcon, ResolvedMoneyColor);
	TestTrue(TEXT("A valid Money row with no icon keeps a numeric-only presentation"),
		ResolvedMoneyIcon == nullptr);
	TestTrue(TEXT("A valid Money row with no icon still keeps its DT UIColor"),
		ResolvedMoneyColor.Equals(ExpectedMoneyColor, KINDA_SMALL_NUMBER));

	ResolvedMoneyColor = ExpectedMoneyColor;
	CGRFundsToastPresentation::Resolve(
		MoneyInfoWithoutIcon, false, ResolvedMoneyIcon, ResolvedMoneyColor);
	TestTrue(TEXT("A missing Money row keeps the neutral fallback color"),
		ResolvedMoneyColor.Equals(FLinearColor::White, KINDA_SMALL_NUMBER));

	const TCHAR* FundsToastClassPath =
		TEXT("/Game/CompanyGrowth/UI/Elements/Notifications/UIE_FundsToast.UIE_FundsToast_C");
	UClass* FundsToastClass = LoadClass<UUserWidget>(nullptr, FundsToastClassPath);

	const TCHAR* WidgetClassTablePath =
		TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass");
	UDataTable* WidgetClassTable = LoadObject<UDataTable>(nullptr, WidgetClassTablePath);
	if (TestNotNull(TEXT("DT_WidgetClass asset loads"), WidgetClassTable))
	{
		const FWidgetDataTable* FundsToastRow = WidgetClassTable->FindRow<FWidgetDataTable>(
			FName(TEXT("FundsToast")), TEXT("CGR.FundsDisplay.FundsToastVisualContract"), false);
		if (TestNotNull(TEXT("DT_WidgetClass contains the FundsToast row"), FundsToastRow))
		{
			TestTrue(TEXT("FundsToast row stores EWidgetType::FundsToast"),
				FundsToastRow->WidgetType == EWidgetType::FundsToast);

			UClass* RegisteredFundsToastClass = FundsToastRow->WidgetClass.Get();
			TestTrue(TEXT("FundsToast row points to UIE_FundsToast_C"),
				RegisteredFundsToastClass == FundsToastClass);
			if (TestNotNull(TEXT("FundsToast row has a widget class"), RegisteredFundsToastClass))
			{
				TestTrue(TEXT("Registered FundsToast class derives from UNotificationElementWidget"),
					RegisteredFundsToastClass->IsChildOf(UNotificationElementWidget::StaticClass()));
			}
		}
	}

	if (!FundsToastClass)
	{
		AddError(FString::Printf(TEXT("Funds toast generated class is missing: %s"), FundsToastClassPath));
	}
	else if (const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(FundsToastClass))
	{
		const UWidgetTree* WidgetTreeArchetype = GeneratedClass->GetWidgetTreeArchetype();
		if (!WidgetTreeArchetype)
		{
			AddError(TEXT("UIE_FundsToast generated class must own a widget tree archetype."));
		}
		else
		{
			const UImage* ResourceIcon =
				WidgetTreeArchetype->FindWidget<UImage>(FName(TEXT("Image_ResourceIcon")));
			if (TestNotNull(TEXT("UIE_FundsToast 아키타입에 Image_ResourceIcon 계약이 존재한다"),
				ResourceIcon))
			{
				TestTrue(TEXT("Image_ResourceIcon defaults to Collapsed"),
					ResourceIcon->GetVisibility() == ESlateVisibility::Collapsed);
				TestTrue(TEXT("FundsToast Money 아이콘은 48x48이다"),
					ResourceIcon->GetBrush().ImageSize.Equals(
						FVector2D(48.0f, 48.0f), KINDA_SMALL_NUMBER));

				const UHorizontalBoxSlot* ResourceIconBoxSlot =
					Cast<UHorizontalBoxSlot>(ResourceIcon->Slot);
				if (TestNotNull(TEXT("FundsToast Money 아이콘은 HorizontalBox 슬롯을 사용한다"),
					ResourceIconBoxSlot))
				{
					TestEqual(TEXT("FundsToast 아이콘 오른쪽 간격은 8px이다"),
						ResourceIconBoxSlot->GetPadding().Right, 8.0f);
				}
			}

			const UTextBlock* MessageText =
				WidgetTreeArchetype->FindWidget<UTextBlock>(FName(TEXT("Text_Message")));
			if (TestNotNull(TEXT("UIE_FundsToast 아키타입에 Text_Message 계약이 존재한다"),
				MessageText))
			{
				TestEqual(TEXT("FundsToast 금액 크기는 기존 34를 유지한다"),
					MessageText->GetFont().Size, 34.0f);
				TestEqual(TEXT("FundsToast 금액은 Pretendard SemiBold face를 유지한다"),
					MessageText->GetFont().TypefaceFontName, FName(TEXT("SemiBold")));

				UObject* ExpectedPretendardFont = LoadObject<UObject>(nullptr,
					TEXT("/Game/CompanyGrowth/Font/Pretendard/F_Pretendard.F_Pretendard"));
				if (TestNotNull(TEXT("FundsToast Pretendard 폰트 에셋을 로드한다"), ExpectedPretendardFont))
				{
					TestTrue(TEXT("FundsToast 금액은 Pretendard를 유지한다"),
						MessageText->GetFont().FontObject == ExpectedPretendardFont);
				}
			}
		}
	}
	else
	{
		AddError(TEXT("UIE_FundsToast must be a Widget Blueprint generated class."));
	}

	return true;
}

#endif
