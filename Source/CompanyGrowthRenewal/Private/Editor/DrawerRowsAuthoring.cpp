// 국가 서랍 줄 부품(월드맵 UI 정돈 Task 6)의 에디터 전용 콘솔 명령 두 개.
//
//   CGR.WorldMapUI.RegisterGoodsTile  ― DT_WidgetClass 에 DrawerGoodsTile 한 행만 네이티브 AddRow
//   CGR.WorldMapUI.RenderRows         ― 줄 WBP 를 상태별 픽스처로 그려 PNG + 지오메트리 JSON
//
// 왜 파이썬이 아닌가: DataTableFunctionLibrary.AddDataTableRow 는 파이썬에 노출되지 않고, JSON fill 은 이름이 폐기된
// raw enum 행(LootBoxLayerWidget 등)을 None 으로 바꿔 버린다(GachaRevealBoxWidgetTableAuthoring 과 같은 계약).
// 보고서 = Saved/WorldMapUIOverhaul/Task6/{dt_widgetclass.json, render/*.png|json}

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "Table/WidgetDataTable.h"
#include "CommonButtonBase.h"
#include "UI/Element/Trade/DrawerGoodsTileWidget.h"
#include "UI/Element/Trade/DrawerHeroRowWidget.h"
#include "UI/Element/Trade/DrawerInfoRowWidget.h"
#include "UI/Element/Trade/DrawerPageToggleWidget.h"
#include "UI/Element/Trade/WorldFactoryLineWidget.h"
#include "UI/Element/Trade/MineLineWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

namespace CGRDrawerRowsAuthoring
{
	static const TCHAR* TableAssetPath = TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass");
	static const TCHAR* GoodsRowName = TEXT("DrawerGoodsTile");
	static const TCHAR* GoodsClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/WorldMap/UIE_DrawerGoodsTile.UIE_DrawerGoodsTile_C");
	static const TCHAR* WorldMapElements = TEXT("/Game/CompanyGrowth/UI/Elements/WorldMap/");

