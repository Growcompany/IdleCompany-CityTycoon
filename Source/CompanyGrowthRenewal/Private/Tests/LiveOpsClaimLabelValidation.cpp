#include "Misc/AutomationTest.h"
#include "ICommonInputModule.h"
#include "CommonInputSettings.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/ListView.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/LiveOps/MailRowWidget.h"
#include "UI/Element/LiveOps/QuestRowWidget.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsClaimLabelTest, "CGR.LiveOps.ClaimLabelRecycling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsClaimLabelTest::RunTest(const FString& Parameters)
{
	// 라이브옵스 창은 뒤로가기 처리 대상이라 생성 때 CommonInput 의 뒤로가기 액션을 찾는다 ― 게임 없이 만들면 입력 데이터가 아직 안 읽혀 ensure 가 난다
	ICommonInputModule::GetSettings().LoadData();
	UWorld* TestWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), TestWorld)) return false;
	const FString OutputDirectory = FPaths::ProjectSavedDir() / TEXT("LiveOpsUI/claim-label");
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	const FLinearColor GrayInk = FLinearColor::FromSRGBColor(FColor(27, 35, 48));

	for (const bool bQuest : { true, false })
	{
		const FString Kind = bQuest ? TEXT("Quest") : TEXT("Mail");
		const FString AssetPath = FString::Printf(
			TEXT("/Game/CompanyGrowth/UI/Elements/LiveOps/UIE_%sRow.UIE_%sRow_C"), *Kind, *Kind);
		UWidgetBlueprintGeneratedClass* RowClass = Cast<UWidgetBlueprintGeneratedClass>(
			LoadClass<UUserWidget>(nullptr, *AssetPath));
		if (!TestNotNull(Kind + TEXT(" WBP class"), RowClass)) continue;
		UWidgetTree* AuthoredTree = RowClass->GetWidgetTreeArchetype();
		const UButtonWidget* AuthoredButton = AuthoredTree
			? Cast<UButtonWidget>(AuthoredTree->FindWidget(TEXT("ClaimButton"))) : nullptr;
		if (!TestNotNull(Kind + TEXT(" authored ClaimButton"), AuthoredButton)) continue;
		const FLinearColor GoldInk = AuthoredButton->GetTextColor().GetSpecifiedColor();
		if (!bQuest)
		{
			TestFalse(Kind + TEXT(" gold and gray inks are distinct"), GoldInk.Equals(GrayInk));
		}
		// 2026-09-26 규칙 개정: 라벨 아웃라인 전면 폐지(사용자 지시)
		TestFalse(Kind + TEXT(" authored outline disabled"), AuthoredButton->GetTextOutlineEnabled());

		// 엔트리 생성은 ListView → WidgetTree → UserWidget 소유 경로가 필요하다.
		// 실제 패널의 리스트를 사용해 런타임과 같은 소유권/풀 생명주기를 검증한다.
		const TCHAR* PanelPath = bQuest
			? TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_QuestPanel.UI_QuestPanel_C")
			: TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_MailboxPanel.UI_MailboxPanel_C");
		UClass* PanelClass = LoadClass<UUserWidget>(nullptr, PanelPath);
		if (!TestNotNull(Kind + TEXT(" panel class"), PanelClass)) continue;
		TStrongObjectPtr<UUserWidget> Panel(CreateWidget<UUserWidget>(TestWorld, PanelClass));
		if (!TestNotNull(Kind + TEXT(" owning panel"), Panel.Get())) continue;
		TStrongObjectPtr<UListView> List(Cast<UListView>(Panel->GetWidgetFromName(bQuest ? TEXT("QuestList") : TEXT("MailList"))));
		if (!TestNotNull(Kind + TEXT(" owned list"), List.Get())) continue;
		if (!TestTrue(Kind + TEXT(" list outer is owning widget tree"),
			List->GetOuter() == Panel->WidgetTree && Panel->WidgetTree != nullptr)) continue;
		if (!TestTrue(Kind + TEXT(" correct entry class"), List->GetEntryWidgetClass() == RowClass)) continue;
		TStrongObjectPtr<UQuestRowItem> QuestItem(NewObject<UQuestRowItem>(List.Get()));
		TStrongObjectPtr<UMailRowItem> MailItem(NewObject<UMailRowItem>(List.Get()));
		QuestItem->Entry.Row.Title = FText::FromString(TEXT("미션 상태 전환 검증"));
		QuestItem->Entry.Target = 5;
		QuestItem->Entry.Current = 5;
		MailItem->Entry.Title = FText::FromString(TEXT("우편 상태 전환 검증"));
		MailItem->NowUnix = 1000;
		UObject* ListItem = bQuest ? static_cast<UObject*>(QuestItem.Get()) : static_cast<UObject*>(MailItem.Get());
		List->SetListItems(TArray<UObject*>{ ListItem });
		const TSharedRef<SWidget> ListSlate = List->TakeWidget();
		FWidgetRenderer Renderer(true);
		const FVector2D DrawSize(1800.f, 240.f);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
		if (!TestNotNull(TEXT("Render target"), Target.Get())) return false;
		UUserWidget* FirstRow = nullptr;

		// 같은 풀 엔트리에 불가 → 가능 → 완료/만료 → 서버 대기 → 가능을 차례로 전달한다.
		for (int32 StateIndex = 0; StateIndex < 5; ++StateIndex)
		{
			const bool bServerValid = StateIndex != 3;
			const bool bGold = StateIndex == 1 || StateIndex == 4 || (bQuest && StateIndex == 3);
			QuestItem->bServerTimeValid = bServerValid;
			QuestItem->Entry.State = bGold ? EQuestState::Claimable
				: (StateIndex == 2 ? EQuestState::Claimed : EQuestState::InProgress);
			MailItem->bServerTimeValid = bServerValid;
			MailItem->Entry.ExpireUnix = StateIndex == 0 || StateIndex == 2 ? 999 : 2000;
			List->RegenerateAllEntries();
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				Renderer.DrawWidget(Target.Get(), ListSlate, 1.f, DrawSize, 0.f);
				FlushRenderingCommands();
			}
			const FString Context = FString::Printf(TEXT("%s state %d"), *Kind, StateIndex);
			UUserWidget* Row = List->GetEntryWidgetFromItem<UUserWidget>(ListItem);
			if (!TestNotNull(Context + TEXT(" generated row"), Row)) continue;
			if (!FirstRow) FirstRow = Row;
			TestTrue(Context + TEXT(" reuses same pooled row"), FirstRow == Row);
			UButtonWidget* Button = Cast<UButtonWidget>(Row->GetWidgetFromName(TEXT("ClaimButton")));
			if (!TestNotNull(Context + TEXT(" button"), Button)) continue;
			UCommonTextBlock* Label = Cast<UCommonTextBlock>(Button->GetWidgetFromName(TEXT("ButtonText")));
			if (!TestNotNull(Context + TEXT(" rendered label"), Label)) continue;
			// 미션 줄 라벨 색은 상태별 버튼 스타일(받기 초록/진행 중 회색)이 정하므로 우편만 잉크를 비교한다
			if (!bQuest)
			{
				TestTrue(Context + TEXT(" label color"), Label->GetColorAndOpacity().GetSpecifiedColor().Equals(bGold ? GoldInk : GrayInk, 0.0001f));
			}
			TestEqual(Context + TEXT(" label outline size"), Label->GetFont().OutlineSettings.OutlineSize, 0);
			TestEqual(Context + TEXT(" claim enabled"), Button->GetIsEnabled(), bGold && bServerValid);
			if (bQuest)
			{
				const bool bClaimed = StateIndex == 2;
				TestTrue(Context + TEXT(" button visibility"), Button->GetVisibility()
					== (bClaimed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible));
				if (!bClaimed)
				{
					TestEqual(Context + TEXT(" button label"), Label->GetText().ToString(),
						FString(bGold ? TEXT("받기") : TEXT("진행 중")));
				}
				UWidget* Stamp = Row->GetWidgetFromName(TEXT("DoneStamp"));
				if (TestNotNull(Context + TEXT(" done stamp"), Stamp))
				{
					TestTrue(Context + TEXT(" done stamp visibility"), Stamp->GetVisibility()
						== (bClaimed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed));
				}
				UWidget* Dot = Row->GetWidgetFromName(TEXT("ClaimDot"));
				if (TestNotNull(Context + TEXT(" claim dot"), Dot))
				{
					TestEqual(Context + TEXT(" claim dot shown"), Dot->GetVisibility() != ESlateVisibility::Collapsed, bGold && bServerValid);
				}
			}
			FImage Image;
			if (TestTrue(Context + TEXT(" render readback"), FImageUtils::GetRenderTargetImage(Target.Get(), Image)))
			{
				Image.GammaSpace = EGammaSpace::sRGB;
				TArray64<uint8> Bytes;
				const FString OutputPath = OutputDirectory / FString::Printf(TEXT("%s-%d.png"), *Kind, StateIndex);
				TestTrue(Context + TEXT(" PNG saved"), FImageUtils::CompressImage(Bytes, TEXT("png"), Image)
					&& FFileHelper::SaveArrayToFile(Bytes, *OutputPath));
			}
		}
		List->ReleaseSlateResources(true);
	}
	return !HasAnyErrors();
}

#endif