	static FString OutDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldMapUIOverhaul/Task6"));
	}

	static bool WriteJson(const TSharedRef<FJsonObject>& Report, const FString& Filename)
	{
		FString Json;
		return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			&& FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	// 이름이 폐기된 enum 도 원시 값으로 비교한다
	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static void RegisterGoodsTile(const TArray<FString>&)
	{
		IFileManager::Get().MakeDirectory(*OutDir(), true);
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetBoolField(TEXT("saved"), false);
		Report->SetBoolField(TEXT("other_rows_preserved"), false);
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		ON_SCOPE_EXIT { WriteJson(Report, OutDir() / TEXT("dt_widgetclass.json")); };

		if (!GEditor || GEditor->PlayWorld)
		{
			Report->SetStringField(TEXT("error"), TEXT("editor_only_pie_stopped"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr, TableAssetPath);
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
		{
			Report->SetStringField(TEXT("error"), TEXT("table_missing_or_wrong_row_struct"));
			return;
		}
		// 남이 이 에디터에서 고치다 만 행까지 같이 저장되지 않게 한다
		if (Table->GetOutermost()->IsDirty())
		{
			Report->SetStringField(TEXT("error"), TEXT("table_has_unsaved_edits"));
			return;
		}
		UClass* GoodsClass = LoadObject<UClass>(nullptr, GoodsClassPath);
		if (!GoodsClass || !GoodsClass->IsChildOf(UDrawerGoodsTileWidget::StaticClass()))
		{
			Report->SetStringField(TEXT("error"), GoodsClass ? TEXT("goods_class_wrong_parent") : TEXT("goods_widget_blueprint_missing"));
			return;
		}

		TMap<FName, FWidgetDataTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FWidgetDataTable*>(Pair.Value));
		}
		Report->SetNumberField(TEXT("before_count"), Before.Num());

		const FName RowKey(GoodsRowName);
		// 같은 WidgetType 을 다른 행이 이미 가지면 TableManager 캐시에서 한쪽이 조용히 이긴다
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key != RowKey && Pair.Value.WidgetType == EWidgetType::DrawerGoodsTile)
			{
				Report->SetStringField(TEXT("error"), TEXT("widget_type_already_used_by:") + Pair.Key.ToString());
				return;
			}
		}

		FWidgetDataTable Row;
		Row.WidgetType = EWidgetType::DrawerGoodsTile;
		Row.WidgetClass = GoodsClass;
		if (const FWidgetDataTable* Existing = Before.Find(RowKey))
		{
			const bool bSame = RowsEqual(*Existing, Row);
			Report->SetBoolField(TEXT("success"), bSame);
			Report->SetBoolField(TEXT("already_registered"), bSame);
			Report->SetBoolField(TEXT("other_rows_preserved"), bSame);
			Report->SetNumberField(TEXT("after_count"), Before.Num());
			Report->SetStringField(TEXT("error"), bSame ? TEXT("") : TEXT("row_exists_with_other_values"));
			return;
		}
		Table->AddRow(RowKey, Row);

		bool bOthersPreserved = true;
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			const FWidgetDataTable* Now = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("DrawerGoodsTileRegister"), false);
			bOthersPreserved &= Now && RowsEqual(*Now, Pair.Value);
		}
		const FWidgetDataTable* Added = Table->FindRow<FWidgetDataTable>(RowKey, TEXT("DrawerGoodsTileRegister"), false);
		const bool bAddedExact = Added && RowsEqual(*Added, Row);
		const int32 AfterCount = Table->GetRowMap().Num();
		Report->SetNumberField(TEXT("after_count"), AfterCount);
		Report->SetBoolField(TEXT("other_rows_preserved"), bOthersPreserved);
		Report->SetBoolField(TEXT("added_row_exact"), bAddedExact);
		if (!bOthersPreserved || !bAddedExact || AfterCount != Before.Num() + 1)
		{
			// 메모리에 남기면 다음 저장(남의 저장 포함)에 섞여 나간다
			Table->RemoveRow(RowKey);
			Report->SetStringField(TEXT("error"), TEXT("rows_not_as_expected_not_saved"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		// 기본값 GError 는 파일 이동 실패(Error Code 32)에서 에디터를 죽인다 ― 경고로 받고 false 를 보고한다
		SaveArgs.Error = GWarn;
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
		if (!bSaved)
		{
			Table->RemoveRow(RowKey);
			Package->SetDirtyFlag(false);
			Report->SetNumberField(TEXT("after_count"), Table->GetRowMap().Num());
		}
		Report->SetStringField(TEXT("row_class"), GoodsClass->GetPathName());
		Report->SetBoolField(TEXT("saved"), bSaved);
		Report->SetBoolField(TEXT("success"), bSaved);
		Report->SetStringField(TEXT("error"), bSaved ? TEXT("") : TEXT("save_failed_row_removed"));
		UE_LOG(LogTemp, Log, TEXT("[DrawerRows] %s rows=%d saved=%d"), GoodsRowName, Table->GetRowMap().Num(), bSaved ? 1 : 0);
	}

	// ── 렌더 픽스처 ──
	static void GatherWidgets(UUserWidget* Panel, TArray<UWidget*>& Out)
	{
		if (!Panel->WidgetTree) { return; }
		TArray<UWidget*> TreeWidgets;
		Panel->WidgetTree->GetAllWidgets(TreeWidgets);
		for (UWidget* Widget : TreeWidgets)
		{
			if (!Widget || Out.Contains(Widget)) { continue; }
			Out.Add(Widget);
			if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				GatherWidgets(Nested, Out);
			}
		}
	}

	static UTexture2D* Tex(const TCHAR* Path)
	{
		return LoadObject<UTexture2D>(nullptr, Path);
	}

	// 디자이너 플래그 + PreConstruct 실행: 게임 Construct/Tick(매니저 조회)은 막고 버튼 스타일·글자 크기는 입힌다
	static bool RenderOne(UWorld* World, const FString& Asset, const FString& Label, float Width,
		TFunctionRef<void(UUserWidget*)> Before, TFunctionRef<void(UUserWidget*)> Fixture, TArray<TSharedPtr<FJsonValue>>& Index)
	{
		const FString ClassPath = FString(WorldMapElements) + Asset + TEXT(".") + Asset + TEXT("_C");
		UClass* WidgetClass = LoadObject<UClass>(nullptr, *ClassPath);
		if (!WidgetClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[DrawerRows] render class missing %s"), *ClassPath);
			return false;
		}
		TStrongObjectPtr<UUserWidget> Panel(NewObject<UUserWidget>(World, WidgetClass, NAME_None, RF_Transient));
		const EWidgetDesignFlags Flags = EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct;
		Panel->SetDesignerFlags(Flags);
		if (!Panel->Initialize() || !Panel->GetRootWidget())
		{
			return false;
		}
		TArray<UWidget*> Widgets{Panel.Get()};
		GatherWidgets(Panel.Get(), Widgets);
		TArray<ESlateVisibility> Authored;
		for (UWidget* Widget : Widgets)
		{
			Widget->SetDesignerFlags(Flags);
			Authored.Add(Widget->GetVisibility());
		}
		// 인스턴스 속성(WBP 디테일에서 고르는 값)은 Slate 를 만들기 전에 넣어야 PreConstruct 가 읽는다
		Before(Panel.Get());
		TSharedPtr<SWidget> Slate = Panel->TakeWidget();
		ON_SCOPE_EXIT
		{
			Panel->ReleaseSlateResources(true);
			Slate.Reset();
		};
		// 디자이너는 숨김을 Visible 로 덮으므로 저작 Visibility 를 돌려준 뒤 런타임 세터를 태운다
		for (int32 I = 0; I < Widgets.Num(); ++I)
		{
			Widgets[I]->SetVisibility(Authored[I]);
		}
		Fixture(Panel.Get());

		Slate->SlatePrepass(1.f);
		const FVector2D Desired = Slate->GetDesiredSize();
		const FVector2D DrawSize(Width > 0.f ? Width : FMath::CeilToFloat(Desired.X), FMath::CeilToFloat(FMath::Max(Desired.Y, 1.f)));
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
		if (!Target.IsValid()) { return false; }
		ON_SCOPE_EXIT
		{
			Target->ReleaseResource();
			FlushRenderingCommands();
		};
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Slate.ToSharedRef(), 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		const FString Base = OutDir() / TEXT("render") / Label;
		FImage Image;
		TArray64<uint8> Png;
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), Image))
		{
			return false;
		}
		Image.GammaSpace = EGammaSpace::sRGB;
		if (!FImageUtils::CompressImage(Png, TEXT("png"), Image) || !FFileHelper::SaveArrayToFile(Png, *(Base + TEXT(".png"))))
		{
			return false;
		}
		const FGeometry RootGeometry = Panel->GetRootWidget()->GetCachedGeometry();
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("asset"), Asset);
		Report->SetNumberField(TEXT("draw_width"), DrawSize.X);
		Report->SetNumberField(TEXT("draw_height"), DrawSize.Y);
		Report->SetNumberField(TEXT("desired_width"), Desired.X);
		Report->SetNumberField(TEXT("desired_height"), Desired.Y);
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (UWidget* Widget : Widgets)
		{
			if (Widget == Panel.Get() || Widget->GetTypedOuter<UUserWidget>() != Panel.Get()) { continue; }
			const FGeometry& G = Widget->GetCachedGeometry();
			const FVector2D Pos = RootGeometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D Size = G.GetLocalSize();
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Widget->GetName());
			Row->SetStringField(TEXT("visibility"), StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility())));
			Row->SetNumberField(TEXT("x"), FMath::IsFinite(Pos.X) ? Pos.X : -1.0);
			Row->SetNumberField(TEXT("y"), FMath::IsFinite(Pos.Y) ? Pos.Y : -1.0);
			Row->SetNumberField(TEXT("w"), FMath::IsFinite(Size.X) ? Size.X : -1.0);
			Row->SetNumberField(TEXT("h"), FMath::IsFinite(Size.Y) ? Size.Y : -1.0);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Report->SetArrayField(TEXT("widgets"), Rows);
		WriteJson(Report, Base + TEXT(".json"));
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("label"), Label);
		Entry->SetNumberField(TEXT("width"), DrawSize.X);
		Entry->SetNumberField(TEXT("height"), DrawSize.Y);
		Index.Add(MakeShared<FJsonValueObject>(Entry));
		return true;
	}

	static void RenderRows(const TArray<FString>&)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[DrawerRows] RenderRows needs the editor world with PIE stopped"));
			return;
		}
		IFileManager::Get().MakeDirectory(*(OutDir() / TEXT("render")), true);
		UTexture2D* Box = Tex(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/box.box"));
		UTexture2D* Glyph = Tex(TEXT("/Game/CompanyGrowth/UI/Textures/Industry/T_IndustryGlyph_Electronics.T_IndustryGlyph_Electronics"));
		const float RowWidth = 1064.f;
		TArray<TSharedPtr<FJsonValue>> Index;
		int32 Failures = 0;
		auto NoBefore = [](UUserWidget*) {};
		auto RunWith = [&](const TCHAR* Asset, const TCHAR* Label, float Width, TFunctionRef<void(UUserWidget*)> Before,
			TFunctionRef<void(UUserWidget*)> Fixture)
		{
			if (!RenderOne(World, Asset, Label, Width, Before, Fixture, Index)) { ++Failures; }
		};
		auto Run = [&](const TCHAR* Asset, const TCHAR* Label, float Width, TFunctionRef<void(UUserWidget*)> Fixture)
		{
			RunWith(Asset, Label, Width, NoBefore, Fixture);
		};
		UClass* GoldStyle = LoadObject<UClass>(nullptr,
			TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Gold.CUI_Style2_Btn_Gold_C"));
		// Task 7 이 공장·광산 요약 줄 인스턴스에 넣을 값과 같은 것
		auto GoldHero = [GoldStyle](UUserWidget* W)
		{
			if (UDrawerHeroRowWidget* Hero = Cast<UDrawerHeroRowWidget>(W))
			{
				if (GoldStyle && GoldStyle->IsChildOf(UCommonButtonStyle::StaticClass()))
				{
					Hero->ActionButtonStyle = GoldStyle;
				}
				Hero->ActionLabelInk = FSlateColor(FLinearColor(FColor(0x3B, 0x27, 0x00)));
			}
		};

		Run(TEXT("UIE_FactoryLineRow"), TEXT("factory_producing"), RowWidth, [&](UUserWidget* W)
		{
			if (UWorldFactoryLineWidget* Line = Cast<UWorldFactoryLineWidget>(W))
			{
				Line->SetLineNumber(1);
				Line->SetProductionData(Box, FText::FromString(TEXT("토스터 오븐")), 0.13f, 38, 81, FTimespan::FromSeconds(299), 38.0 / 0.13);
			}
		});
		Run(TEXT("UIE_FactoryLineRow"), TEXT("factory_completed"), RowWidth, [&](UUserWidget* W)
		{
			if (UWorldFactoryLineWidget* Line = Cast<UWorldFactoryLineWidget>(W))
			{
				Line->SetLineNumber(2);
				Line->SetProductionData(Box, FText::FromString(TEXT("에어프라이어")), 0.13f, 243, 243, FTimespan::Zero(), 0.0);
				Line->SetCompleted(243);
			}
		});
		Run(TEXT("UIE_MineLineRow"), TEXT("mine_authored"), RowWidth, [](UUserWidget*) {});
		Run(TEXT("UIE_DrawerInfoRow"), TEXT("info_empty_line"), RowWidth, [](UUserWidget* W)
		{
			if (UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(W))
			{
				Row->SetRow(nullptr, FText::AsNumber(3), FText::FromString(TEXT("빈 라인")), false,
					FText::FromString(TEXT("제품을 고르면 생산을 시작합니다")), FText::GetEmpty(), EDrawerRowTone::Normal);
				Row->SetAction(FText::FromString(TEXT("생산 시작")));
				Row->SetLocked(false);
				Row->SetEmptyPlate(true);
			}
		});
		Run(TEXT("UIE_DrawerInfoRow"), TEXT("info_locked_line"), RowWidth, [](UUserWidget* W)
		{
			if (UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(W))
			{
				Row->SetRow(nullptr, FText::GetEmpty(), FText::FromString(TEXT("4번 라인")), false,
					FText::FromString(TEXT("강화의 라인 증설로 열립니다")), FText::GetEmpty(), EDrawerRowTone::Normal);
				Row->SetAction(FText::GetEmpty());
				Row->SetLocked(true);
			}
		});
		Run(TEXT("UIE_DrawerInfoRow"), TEXT("info_port_order"), RowWidth, [&](UUserWidget* W)
		{
			if (UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(W))
			{
				Row->SetRow(Box, FText::GetEmpty(), FText::FromString(TEXT("B 주문 · 에어프라이어")), false,
					FText::FromString(TEXT("80개 납품 · 보상 ×1.5")), FText::FromString(TEXT("납품 가능")), EDrawerRowTone::Good);
				Row->SetAction(FText::GetEmpty());
			}
		});
		RunWith(TEXT("UIE_DrawerHeroRow"), TEXT("hero_claim"), RowWidth, GoldHero, [&](UUserWidget* W)
		{
			if (UDrawerHeroRowWidget* Hero = Cast<UDrawerHeroRowWidget>(W))
			{
				Hero->SetSummary(FText::FromString(TEXT("전자 공장")), FText::FromString(TEXT("라인 3개 · 생산 중 1 · 완료 1")));
				Hero->SetAction(FText::FromString(TEXT("모두 받기")), EResourceType::Box, 243);
				Hero->SetFacilityTile(Glyph, FLinearColor(FColor(0xEE, 0x9F, 0x22)));
			}
		});
		RunWith(TEXT("UIE_DrawerHeroRow"), TEXT("hero_nothing"), RowWidth, GoldHero, [&](UUserWidget* W)
		{
			if (UDrawerHeroRowWidget* Hero = Cast<UDrawerHeroRowWidget>(W))
			{
				Hero->SetSummary(FText::FromString(TEXT("구리 광산")), FText::FromString(TEXT("라인 1개")));
				Hero->SetAction(FText::FromString(TEXT("모두 받기")), EResourceType::Box, 0);
				Hero->SetFacilityTile(nullptr, FLinearColor(FColor(0x8F, 0x60, 0x40)));
			}
		});
		Run(TEXT("UIE_DrawerHeroRow"), TEXT("hero_port_sell"), RowWidth, [&](UUserWidget* W)
		{
			if (UDrawerHeroRowWidget* Hero = Cast<UDrawerHeroRowWidget>(W))
			{
				Hero->SetSummary(FText::FromString(TEXT("싱가포르 무역항")), FText::FromString(TEXT("시세 ×1.2 · 주문을 먼저 채웁니다")));
				Hero->SetAction(FText::FromString(TEXT("모두 판매")), EResourceType::Money, 175000);
				Hero->SetFacilityTile(Box, FLinearColor(FColor(0x17, 0x84, 0x78)));
			}
		});
		Run(TEXT("UIE_DrawerGoodsTile"), TEXT("goods_tile"), 250.f, [&](UUserWidget* W)
		{
			if (UDrawerGoodsTileWidget* Tile = Cast<UDrawerGoodsTileWidget>(W))
			{
				Tile->SetGoods(Box, FText::FromString(TEXT("에어프라이어")), 243);
			}
		});
		Run(TEXT("UIE_DrawerPageToggle"), TEXT("toggle_dot"), 1112.f, [](UUserWidget* W)
		{
			if (UDrawerPageToggleWidget* Toggle = Cast<UDrawerPageToggleWidget>(W))
			{
				Toggle->SetProductionDot(true);
			}
		});

		TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetArrayField(TEXT("renders"), Index);
		Summary->SetNumberField(TEXT("failures"), Failures);
		Summary->SetStringField(TEXT("limitations"), TEXT("Designer-flag render: PreConstruct runs, game Construct/Tick and manager lookups do not; toggle selection is not applied (no button group)."));
		WriteJson(Summary, OutDir() / TEXT("render/index.json"));
		UE_LOG(LogTemp, Display, TEXT("[DrawerRows] RenderRows done renders=%d failures=%d"), Index.Num(), Failures);
	}

	// ── 서랍 전체(UI_CountryDetail) 디자이너 렌더 ― Task 7 저작 직후 눈으로 보는 확인용(게임 수명주기 없음) ──
	static const TCHAR* DrawerClassPath = TEXT("/Game/CompanyGrowth/UI/Panels/WorldMap/UI_CountryDetail.UI_CountryDetail_C");
	// 서랍 전용 강화 줄(UpgradeRow DT 행이 가리키는 것) ― 공용 UIE_UpgradeSlotHorizon 은 224 높이라 서랍 판에 안 들어간다
	static const TCHAR* UpgradeSlotClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/WorldMap/UIE_DrawerUpgradeSlot.UIE_DrawerUpgradeSlot_C");

	enum class EDrawerScene : uint8 { FactoryProduction, FactoryScroll, FactoryUpgrade, MineProduction, Port, PortEmpty };

	struct FDrawerRender
	{
		UWorld* World = nullptr;
		EWidgetDesignFlags Flags = EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct;
		TArray<UUserWidget*> Rows;
		TArray<TStrongObjectPtr<UUserWidget>> Keep;

		UUserWidget* Make(const FString& ClassPath, const FString& Name)
		{
			UClass* Cls = LoadObject<UClass>(nullptr, *ClassPath);
			if (!Cls) { return nullptr; }
			UUserWidget* W = NewObject<UUserWidget>(World, Cls, FName(*Name), RF_Transient);
			W->SetDesignerFlags(Flags);
			if (!W->Initialize()) { return nullptr; }
			Keep.Emplace(W);
			Rows.Add(W);
			return W;
		}
	};

	static FString RowClass(const TCHAR* Asset)
	{
		return FString(WorldMapElements) + Asset + TEXT(".") + Asset + TEXT("_C");
	}

	static void AddToList(UWidget* List, UWidget* Row, float RowGap = 16.f)
	{
		if (UVerticalBox* Box = Cast<UVerticalBox>(List))
		{
			if (UVerticalBoxSlot* BoxSlot = Box->AddChildToVerticalBox(Row))
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, RowGap));
			}
		}
	}

	static bool RenderDrawerScene(UWorld* World, EDrawerScene Scene, const FString& Label, float Width, TArray<TSharedPtr<FJsonValue>>& Index)
	{
		UClass* DrawerClass = LoadObject<UClass>(nullptr, DrawerClassPath);
		if (!DrawerClass) { return false; }
		FDrawerRender R;
		R.World = World;
		TStrongObjectPtr<UUserWidget> Panel(NewObject<UUserWidget>(World, DrawerClass, NAME_None, RF_Transient));
		Panel->SetDesignerFlags(R.Flags);
		if (!Panel->Initialize()) { return false; }
		auto FindNamed = [&Panel](const TCHAR* Name) { return Panel->GetWidgetFromName(FName(Name)); };
		UTexture2D* Box = Tex(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/box.box"));
		UTexture2D* Glyph = Tex(TEXT("/Game/CompanyGrowth/UI/Textures/Industry/T_IndustryGlyph_Electronics.T_IndustryGlyph_Electronics"));

		const bool bFactory = Scene == EDrawerScene::FactoryProduction || Scene == EDrawerScene::FactoryScroll || Scene == EDrawerScene::FactoryUpgrade;
		const bool bMine = Scene == EDrawerScene::MineProduction;
		const bool bPort = Scene == EDrawerScene::Port || Scene == EDrawerScene::PortEmpty;

		// 1) 목록 행 ― 게임이 런타임에 붙이는 것과 같은 WBP 를 같은 칸에
		TArray<UWorldFactoryLineWidget*> FactoryLines;
		TArray<UDrawerInfoRowWidget*> InfoRows;
		TArray<UUpgradeSlot*> UpgradeRows;
		UMineLineWidget* MineLine = nullptr;
		TArray<UDrawerGoodsTileWidget*> GoodsTiles;
		if (bFactory)
		{
			const int32 LineCount = Scene == EDrawerScene::FactoryScroll ? 5 : 2;
			for (int32 I = 0; I < LineCount; ++I)
			{
				if (UWorldFactoryLineWidget* Line = Cast<UWorldFactoryLineWidget>(R.Make(RowClass(TEXT("UIE_FactoryLineRow")), FString::Printf(TEXT("Line%d"), I + 1))))
				{
					AddToList(FindNamed(TEXT("FactoryLineList")), Line);
					FactoryLines.Add(Line);
				}
			}
			for (int32 I = 0; I < 2; ++I)
			{
				if (UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(R.Make(RowClass(TEXT("UIE_DrawerInfoRow")), FString::Printf(TEXT("Info%d"), I + 1))))
				{
					AddToList(FindNamed(TEXT("FactoryLineList")), Row);
					InfoRows.Add(Row);
				}
			}
			for (int32 I = 0; I < 5; ++I)
			{
				if (UUpgradeSlot* UpgradeRow = Cast<UUpgradeSlot>(R.Make(UpgradeSlotClassPath, FString::Printf(TEXT("Upgrade%d"), I + 1))))
				{
					AddToList(FindNamed(TEXT("FactoryEnhanceList")), UpgradeRow);
					UpgradeRows.Add(UpgradeRow);
				}
			}
		}
		if (bMine)
		{
			MineLine = Cast<UMineLineWidget>(R.Make(RowClass(TEXT("UIE_MineLineRow")), TEXT("MineLine1")));
			AddToList(FindNamed(TEXT("MineLineList")), MineLine);
			if (UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(R.Make(RowClass(TEXT("UIE_DrawerInfoRow")), TEXT("MineLocked"))))
			{
				AddToList(FindNamed(TEXT("MineLineList")), Row);
				InfoRows.Add(Row);
			}
		}
		if (bPort)
		{
			for (int32 I = 0; I < 2; ++I)
			{
				if (UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(R.Make(RowClass(TEXT("UIE_DrawerInfoRow")), FString::Printf(TEXT("Order%d"), I + 1))))
				{
					AddToList(FindNamed(TEXT("PortOrderList")), Row);
					InfoRows.Add(Row);
				}
			}
			UUniformGridPanel* GoodsGrid = Cast<UUniformGridPanel>(FindNamed(TEXT("PortGoodsList")));
			const int32 GoodsCount = Scene == EDrawerScene::Port ? 4 : 0;
			for (int32 I = 0; GoodsGrid && I < GoodsCount; ++I)
			{
				if (UDrawerGoodsTileWidget* GoodsTile = Cast<UDrawerGoodsTileWidget>(R.Make(RowClass(TEXT("UIE_DrawerGoodsTile")), FString::Printf(TEXT("Goods%d"), I + 1))))
				{
					if (UUniformGridSlot* GridSlot = GoodsGrid->AddChildToUniformGrid(GoodsTile, 0, I))
					{
						GridSlot->SetHorizontalAlignment(HAlign_Fill);
						GridSlot->SetVerticalAlignment(VAlign_Fill);
					}
					GoodsTiles.Add(GoodsTile);
				}
			}
		}

		// 2) 디자이너 플래그 → Slate → 저작 Visibility 복원(디자이너는 숨김을 Visible 로 덮는다)
		TArray<UWidget*> Widgets{Panel.Get()};
		GatherWidgets(Panel.Get(), Widgets);
		for (UUserWidget* Row : R.Rows)
		{
			Widgets.AddUnique(Row);
			GatherWidgets(Row, Widgets);
		}
		TArray<ESlateVisibility> Authored;
		for (UWidget* Widget : Widgets)
		{
			Widget->SetDesignerFlags(R.Flags);
			Authored.Add(Widget->GetVisibility());
		}
		TSharedPtr<SWidget> Slate = Panel->TakeWidget();
		ON_SCOPE_EXIT
		{
			Panel->ReleaseSlateResources(true);
			Slate.Reset();
		};
		for (int32 I = 0; I < Widgets.Num(); ++I)
		{
			Widgets[I]->SetVisibility(Authored[I]);
		}

		// 3) 런타임 세터 ― CountryDetailWidget 이 하는 것과 같은 표시
		if (UWidget* TabBar = FindNamed(TEXT("TabBar"))) { TabBar->SetVisibility(ESlateVisibility::Collapsed); }
		if (UWidgetSwitcher* MainSwitcher = Cast<UWidgetSwitcher>(FindNamed(TEXT("MainContentSwitcher"))))
		{
			if (UWidget* PagePlate = FindNamed(bFactory ? TEXT("FactoryPlate") : bMine ? TEXT("MinePlate") : TEXT("PortPlate")))
			{
				MainSwitcher->SetActiveWidget(PagePlate);
			}
		}
		const bool bUpgrade = Scene == EDrawerScene::FactoryUpgrade;
		for (const TCHAR* Name : { TEXT("FactoryPageSwitcher"), TEXT("MinePageSwitcher") })
		{
			if (UWidgetSwitcher* PageSwitcher = Cast<UWidgetSwitcher>(FindNamed(Name))) { PageSwitcher->SetActiveWidgetIndex(bUpgrade ? 1 : 0); }
		}
		if (UUserWidget* ToggleWidget = Cast<UUserWidget>(FindNamed(TEXT("PageToggle"))))
		{
			ToggleWidget->SetVisibility(bPort ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
			if (UDrawerPageToggleWidget* PageToggle = Cast<UDrawerPageToggleWidget>(ToggleWidget)) { PageToggle->SetProductionDot(bUpgrade); }
			// 그룹이 없는 디자이너라 선택 칸을 직접 고르고 잉크도 같은 규칙(선택 #1B2330 / 비선택 #8F9AAB)으로
			for (const TCHAR* SegName : { TEXT("ProductionSegment"), TEXT("UpgradeSegment") })
			{
				if (UButtonWidget* Segment = Cast<UButtonWidget>(ToggleWidget->GetWidgetFromName(FName(SegName))))
				{
					const bool bSelected = (FCString::Strcmp(SegName, TEXT("UpgradeSegment")) == 0) == bUpgrade;
					Segment->SetIsSelectable(true);
					Segment->SetIsSelected(bSelected, false);
					Segment->SetTextColor(FSlateColor(bSelected ? FLinearColor(FColor(0x1B, 0x23, 0x30)) : FLinearColor(FColor(0x8F, 0x9A, 0xAB))));
				}
			}
		}
		if (bFactory)
		{
			if (UDrawerHeroRowWidget* HeroRow = Cast<UDrawerHeroRowWidget>(FindNamed(TEXT("FactoryHero"))))
			{
				HeroRow->SetSummary(FText::FromString(TEXT("전자 공장")), FText::FromString(TEXT("라인 3개 · 생산 중 1 · 완료 1")));
				HeroRow->SetAction(FText::FromString(TEXT("모두 받기")), EResourceType::Box, 243);
				HeroRow->SetFacilityTile(Glyph, FLinearColor(FColor(0xEE, 0x9F, 0x22)));
			}
			for (int32 I = 0; I < FactoryLines.Num(); ++I)
			{
				UWorldFactoryLineWidget* Line = FactoryLines[I];
				Line->SetLineNumber(I + 1);
				if (I % 2 == 0)
				{
					Line->SetProductionData(Box, FText::FromString(TEXT("토스터 오븐")), 0.13f, 38, 81, FTimespan::FromSeconds(299), 38.0 / 0.13);
				}
				else
				{
					Line->SetProductionData(Box, FText::FromString(TEXT("에어프라이어")), 0.13f, 243, 243, FTimespan::Zero(), 0.0);
					Line->SetCompleted(243);
				}
			}
			if (InfoRows.Num() >= 2)
			{
				const int32 NextNumber = FactoryLines.Num() + 1;
				InfoRows[0]->SetRow(nullptr, FText::AsNumber(NextNumber), FText::FromString(TEXT("빈 라인")), false,
					FText::FromString(TEXT("제품을 고르면 생산을 시작합니다")), FText::GetEmpty(), EDrawerRowTone::Normal);
				InfoRows[0]->SetAction(FText::FromString(TEXT("생산 시작")));
				InfoRows[0]->SetLocked(false);
				InfoRows[0]->SetEmptyPlate(true);
				InfoRows[1]->SetRow(nullptr, FText::GetEmpty(), FText::FromString(FString::Printf(TEXT("%d번 라인"), NextNumber + 1)), true,
					FText::FromString(TEXT("강화의 라인 증설로 열립니다")), FText::GetEmpty(), EDrawerRowTone::Normal);
				InfoRows[1]->SetAction(FText::GetEmpty());
				InfoRows[1]->SetEmptyPlate(false);
				InfoRows[1]->SetLocked(true);
			}
			const TCHAR* UpgradeTitles[] = { TEXT("라인 증설"), TEXT("자동화"), TEXT("품질 관리"), TEXT("창고 증축"), TEXT("특급 가공") };
			for (int32 I = 0; I < UpgradeRows.Num(); ++I)
			{
				UpgradeRows[I]->SetRuntimePresentation(FText::FromString(UpgradeTitles[I]), FText::FromString(TEXT("생산 라인 증가")), EResourceType::Money);
				// 서랍과 같은 「최대」 라벨 주입(CountryDetailWidget::RebuildFactoryEnhancementSlots)
				UCostActionButtonWidget* UpgradeBtn = UpgradeRows[I]->GetUpgradeButton();
				if (UpgradeBtn)
				{
					UpgradeBtn->MaxLevelText = CountryDrawer::MaxLevelLabel();
				}
				// 0·1·3 = 가능, 2 = 돈 부족(가격만 빨강), 4 = 최대(잠금 덮개 없음)
				const bool bMax = I == 4;
				UpgradeRows[I]->UpdateInfo(bMax ? 10 : I, 3.f + I, 4.f + I, 100000LL * (I + 1), CountryDrawer::UpgradeSlotLocked(bMax), 10);
				if (I == 2 && UpgradeBtn)
				{
					UpgradeBtn->SetCanAfford(false);
				}
				// 서랍의 ApplyUpgradeLabelInk 와 같은 규칙(최대 = 차콜)
				UTextBlock* UpgradeLabel = UpgradeBtn && UpgradeBtn->WidgetTree
					? Cast<UTextBlock>(UpgradeBtn->WidgetTree->FindWidget(TEXT("BtnText"))) : nullptr;
				if (UpgradeLabel && UpgradeBtn->bOverrideTextStyle)
				{
					UpgradeLabel->SetColorAndOpacity(bMax ? FSlateColor(FLinearColor(FColor(0x1B, 0x23, 0x30))) : UpgradeBtn->BtnTextColor);
				}
			}
		}
		if (bMine)
		{
			if (UDrawerHeroRowWidget* HeroRow = Cast<UDrawerHeroRowWidget>(FindNamed(TEXT("MineHero"))))
			{
				HeroRow->SetSummary(FText::FromString(TEXT("구리 광산")), FText::FromString(TEXT("라인 1개 · 가득 참 1")));
				HeroRow->SetAction(FText::FromString(TEXT("모두 받기")), EResourceType::Box, 5);
				HeroRow->SetFacilityTile(nullptr, FLinearColor(FColor(0x8F, 0x60, 0x40)));
			}
			if (MineLine)
			{
				FMineLineState MineState;
				MineState.Resource = EResourceType::Copper;
				MineState.CurrentQty = 5;
				MineState.bSaturated = true;
				MineLine->SetLineState(ECountryType::Canada, MineState, 5, 0, 2.f);
			}
			if (InfoRows.Num() >= 1)
			{
				InfoRows[0]->SetRow(nullptr, FText::GetEmpty(), FText::FromString(TEXT("2번 라인")), true,
					FText::FromString(TEXT("강화의 라인 증설로 열립니다")), FText::GetEmpty(), EDrawerRowTone::Normal);
				InfoRows[0]->SetAction(FText::GetEmpty());
				InfoRows[0]->SetEmptyPlate(false);
				InfoRows[0]->SetLocked(true);
			}
		}
		if (bPort)
		{
			if (UDrawerHeroRowWidget* HeroRow = Cast<UDrawerHeroRowWidget>(FindNamed(TEXT("PortHero"))))
			{
				HeroRow->SetSummary(FText::FromString(TEXT("싱가포르 무역항")), FText::FromString(TEXT("시세 ×1.2 · 주문을 먼저 채웁니다")));
				HeroRow->SetAction(FText::FromString(TEXT("모두 판매")), EResourceType::Money, Scene == EDrawerScene::Port ? 175000 : 0);
				HeroRow->SetFacilityTile(Box, FLinearColor(FColor(0x17, 0x84, 0x78)));
			}
			const TCHAR* OrderTexts[][3] = { { TEXT("B 주문 · 에어프라이어"), TEXT("80개 납품 · 보상 ×1.5"), TEXT("납품 가능") },
				{ TEXT("C 주문 · 토스터"), TEXT("120개 납품 · 보상 ×1.2"), TEXT("재고 부족") } };
			for (int32 I = 0; I < InfoRows.Num() && I < 2; ++I)
			{
				InfoRows[I]->SetRow(Box, FText::GetEmpty(), FText::FromString(OrderTexts[I][0]), false, FText::FromString(OrderTexts[I][1]),
					FText::FromString(OrderTexts[I][2]), I == 0 ? EDrawerRowTone::Good : EDrawerRowTone::Muted);
				InfoRows[I]->SetAction(FText::GetEmpty());
			}
			const TCHAR* GoodsNames[] = { TEXT("에어프라이어"), TEXT("토스터 오븐"), TEXT("스마트 전구"), TEXT("토스터") };
			for (int32 I = 0; I < GoodsTiles.Num(); ++I)
			{
				GoodsTiles[I]->SetGoods(Box, FText::FromString(GoodsNames[I]), 243 - I * 60);
			}
			// RefreshPortPage 와 같은 규칙: 칸이 없으면 격자를 접고, 주문이 없으면 「주문」을 접는다
			if (UWidget* GoodsGrid = FindNamed(TEXT("PortGoodsList")))
			{
				GoodsGrid->SetVisibility(GoodsTiles.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			}
			if (UWidget* EmptyText = FindNamed(TEXT("PortEmptyText")))
			{
				EmptyText->SetVisibility(GoodsTiles.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
		}

		// 4) 그리기 + 지오메트리(행 안 위젯은 「행이름/위젯이름」)
		const FVector2D DrawSize(Width, 1440.f);
		Slate->SlatePrepass(1.f);
		// 디자이너엔 막대 가시성 이벤트가 없다 ― 넘치는 장면만 CountryDetailWidget::ApplyLineScrollBarInset 과 같은 식으로 내민다
		if (Scene == EDrawerScene::FactoryScroll)
		{
			if (UScrollBox* LineScroll = Cast<UScrollBox>(FindNamed(TEXT("FactoryLineScroll"))))
			{
				if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(LineScroll->Slot))
				{
					const FMargin BarPadding = LineScroll->GetScrollbarPadding();
					const FScrollBarStyle& BarStyle = LineScroll->GetWidgetBarStyle();
					const float BarWidth = FMath::Max3(static_cast<float>(LineScroll->GetScrollbarThickness().X),
						static_cast<float>(BarStyle.VerticalTopSlotImage.ImageSize.X), static_cast<float>(BarStyle.VerticalBottomSlotImage.ImageSize.X));
					ScrollSlot->SetPadding(FMargin(0.f, 0.f, -(BarWidth + BarPadding.Left + BarPadding.Right), 0.f));
				}
			}
			Slate->SlatePrepass(1.f);
		}
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
		if (!Target.IsValid()) { return false; }
		ON_SCOPE_EXIT
		{
			Target->ReleaseResource();
			FlushRenderingCommands();
		};
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Slate.ToSharedRef(), 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		const FString RenderDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldMapUIOverhaul/Task7/render"));
		IFileManager::Get().MakeDirectory(*RenderDir, true);
		const FString Base = RenderDir / Label;
		FImage Image;
		TArray64<uint8> Png;
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), Image)) { return false; }
		Image.GammaSpace = EGammaSpace::sRGB;
		if (!FImageUtils::CompressImage(Png, TEXT("png"), Image) || !FFileHelper::SaveArrayToFile(Png, *(Base + TEXT(".png")))) { return false; }

		const FGeometry RootGeometry = Panel->GetRootWidget()->GetCachedGeometry();
		TArray<TSharedPtr<FJsonValue>> Rects;
		for (UWidget* Widget : Widgets)
		{
			if (Widget == Panel.Get()) { continue; }
			const FGeometry& G = Widget->GetCachedGeometry();
			const FVector2D Pos = RootGeometry.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D Size = G.GetLocalSize();
			const UUserWidget* OwnerWidget = Widget->GetTypedOuter<UUserWidget>();
			FString Name = Widget->GetName();
			if (OwnerWidget && OwnerWidget != Panel.Get())
			{
				const UUserWidget* RowOwner = OwnerWidget;
				while (RowOwner && !R.Rows.Contains(const_cast<UUserWidget*>(RowOwner)) && RowOwner->GetTypedOuter<UUserWidget>() && RowOwner->GetTypedOuter<UUserWidget>() != Panel.Get())
				{
					RowOwner = RowOwner->GetTypedOuter<UUserWidget>();
				}
				Name = (RowOwner ? RowOwner->GetName() : OwnerWidget->GetName()) + TEXT("/") + Name;
			}
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Name);
			Row->SetStringField(TEXT("visibility"), StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility())));
			Row->SetBoolField(TEXT("shown"), Widget->IsVisible() && Size.X > 0.f);
			Row->SetNumberField(TEXT("x"), FMath::IsFinite(Pos.X) ? Pos.X : -1.0);
			Row->SetNumberField(TEXT("y"), FMath::IsFinite(Pos.Y) ? Pos.Y : -1.0);
			Row->SetNumberField(TEXT("w"), FMath::IsFinite(Size.X) ? Size.X : -1.0);
			Row->SetNumberField(TEXT("h"), FMath::IsFinite(Size.Y) ? Size.Y : -1.0);
			Rects.Add(MakeShared<FJsonValueObject>(Row));
		}
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("label"), Label);
		Report->SetNumberField(TEXT("width"), DrawSize.X);
		Report->SetNumberField(TEXT("height"), DrawSize.Y);
		Report->SetArrayField(TEXT("widgets"), Rects);
		WriteJson(Report, Base + TEXT(".json"));
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("label"), Label);
		Entry->SetNumberField(TEXT("width"), DrawSize.X);
		Index.Add(MakeShared<FJsonValueObject>(Entry));
		return true;
	}

	static void RenderDrawer(const TArray<FString>&)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Error, TEXT("[DrawerRows] RenderDrawer needs the editor world with PIE stopped"));
			return;
		}
		const TPair<EDrawerScene, const TCHAR*> DrawerScenes[] = {
			{ EDrawerScene::FactoryProduction, TEXT("factory_production") }, { EDrawerScene::FactoryScroll, TEXT("factory_scroll") },
			{ EDrawerScene::FactoryUpgrade, TEXT("factory_upgrade") }, { EDrawerScene::MineProduction, TEXT("mine_production") },
			{ EDrawerScene::Port, TEXT("port") }, { EDrawerScene::PortEmpty, TEXT("port_empty") } };
		TArray<TSharedPtr<FJsonValue>> Index;
		int32 Failures = 0;
		for (const float Width : { 2304.f, 3120.f })
		{
			for (const TPair<EDrawerScene, const TCHAR*>& Scene : DrawerScenes)
			{
				const FString Label = FString::Printf(TEXT("%s_%d"), Scene.Value, FMath::RoundToInt(Width));
				if (!RenderDrawerScene(World, Scene.Key, Label, Width, Index)) { ++Failures; }
			}
		}
		TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetArrayField(TEXT("renders"), Index);
		Summary->SetNumberField(TEXT("failures"), Failures);
		Summary->SetStringField(TEXT("limitations"), TEXT("Designer-flag render of UI_CountryDetail: no NativeConstruct, no managers; rows are injected fixtures; instant finish shows the locked face because cost is 0 without managers."));
		WriteJson(Summary, FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldMapUIOverhaul/Task7/render/index.json")));
		UE_LOG(LogTemp, Display, TEXT("[DrawerRows] RenderDrawer done renders=%d failures=%d"), Index.Num(), Failures);
	}

	// DT_WidgetClass 의 DrawerOnePage_UpgradeRow(EWidgetType::UpgradeRow ― 서랍만 쓴다) 한 행의 WidgetClass 만 서랍 전용 강화 줄로
	static void RepointUpgradeRow(const TArray<FString>&)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldMapUIOverhaul/Task7"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), false);
		Report->SetStringField(TEXT("error"), TEXT("operation_not_completed"));
		ON_SCOPE_EXIT { WriteJson(Report, Dir / TEXT("dt_upgrade_row.json")); };
		if (!GEditor || GEditor->PlayWorld)
		{
			Report->SetStringField(TEXT("error"), TEXT("editor_only_pie_stopped"));
			return;
		}
		UDataTable* Table = LoadObject<UDataTable>(nullptr, TableAssetPath);
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct())
		{
			Report->SetStringField(TEXT("error"), TEXT("table_missing_or_wrong_row_struct"));
			return;
		}
		if (Table->GetOutermost()->IsDirty())
		{
			Report->SetStringField(TEXT("error"), TEXT("table_has_unsaved_edits"));
			return;
		}
		UClass* VariantClass = LoadObject<UClass>(nullptr, UpgradeSlotClassPath);
		if (!VariantClass || !VariantClass->IsChildOf(UUpgradeSlot::StaticClass()))
		{
			Report->SetStringField(TEXT("error"), VariantClass ? TEXT("variant_wrong_parent") : TEXT("variant_missing"));
			return;
		}
		const FName RowKey(TEXT("DrawerOnePage_UpgradeRow"));
		TMap<FName, FWidgetDataTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FWidgetDataTable*>(Pair.Value));
		}
		Report->SetNumberField(TEXT("before_count"), Before.Num());
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key != RowKey && Pair.Value.WidgetType == EWidgetType::UpgradeRow)
			{
				Report->SetStringField(TEXT("error"), TEXT("upgrade_row_type_also_in:") + Pair.Key.ToString());
				return;
			}
		}
		FWidgetDataTable* Row = Table->FindRow<FWidgetDataTable>(RowKey, TEXT("RepointUpgradeRow"), false);
		if (!Row || Row->WidgetType != EWidgetType::UpgradeRow)
		{
			Report->SetStringField(TEXT("error"), Row ? TEXT("row_has_other_type") : TEXT("row_missing"));
			return;
		}
		Report->SetStringField(TEXT("class_before"), GetPathNameSafe(Row->WidgetClass.Get()));
		if (Row->WidgetClass == VariantClass)
		{
			Report->SetBoolField(TEXT("success"), true);
			Report->SetBoolField(TEXT("already_repointed"), true);
			Report->SetStringField(TEXT("error"), TEXT(""));
			return;
		}
		const TSubclassOf<UUserWidget> Previous = Row->WidgetClass;
		Row->WidgetClass = VariantClass;
		bool bOthersPreserved = Table->GetRowMap().Num() == Before.Num();
		for (const TPair<FName, FWidgetDataTable>& Pair : Before)
		{
			if (Pair.Key == RowKey) { continue; }
			const FWidgetDataTable* Now = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("RepointUpgradeRow"), false);
			bOthersPreserved &= Now && RowsEqual(*Now, Pair.Value);
		}
		Report->SetBoolField(TEXT("other_rows_preserved"), bOthersPreserved);
		if (!bOthersPreserved)
		{
			Row->WidgetClass = Previous;
			Report->SetStringField(TEXT("error"), TEXT("rows_not_as_expected_not_saved"));
			return;
		}
		UPackage* Package = Table->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
		if (!bSaved)
		{
			Row->WidgetClass = Previous;
			Package->SetDirtyFlag(false);
		}
		Report->SetNumberField(TEXT("after_count"), Table->GetRowMap().Num());
		Report->SetStringField(TEXT("class_after"), GetPathNameSafe(Row->WidgetClass.Get()));
		Report->SetBoolField(TEXT("saved"), bSaved);
		Report->SetBoolField(TEXT("success"), bSaved);
		Report->SetStringField(TEXT("error"), bSaved ? TEXT("") : TEXT("save_failed_restored"));
		UE_LOG(LogTemp, Log, TEXT("[DrawerRows] UpgradeRow -> %s saved=%d"), *GetNameSafe(Row->WidgetClass.Get()), bSaved ? 1 : 0);
	}

	static FAutoConsoleCommand RepointUpgradeRowCommand(
		TEXT("CGR.WorldMapUI.RepointUpgradeRow"),
		TEXT("Point the drawer-only DT_WidgetClass UpgradeRow row at UIE_DrawerUpgradeSlot; other rows are proven unchanged before saving."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RepointUpgradeRow));

	static FAutoConsoleCommand RenderDrawerCommand(
		TEXT("CGR.WorldMapUI.RenderDrawer"),
		TEXT("Render UI_CountryDetail with fixture rows at 2304 and 3120 x 1440 to Saved/WorldMapUIOverhaul/Task7/render."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RenderDrawer));

	static FAutoConsoleCommand RegisterGoodsTileCommand(
		TEXT("CGR.WorldMapUI.RegisterGoodsTile"),
		TEXT("Add the DrawerGoodsTile row to DT_WidgetClass natively; other rows are proven unchanged before saving."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterGoodsTile));

	static FAutoConsoleCommand RenderRowsCommand(
		TEXT("CGR.WorldMapUI.RenderRows"),
		TEXT("Render the country drawer row widgets with state fixtures to Saved/WorldMapUIOverhaul/Task6/render."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RenderRows));
}

#endif
