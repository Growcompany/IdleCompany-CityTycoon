#include "CoreMinimal.h"

#if WITH_EDITOR

// 본사 운영 패널 3탭 저작 — 목업 대응표 = docs/superpowers/specs/2026-10-11-hq-panel-3tab-design.md §5
// 콘솔: CGR.HQ.DumpTree <에셋경로> / CGR.HQ.CreateDesaturateMaterial / CGR.HQ.AssignPanelIcons / CGR.HQ.RegisterWidgets / CGR.HQ.BuildWidgets

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Table/CompanyInfoTable.h"
#include "Table/HQLevelData.h"
#include "Table/WidgetDataTable.h"
#include "Enum/WidgetType.h"
#include "UI/Element/Building/BuildingSlotHorizonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/HQ/HQConditionCardWidget.h"
#include "UI/Element/HQ/HQIndustryCardWidget.h"
#include "UI/Element/HQ/HQKpiCellWidget.h"
#include "UI/Element/HQ/HQLevelStepWidget.h"
#include "UI/Element/HQ/HQRewardSlotWidget.h"
#include "UI/Panel/HQManagePanelWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace CGRHQAuthoring
{
	// ---------- 경로 ----------
	static const TCHAR* HQPanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_HQManagePanel.UI_HQManagePanel");
	static const TCHAR* CondPath = TEXT("/Game/CompanyGrowth/UI/Elements/Common/UIE_CondStatRow.UIE_CondStatRow");
	static const TCHAR* SlotPath = TEXT("/Game/CompanyGrowth/UI/Elements/Building/UIE_BuildingSlotHorizon.UIE_BuildingSlotHorizon");
	static const TCHAR* HQElementFolder = TEXT("/Game/CompanyGrowth/UI/Elements/HQ");
	static const TCHAR* DesaturatePackage = TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_IconDesaturate");
	static const TCHAR* CostButtonClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Button/UIE_CostActionButton_Horizontal.UIE_CostActionButton_Horizontal_C");
	static const TCHAR* HudChipPath = TEXT("/Game/CompanyGrowth/UI/Elements/Common/UIE_Resource_Hud.UIE_Resource_Hud");
	static const TCHAR* WidgetTablePath = TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass");
	static const TCHAR* CompanyTablePath = TEXT("/Game/CompanyGrowth/Table/Company/DT_CompanyInfo.DT_CompanyInfo");
	static const TCHAR* HQLevelTablePath = TEXT("/Game/CompanyGrowth/Table/Company/DT_HQLevel.DT_HQLevel");

	// ---------- 공통 ----------
	static FLinearColor Hex(uint32 RGB, float Alpha = 1.f)
	{
		return FLinearColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255)).CopyWithNewOpacity(Alpha);
	}

	static bool CanAuthor()
	{
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Requires the editor game thread with PIE/SIE stopped."));
			return false;
		}
		return true;
	}

	static bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
		UE_LOG(LogTemp, Display, TEXT("[HQAuthor] Save %s -> %s"), *Package->GetName(), bSaved ? TEXT("OK") : TEXT("FAILED"));
		return bSaved;
	}

	static bool Compile(UWidgetBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Log;
		Log.bSilentMode = false;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
		for (const TSharedRef<FTokenizedMessage>& Message : Log.Messages)
		{
			if (Message->GetSeverity() == EMessageSeverity::Error)
			{
				UE_LOG(LogTemp, Error, TEXT("[HQAuthor] %s: %s"), *Blueprint->GetName(), *Message->ToText().ToString());
			}
		}
		const bool bOk = Log.NumErrors == 0 && Blueprint->Status != BS_Error;
		UE_LOG(LogTemp, Display, TEXT("[HQAuthor] Compile %s -> %s (errors %d, warnings %d)"),
			*Blueprint->GetName(), bOk ? TEXT("OK") : TEXT("FAILED"), Log.NumErrors, Log.NumWarnings);
		return bOk;
	}

	template <typename T>
	static T* LoadAsset(const TCHAR* Path)
	{
		T* Asset = LoadObject<T>(nullptr, Path);
		if (!Asset)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Missing asset %s"), Path);
		}
		return Asset;
	}

	// 위젯을 트리에서 빼고 이름을 비운다 — 같은 이름 재생성 충돌(에디터 즉사)을 막는다
	static void Retire(UWidgetTree* Tree, UWidget* Widget)
	{
		if (!Widget) return;
		Tree->RemoveWidget(Widget);
		const FName Discard = MakeUniqueObjectName(GetTransientPackage(), Widget->GetClass(),
			*FString::Printf(TEXT("%s_Retired_%s"), *Widget->GetName(), *FGuid::NewGuid().ToString(EGuidFormats::Short)));
		Widget->Rename(*Discard.ToString(), GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
	}

	static void RetireSubtree(UWidgetTree* Tree, UWidget* Root, const TSet<UWidget*>& Keep)
	{
		TArray<UWidget*> Descendants;
		UWidgetTree::GetChildWidgets(Root, Descendants);
		for (UWidget* Widget : Descendants)
		{
			if (Widget && !Keep.Contains(Widget)) Retire(Tree, Widget);
		}
		if (!Keep.Contains(Root)) Retire(Tree, Root);
	}

	// ---------- 브러시 ----------
	static FSlateBrush Rounded(const FLinearColor& Fill, const FVector4& Radii,
		const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radii, FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.Margin = FMargin(0.f);
		return Brush;
	}

	static FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		return Rounded(Fill, FVector4(Radius, Radius, Radius, Radius), Outline, OutlineWidth);
	}

	static FSlateBrush RoundedTexture(UObject* Resource, const FVector2D& Size, const FVector4& Radii, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush = Rounded(Tint, Radii);
		Brush.SetResourceObject(Resource);
		Brush.ImageSize = Size;
		return Brush;
	}

	static FSlateBrush TextureBrush(UObject* Resource, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Resource);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	}

	static FSlateBrush SolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.TintColor = FSlateColor(Color);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	}

	// ---------- 자산 ----------
	struct FAssets
	{
		UObject* NexonBold = nullptr;
		UObject* NexonRegular = nullptr;
		UObject* Pretendard = nullptr;
		UTexture2D* Plaque = nullptr;
		UTexture2D* RandomReward = nullptr;
		UTexture2D* WorldMapReward = nullptr;
		UTexture2D* SheenV = nullptr;
		UTexture2D* Halftone = nullptr;
		UTexture2D* GlowOval = nullptr;
		UTexture2D* Gradient0 = nullptr;
		UTexture2D* Money = nullptr;
		UTexture2D* Building = nullptr;
		UTexture2D* MarketCap = nullptr;
		UTexture2D* Employees = nullptr;
		UTexture2D* Check = nullptr;
		UTexture2D* Lock = nullptr;
		UObject* NoisePlate = nullptr;
		UMaterialInterface* Desaturate = nullptr;
		FSlateBrush WellBrush;

		bool Load()
		{
			NexonBold = LoadAsset<UObject>(TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"));
			NexonRegular = LoadAsset<UObject>(TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"));
			Pretendard = LoadAsset<UObject>(TEXT("/Game/CompanyGrowth/Font/Pretendard/F_Pretendard.F_Pretendard"));
			Plaque = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/HQ/T_HQLevelPlaque.T_HQLevelPlaque"));
			// 승인 목업(보상 칸안)의 랜덤 보상·세계지도 그림 = 기존 컬러 아이콘 팩
			RandomReward = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/CasualPack/Icons/Colored/Treasure_Box.Treasure_Box"));
			WorldMapReward = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/CasualPack/Icons/Colored/Map.Map"));
			SheenV = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/HQ/T_HQ_SheenV.T_HQ_SheenV"));
			Halftone = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/HQ/T_HQ_HeroHalftone.T_HQ_HeroHalftone"));
			GlowOval = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/Gradation/Glow_Oval.Glow_Oval"));
			Gradient0 = LoadAsset<UTexture2D>(TEXT("/Engine/EngineResources/GradientTexture0.GradientTexture0"));
			Money = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/MoneyIcon.MoneyIcon"));
			Building = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Resource_BuildingIcon2.Resource_BuildingIcon2"));
			MarketCap = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Report/T_UIIcon_Report_MarketCap.T_UIIcon_Report_MarketCap"));
			Employees = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/HQ/T_UIIcon_HQEmployeesWhite.T_UIIcon_HQEmployeesWhite"));
			// 34 원판 위 17px 체크 — 가는 메뉴 체크는 검정 테가 회색으로 번져 흐려진다, 굵은 CasualPack 체크(기획 보드 단계 탭과 같은 것)
			Check = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/CasualPack/Icons/B_W/Actions/Checkmark.Checkmark"));
			Lock = LoadAsset<UTexture2D>(TEXT("/Game/CompanyGrowth/UI/Textures/lock.lock"));
			NoisePlate = LoadAsset<UObject>(TEXT("/Game/CompanyGrowth/UI/Materials/MI_UI_NoiseTile_Plate.MI_UI_NoiseTile_Plate"));
			Desaturate = LoadAsset<UMaterialInterface>(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_IconDesaturate.M_UI_IconDesaturate"));

			// HUD 자원 칩의 어두운 아이콘 칸을 그대로 쓴다 (사용자 지시: 기존 것과 맞춘다)
			bool bWell = false;
			if (UWidgetBlueprint* Hud = LoadAsset<UWidgetBlueprint>(HudChipPath))
			{
				if (UWidget* Well = Hud->WidgetTree ? Hud->WidgetTree->FindWidget(TEXT("IconWell")) : nullptr)
				{
					if (UImage* WellImage = Cast<UImage>(Well)) { WellBrush = WellImage->GetBrush(); bWell = true; }
					else if (UBorder* WellBorder = Cast<UBorder>(Well)) { WellBrush = WellBorder->Background; bWell = true; }
				}
			}
			if (!bWell) UE_LOG(LogTemp, Warning, TEXT("[HQAuthor] UIE_Resource_Hud.IconWell brush not found"));

			return NexonBold && NexonRegular && Pretendard && Plaque && RandomReward && WorldMapReward && SheenV && Halftone && GlowOval && Gradient0
				&& Money && Building && MarketCap && Employees && Check && Lock && NoisePlate && Desaturate;
		}

		// HUD 칩의 아이콘 칸은 어두운 칩 위 흰 7.5% 라 흰 카드에선 사라진다 — 화면에 보이는 합성색 그대로 불투명하게
		FSlateBrush Well(float Size) const
		{
			FSlateBrush Brush = Rounded(Hex(0x1A1E24), Size * 0.23f, Hex(0x0E1116), 3.f);
			Brush.ImageSize = FVector2D(Size, Size);
			return Brush;
		}
	};

	// ---------- 위젯 생성 ----------
	struct FBuilder
	{
		UWidgetTree* Tree = nullptr;
		const FAssets* A = nullptr;

		template <typename T>
		T* Make(FName Name)
		{
			if (!Name.IsNone() && StaticFindObjectFast(nullptr, Tree, Name))
			{
				UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Name already used in tree: %s"), *Name.ToString());
				return nullptr;
			}
			return Tree->ConstructWidget<T>(T::StaticClass(), Name);
		}

		UUserWidget* MakeUser(UClass* Class, FName Name)
		{
			if (StaticFindObjectFast(nullptr, Tree, Name))
			{
				UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Name already used in tree: %s"), *Name.ToString());
				return nullptr;
			}
			return Tree->ConstructWidget<UUserWidget>(Class, Name);
		}

		FSlateFontInfo Font(UObject* FontObject, const TCHAR* Typeface, int32 Size, int32 OutlineSize = 0, const FLinearColor& OutlineColor = FLinearColor::Black) const
		{
			FSlateFontInfo Info;
			Info.FontObject = FontObject;
			Info.TypefaceFontName = FName(Typeface);
			// 목업은 캔버스 px, Slate 폰트는 96DPI 포인트(실제 em = Size x 1.333) — 0.75 로 환산하고 모바일 하한(라벨 24·캡션 20)은 지킨다
			Info.Size = FMath::Max(FMath::RoundToInt(Size * 0.75f), Size >= 30 ? 24 : 20);
			Info.LetterSpacing = 0;
			if (OutlineSize > 0)
			{
				Info.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(OutlineSize * 0.75f));
				Info.OutlineSettings.OutlineColor = OutlineColor;
			}
			return Info;
		}
		FSlateFontInfo Bold(int32 Size) const { return Font(A->NexonBold, TEXT("Default"), Size); }
		FSlateFontInfo Regular(int32 Size) const { return Font(A->NexonRegular, TEXT("Default"), Size); }
		FSlateFontInfo Num(int32 Size, int32 OutlineSize = 0, const FLinearColor& OutlineColor = FLinearColor::Black) const { return Font(A->Pretendard, TEXT("SemiBold"), Size, OutlineSize, OutlineColor); }
		FSlateFontInfo NumMedium(int32 Size) const { return Font(A->Pretendard, TEXT("Medium"), Size); }

		UCommonTextBlock* Text(FName Name, const FSlateFontInfo& FontInfo, const FLinearColor& Color, const FText& Content = FText::GetEmpty(),
			ETextJustify::Type Justify = ETextJustify::Left, bool bWrap = false)
		{
			UCommonTextBlock* Block = Make<UCommonTextBlock>(Name);
			if (!Block) return nullptr;
			// 라이트 판 위 글자는 스타일 슬롯을 비우고 폰트·잉크를 직접 넣는다 (카탈로그 §1.2)
			Block->SetStyle(nullptr);
			Block->SetFont(FontInfo);
			Block->SetColorAndOpacity(FSlateColor(Color));
			Block->SetJustification(Justify);
			Block->SetAutoWrapText(bWrap);
			Block->SetText(Content);
			Block->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			return Block;
		}

		UBorder* Border(FName Name, const FSlateBrush& Brush, const FMargin& Padding = FMargin(0.f),
			EHorizontalAlignment H = HAlign_Fill, EVerticalAlignment V = VAlign_Fill)
		{
			UBorder* Result = Make<UBorder>(Name);
			if (!Result) return nullptr;
			Result->SetBrush(Brush);
			Result->SetBrushColor(FLinearColor::White);
			Result->SetPadding(Padding);
			Result->SetHorizontalAlignment(H);
			Result->SetVerticalAlignment(V);
			Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			return Result;
		}

		UImage* Image(FName Name, const FSlateBrush& Brush, const FLinearColor& Color = FLinearColor::White)
		{
			UImage* Result = Make<UImage>(Name);
			if (!Result) return nullptr;
			Result->SetBrush(Brush);
			Result->SetColorAndOpacity(Color);
			Result->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Result;
		}

		USizeBox* Size(FName Name, float Width = 0.f, float Height = 0.f)
		{
			USizeBox* Result = Make<USizeBox>(Name);
			if (!Result) return nullptr;
			if (Width > 0.f) Result->SetWidthOverride(Width);
			if (Height > 0.f) Result->SetHeightOverride(Height);
			Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			return Result;
		}

		UVerticalBox* VBox(FName Name) { UVerticalBox* R = Make<UVerticalBox>(Name); if (R) R->SetVisibility(ESlateVisibility::SelfHitTestInvisible); return R; }
		UHorizontalBox* HBox(FName Name) { UHorizontalBox* R = Make<UHorizontalBox>(Name); if (R) R->SetVisibility(ESlateVisibility::SelfHitTestInvisible); return R; }
		UOverlay* Overlay(FName Name) { UOverlay* R = Make<UOverlay>(Name); if (R) R->SetVisibility(ESlateVisibility::SelfHitTestInvisible); return R; }

		// 두께(립) + 면(테두리) 2층 — 목업 .card = inset 3 테두리 + 0 6px 0 립
		UBorder* Card(FName LipName, FName FaceName, const FVector4& Radii, const FLinearColor& Face, const FLinearColor& Rim,
			const FLinearColor& Lip, UBorder*& OutFace, const FMargin& FacePadding = FMargin(3.f), float RimWidth = 3.f, float LipHeight = 6.f)
		{
			UBorder* LipBorder = Border(LipName, Rounded(Lip, Radii), FMargin(0.f, 0.f, 0.f, LipHeight));
			OutFace = Border(FaceName, Rounded(Face, Radii, Rim, RimWidth), FacePadding);
			if (LipBorder && OutFace) LipBorder->SetContent(OutFace);
			return LipBorder;
		}
	};

	static UVerticalBoxSlot* AddV(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.f), bool bFill = false,
		EHorizontalAlignment H = HAlign_Fill, EVerticalAlignment V = VAlign_Fill)
	{
		if (!Parent || !Child) return nullptr;
		UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Child);
		BoxSlot->SetPadding(Padding);
		BoxSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(H);
		BoxSlot->SetVerticalAlignment(V);
		return BoxSlot;
	}

	static UHorizontalBoxSlot* AddH(UHorizontalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.f), bool bFill = false,
		EHorizontalAlignment H = HAlign_Fill, EVerticalAlignment V = VAlign_Fill)
	{
		if (!Parent || !Child) return nullptr;
		UHorizontalBoxSlot* BoxSlot = Parent->AddChildToHorizontalBox(Child);
		BoxSlot->SetPadding(Padding);
		BoxSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(H);
		BoxSlot->SetVerticalAlignment(V);
		return BoxSlot;
	}

	static UOverlaySlot* AddO(UOverlay* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.f),
		EHorizontalAlignment H = HAlign_Fill, EVerticalAlignment V = VAlign_Fill)
	{
		if (!Parent || !Child) return nullptr;
		UOverlaySlot* OverlaySlot = Parent->AddChildToOverlay(Child);
		OverlaySlot->SetPadding(Padding);
		OverlaySlot->SetHorizontalAlignment(H);
		OverlaySlot->SetVerticalAlignment(V);
		return OverlaySlot;
	}

	static void SetSizeChild(USizeBox* Parent, UWidget* Child, EHorizontalAlignment H = HAlign_Fill, EVerticalAlignment V = VAlign_Fill)
	{
		if (!Parent || !Child) return;
		if (USizeBoxSlot* BoxSlot = Cast<USizeBoxSlot>(Parent->SetContent(Child)))
		{
			BoxSlot->SetHorizontalAlignment(H);
			BoxSlot->SetVerticalAlignment(V);
		}
	}

	// 트랙(Border) 안에 진행 바 — 바 자체의 BorderPadding 인셋은 채움을 바 원점에서 그린 뒤 인셋 경계로 잘라 시작 끝 라운드를 지운다
	static UWidget* MakeBar(FBuilder& B, FName Name, const FLinearColor& Track, const FLinearColor& TrackRim, float RimWidth,
		float Radius, float Inset)
	{
		UBorder* TrackBorder = B.Border(*FString::Printf(TEXT("%sTrack"), *Name.ToString()), Rounded(Track, Radius, TrackRim, RimWidth), FMargin(Inset));
		UProgressBar* Bar = B.Make<UProgressBar>(Name);
		if (!TrackBorder || !Bar) return nullptr;
		FProgressBarStyle Style;
		Style.BackgroundImage.DrawAs = ESlateBrushDrawType::NoDrawType;
		// 채움 = GradientTexture0 + 진행 색 틴트(카탈로그 §4-A), 끝 둥글게 = Scale 채움
		Style.FillImage = RoundedTexture(B.A->Gradient0, FVector2D(32.f, 32.f), FVector4(Radius - Inset, Radius - Inset, Radius - Inset, Radius - Inset));
		Style.MarqueeImage = Style.FillImage;
		Bar->SetWidgetStyle(Style);
		Bar->SetBarFillType(EProgressBarFillType::LeftToRight);
		Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
		Bar->SetBorderPadding(FVector2D::ZeroVector);
		Bar->SetFillColorAndOpacity(FLinearColor(0.047f, 0.328f, 0.745f));
		Bar->SetPercent(0.6f);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		TrackBorder->SetContent(Bar);
		TrackBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		return TrackBorder;
	}

	// 초록 원 + 체크 — 달성 띠·해금 기록 칸 공용
	static UWidget* MakeCheck(FBuilder& B, FName Prefix)
	{
		USizeBox* Box = B.Size(*FString::Printf(TEXT("%sBox"), *Prefix.ToString()), 34.f, 34.f);
		UOverlay* Layer = B.Overlay(*FString::Printf(TEXT("%sLayer"), *Prefix.ToString()));
		SetSizeChild(Box, Layer);
		AddO(Layer, B.Image(*FString::Printf(TEXT("%sDisc"), *Prefix.ToString()), Rounded(Hex(0x47A22A), 17.f)));
		AddO(Layer, B.Image(*FString::Printf(TEXT("%sGlyph"), *Prefix.ToString()), TextureBrush(B.A->Check, FVector2D(25.f, 20.f))),
			FMargin(0.f), HAlign_Center, VAlign_Center);
		return Box;
	}

	// 섹션 제목 — 목업 .sec-t (36 Bold, 높이 48)
	static UWidget* SectionTitle(FBuilder& B, FName Name, const FText& Content)
	{
		USizeBox* Box = B.Size(*FString::Printf(TEXT("%sSize"), *Name.ToString()), 0.f, 48.f);
		SetSizeChild(Box, B.Text(Name, B.Bold(36), Hex(0x16202C), Content), HAlign_Left, VAlign_Center);
		return Box;
	}

	// ---------- 신규/재구성 WBP 공통 ----------
	static UWidgetBlueprint* CreateOrLoadWidgetBlueprint(const FString& PackagePath, UClass* ParentClass)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
		if (UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, *(PackagePath + TEXT(".") + AssetName)))
		{
			if (Existing->ParentClass != ParentClass)
			{
				UE_LOG(LogTemp, Error, TEXT("[HQAuthor] %s exists with another parent"), *PackagePath);
				return nullptr;
			}
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackagePath);
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = ParentClass;
		Factory->BlueprintType = BPTYPE_Normal;
		UWidgetBlueprint* Created = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package,
			FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
		if (Created) FAssetRegistryModule::AssetCreated(Created);
		return Created;
	}

	// 루트까지 트리 전체를 비운다 (재실행 멱등)
	static void ClearTree(UWidgetBlueprint* Blueprint, const TSet<UWidget*>& Keep)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		if (UWidget* Root = Tree->RootWidget)
		{
			Tree->RootWidget = nullptr;
			RetireSubtree(Tree, Root, Keep);
		}
		TArray<UWidget*> Leftovers;
		Tree->GetAllWidgets(Leftovers);
		for (UWidget* Widget : Leftovers)
		{
			if (Widget && !Keep.Contains(Widget)) Retire(Tree, Widget);
		}
	}

	// ---------- UIE_HQKpiCell ----------
	static bool BuildKpiCell(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = CreateOrLoadWidgetBlueprint(FString(HQElementFolder) / TEXT("UIE_HQKpiCell"), UHQKpiCellWidget::StaticClass());
		if (!Blueprint) return false;
		Blueprint->Modify();
		ClearTree(Blueprint, {});
		FBuilder B{Blueprint->WidgetTree, &Assets};

		UVerticalBox* Root = B.VBox(TEXT("KpiRoot"));
		B.Tree->RootWidget = Root;

		UBorder* Band = B.Border(TEXT("LabelBand"), Rounded(Hex(0xD3DEEA), FVector4(18.f, 18.f, 0.f, 0.f), Hex(0xBCCADA), 3.f),
			FMargin(12.f, 0.f), HAlign_Center, VAlign_Center);
		USizeBox* BandSize = B.Size(TEXT("LabelBandSize"), 0.f, 56.f);
		SetSizeChild(BandSize, Band);
		Band->SetContent(B.Text(TEXT("LabelText"), B.Bold(30), Hex(0x33475E), FText::GetEmpty(), ETextJustify::Center));
		AddV(Root, BandSize);

		UBorder* Face = nullptr;
		UBorder* Lip = B.Card(TEXT("ValueLip"), TEXT("ValueFace"), FVector4(0.f, 0.f, 20.f, 20.f), Hex(0xFFFFFF), Hex(0xC5D0DD), Hex(0xB1BDCC), Face);
		USizeBox* ValueSize = B.Size(TEXT("ValueSize"), 0.f, 126.f);
		SetSizeChild(ValueSize, Lip);
		AddV(Root, ValueSize, FMargin(0.f, -3.f, 0.f, 0.f));

		UOverlay* ValueLayer = B.Overlay(TEXT("ValueLayer"));
		Face->SetContent(ValueLayer);
		USizeBox* WellSize = B.Size(TEXT("IconWellSize"), 104.f, 104.f);
		UOverlay* WellLayer = B.Overlay(TEXT("IconWellLayer"));
		SetSizeChild(WellSize, WellLayer);
		AddO(WellLayer, B.Image(TEXT("IconWell"), Assets.Well(104.f)));
		UImage* Icon = B.Image(TEXT("IconImage"), TextureBrush(Assets.Money, FVector2D(72.f, 72.f)));
		AddO(WellLayer, Icon, FMargin(0.f), HAlign_Center, VAlign_Center);
		AddO(ValueLayer, WellSize, FMargin(15.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

		UHorizontalBox* ValueRow = B.HBox(TEXT("ValueRow"));
		AddO(ValueLayer, ValueRow, FMargin(130.f, 0.f, 25.f, 0.f), HAlign_Right, VAlign_Center);
		AddH(ValueRow, B.Text(TEXT("ValueText"), B.Num(64), Hex(0x16202C), FText::FromString(TEXT("720만")), ETextJustify::Right),
			FMargin(0.f), false, HAlign_Right, VAlign_Bottom);
		AddH(ValueRow, B.Text(TEXT("UnitText"), B.NumMedium(34), Hex(0x5A6B7D), FText::FromString(TEXT("개"))),
			FMargin(8.f, 0.f, 0.f, 6.f), false, HAlign_Left, VAlign_Bottom);

		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- UIE_HQLevelStep ----------
	static bool BuildLevelStep(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = CreateOrLoadWidgetBlueprint(FString(HQElementFolder) / TEXT("UIE_HQLevelStep"), UHQLevelStepWidget::StaticClass());
		if (!Blueprint) return false;
		Blueprint->Modify();
		ClearTree(Blueprint, {});
		FBuilder B{Blueprint->WidgetTree, &Assets};

		UBorder* Face = nullptr;
		UBorder* Lip = B.Card(TEXT("CardLip"), TEXT("CardFace"), FVector4(22.f, 22.f, 22.f, 22.f), Hex(0xFFFFFF), Hex(0xC5D0DD), Hex(0xB1BDCC), Face,
			FMargin(4.f), 4.f);
		B.Tree->RootWidget = Lip;

		UVerticalBox* Column = B.VBox(TEXT("StepColumn"));
		Face->SetContent(Column);

		UVerticalBox* Mid = B.VBox(TEXT("StepMid"));
		AddV(Column, Mid, FMargin(18.f, 0.f), true, HAlign_Center, VAlign_Center);
		UBorder* Tile = B.Border(TEXT("LevelTile"), Rounded(FLinearColor::White, 24.f, Hex(0x0E1A28), 3.f), FMargin(0.f), HAlign_Center, VAlign_Center);
		Tile->SetBrushColor(Hex(0x243B56));
		USizeBox* TileSize = B.Size(TEXT("LevelTileSize"), 230.f, 140.f);
		SetSizeChild(TileSize, Tile);
		UHorizontalBox* TileRow = B.HBox(TEXT("LevelTileRow"));
		Tile->SetContent(TileRow);
		AddH(TileRow, B.Text(TEXT("LevelPrefixText"), B.Num(38), FLinearColor::White, FText::FromString(TEXT("Lv."))),
			FMargin(0.f, 0.f, 4.f, 12.f), false, HAlign_Left, VAlign_Bottom);
		AddH(TileRow, B.Text(TEXT("LevelText"), B.Num(76), FLinearColor::White, FText::FromString(TEXT("3"))),
			FMargin(0.f), false, HAlign_Left, VAlign_Center);
		AddV(Mid, TileSize, FMargin(0.f), false, HAlign_Center);

		USizeBox* UnlockSize = B.Size(TEXT("UnlockSize"));
		UnlockSize->SetMinDesiredHeight(156.f);
		SetSizeChild(UnlockSize, B.Text(TEXT("UnlockText"), B.Bold(36), Hex(0x16202C), FText::FromString(TEXT("세계지도 개방 (채광 · 무역)")),
			ETextJustify::Center, true), HAlign_Fill, VAlign_Top);
		AddV(Mid, UnlockSize, FMargin(0.f, 34.f, 0.f, 0.f), false, HAlign_Fill);

		UBorder* Strip = B.Border(TEXT("StepStrip"), Rounded(Hex(0xDDF2D2), FVector4(0.f, 0.f, 18.f, 18.f)), FMargin(0.f), HAlign_Center, VAlign_Center);
		USizeBox* StripSize = B.Size(TEXT("StepStripSize"), 0.f, 66.f);
		SetSizeChild(StripSize, Strip);
		UHorizontalBox* StripRow = B.HBox(TEXT("StepStripRow"));
		Strip->SetContent(StripRow);
		AddH(StripRow, MakeCheck(B, TEXT("StepCheck")), FMargin(0.f, 0.f, 14.f, 0.f), false, HAlign_Left, VAlign_Center);
		AddH(StripRow, B.Text(TEXT("StepStripText"), B.Bold(31), Hex(0x2B7419), NSLOCTEXT("HQ", "StepDone", "달성")), FMargin(0.f), false, HAlign_Left, VAlign_Center);
		AddV(Column, StripSize);

		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- UIE_HQRewardSlot ----------
	// 목업 .slot = .tile(크림 면 + 테 3 + 립 5, 반경 24) + .cnt 배지 + .cap2 이름. 크기·테 색은 C++ 이 보상 종류·개수로 바꾼다
	static bool BuildRewardSlot(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = CreateOrLoadWidgetBlueprint(FString(HQElementFolder) / TEXT("UIE_HQRewardSlot"), UHQRewardSlotWidget::StaticClass());
		if (!Blueprint) return false;
		Blueprint->Modify();
		ClearTree(Blueprint, {});
		FBuilder B{Blueprint->WidgetTree, &Assets};

		UVerticalBox* Root = B.VBox(TEXT("SlotRoot"));
		B.Tree->RootWidget = Root;

		USizeBox* TileSize = B.Size(TEXT("TileSize"), 112.f, 117.f);
		UOverlay* TileLayer = B.Overlay(TEXT("TileLayer"));
		SetSizeChild(TileSize, TileLayer);
		// 금테 칸(공장·세계지도) 바깥 후광 — 목업 0 0 0 8px rgba(255,214,107,.35)
		UImage* Glow = B.Image(TEXT("TileGlow"), Rounded(Hex(0xFFD66B, 0.35f), 32.f));
		AddO(TileLayer, Glow, FMargin(-8.f, -8.f, -8.f, -3.f));
		Glow->SetVisibility(ESlateVisibility::Collapsed);
		UBorder* Face = nullptr;
		UBorder* Lip = B.Card(TEXT("TileLip"), TEXT("TileFace"), FVector4(24.f, 24.f, 24.f, 24.f), Hex(0xFBF4E6), Hex(0xE0C68C), Hex(0xCDAE6B), Face,
			FMargin(3.f), 3.f, 5.f);
		AddO(TileLayer, Lip);
		UOverlay* FaceLayer = B.Overlay(TEXT("TileFaceLayer"));
		Face->SetContent(FaceLayer);
		AddO(FaceLayer, B.Image(TEXT("TileSheen"), RoundedTexture(Assets.SheenV, FVector2D(8.f, 106.f), FVector4(21.f, 21.f, 21.f, 21.f))));
		UImage* Photo = B.Image(TEXT("PhotoImage"), RoundedTexture(Assets.Building, FVector2D(106.f, 106.f), FVector4(21.f, 21.f, 21.f, 21.f)));
		AddO(FaceLayer, Photo);
		Photo->SetVisibility(ESlateVisibility::Collapsed);
		AddO(FaceLayer, B.Image(TEXT("IconImage"), TextureBrush(Assets.Money, FVector2D(80.f, 80.f))), FMargin(0.f), HAlign_Center, VAlign_Center);

		USizeBox* BadgeSize = B.Size(TEXT("CountBadge"), 0.f, 46.f);
		BadgeSize->SetMinDesiredWidth(58.f);
		UBorder* Pill = B.Border(TEXT("CountBadgePill"), Rounded(Hex(0x2C3E55), 23.f, Hex(0xFFF8EA), 3.f), FMargin(13.f, 0.f), HAlign_Center, VAlign_Center);
		SetSizeChild(BadgeSize, Pill);
		Pill->SetContent(B.Text(TEXT("CountText"), B.Num(28), FLinearColor::White, FText::FromString(TEXT("x1")), ETextJustify::Center));
		// 목업 .cnt = 칸 오른쪽 아래 -12 (립 5 는 칸 바깥이라 레이어 기준 -7)
		AddO(TileLayer, BadgeSize, FMargin(0.f, 0.f, -12.f, -7.f), HAlign_Right, VAlign_Bottom);
		BadgeSize->SetVisibility(ESlateVisibility::Collapsed);
		AddV(Root, TileSize, FMargin(0.f), false, HAlign_Center);

		// 이름은 칸보다 양옆 28 씩 넓게(칸 사이 간격으로 넘친다) — 음수 여백은 원하는 폭에서 빠져 칸 배치를 밀지 않는다
		AddV(Root, B.Text(TEXT("CaptionText"), B.Bold(28), Hex(0x3A2A0F), FText::FromString(TEXT("업적")), ETextJustify::Center),
			FMargin(-28.f, 12.f, -28.f, 0.f), false, HAlign_Fill);

		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- UIE_HQIndustryCard ----------
	static bool BuildIndustryCard(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = CreateOrLoadWidgetBlueprint(FString(HQElementFolder) / TEXT("UIE_HQIndustryCard"), UHQIndustryCardWidget::StaticClass());
		if (!Blueprint) return false;
		Blueprint->Modify();
		ClearTree(Blueprint, {});
		FBuilder B{Blueprint->WidgetTree, &Assets};

		UOverlay* Root = B.Overlay(TEXT("IndustryRoot"));
		B.Tree->RootWidget = Root;

		UBorder* Face = nullptr;
		UBorder* Lip = B.Card(TEXT("CardLip"), TEXT("CardFace"), FVector4(22.f, 22.f, 22.f, 22.f), Hex(0xFFFFFF), Hex(0xC5D0DD), Hex(0xB1BDCC), Face);
		AddO(Root, Lip);
		UVerticalBox* Column = B.VBox(TEXT("CardColumn"));
		Face->SetContent(Column);

		// 리본 = 아래색 면 + 위색 세로 시트(목업 linear-gradient(c1, c2))
		UBorder* Ribbon = B.Border(TEXT("RibbonBorder"), Rounded(FLinearColor::White, FVector4(19.f, 19.f, 0.f, 0.f)));
		Ribbon->SetBrushColor(Hex(0x8041BC));
		USizeBox* RibbonSize = B.Size(TEXT("RibbonSize"), 0.f, 92.f);
		SetSizeChild(RibbonSize, Ribbon);
		UOverlay* RibbonLayer = B.Overlay(TEXT("RibbonLayer"));
		Ribbon->SetContent(RibbonLayer);
		AddO(RibbonLayer, B.Image(TEXT("RibbonSheen"), RoundedTexture(Assets.SheenV, FVector2D(8.f, 92.f), FVector4(19.f, 19.f, 0.f, 0.f)), Hex(0x9B4EE3)));
		AddO(RibbonLayer, B.Image(TEXT("RibbonShade"), SolidBrush(FLinearColor(0.f, 0.f, 0.f, 0.18f))), FMargin(0.f), HAlign_Fill, VAlign_Bottom)
			->SetPadding(FMargin(0.f, 88.f, 0.f, 0.f));
		AddO(RibbonLayer, B.Text(TEXT("NameText"), B.Bold(40), FLinearColor::White, FText::FromString(TEXT("게임")), ETextJustify::Center),
			FMargin(0.f, 17.f, 0.f, 0.f), HAlign_Center, VAlign_Top);
		AddV(Column, RibbonSize);

		AddV(Column, B.Text(TEXT("CategoryText"), B.Bold(28), Hex(0x5A6B7D), FText::FromString(TEXT("프로젝트형")), ETextJustify::Center),
			FMargin(0.f, 121.f, 0.f, 0.f), false, HAlign_Center);

		// 운영 중 — 수치 4줄
		UVerticalBox* Metrics = B.VBox(TEXT("MetricsBox"));
		AddV(Column, Metrics, FMargin(19.f, 14.f, 19.f, 0.f), true);
		struct FRow { const TCHAR* Key; const TCHAR* Label; const TCHAR* Value; const TCHAR* Unit; };
		const FRow Rows[] = {
			{ TEXT("Buildings"), TEXT("보유 빌딩"), TEXT("4"), TEXT("개") },
			{ TEXT("Employees"), TEXT("직원"), TEXT("10"), TEXT("명") },
			{ TEXT("Tier"), TEXT("최고 단계"), TEXT("3"), TEXT("단계") },
			{ TEXT("Income"), TEXT("초당 수익"), TEXT("1,240"), TEXT("/초") },
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
		{
			const FRow& Row = Rows[Index];
			UBorder* RowBorder = B.Border(*FString::Printf(TEXT("Metric%sRow"), Row.Key), Rounded(Hex(0xF0F4F8), 14.f), FMargin(22.f, 0.f), HAlign_Fill, VAlign_Center);
			USizeBox* RowSize = B.Size(*FString::Printf(TEXT("Metric%sSize"), Row.Key), 0.f, 66.f);
			SetSizeChild(RowSize, RowBorder);
			UHorizontalBox* Line = B.HBox(*FString::Printf(TEXT("Metric%sLine"), Row.Key));
			RowBorder->SetContent(Line);
			AddH(Line, B.Text(*FString::Printf(TEXT("Metric%sLabel"), Row.Key), B.Bold(30), Hex(0x5A6B7D), FText::FromString(Row.Label)),
				FMargin(0.f), true, HAlign_Left, VAlign_Center);
			AddH(Line, B.Text(*FString::Printf(TEXT("%sValue"), Row.Key), B.Num(42), Hex(0x16202C), FText::FromString(Row.Value), ETextJustify::Right),
				FMargin(0.f), false, HAlign_Right, VAlign_Center);
			AddH(Line, B.Text(*FString::Printf(TEXT("Metric%sUnit"), Row.Key), B.NumMedium(28), Hex(0x5A6B7D), FText::FromString(Row.Unit)),
				FMargin(4.f, 0.f, 0.f, 0.f), false, HAlign_Left, VAlign_Center);
			AddV(Metrics, RowSize, FMargin(0.f, Index == 0 ? 0.f : 10.f, 0.f, 0.f));
		}

		// 잠김·미진출 — 특징 문구
		UVerticalBox* Feature = B.VBox(TEXT("FeatureBox"));
		AddV(Column, Feature, FMargin(27.f, 22.f, 27.f, 0.f), true);
		AddV(Feature, B.Text(TEXT("FeatureTitleText"), B.Bold(44), Hex(0x16202C), FText::FromString(TEXT("Lv.5에 해금")), ETextJustify::Center));
		AddV(Feature, B.Text(TEXT("FeatureDescText"), B.Bold(32), Hex(0x33475E), FText::FromString(TEXT("대량 생산 고수익")), ETextJustify::Center, true),
			FMargin(0.f, 18.f, 0.f, 0.f));
		AddV(Feature, B.Text(TEXT("FeatureSubText"), B.Regular(28), Hex(0x5A6B7D), FText::FromString(TEXT("부서: 디자인 / 엔지니어 / 테스트 / 마케팅")), ETextJustify::Center, true),
			FMargin(0.f, 18.f, 0.f, 0.f));
		Feature->SetVisibility(ESlateVisibility::Collapsed);

		UBorder* Strip = B.Border(TEXT("StatusStrip"), Rounded(FLinearColor::White, FVector4(0.f, 0.f, 19.f, 19.f)), FMargin(0.f), HAlign_Center, VAlign_Center);
		Strip->SetBrushColor(Hex(0xF2E6FE));
		USizeBox* StripSize = B.Size(TEXT("StatusStripSize"), 0.f, 66.f);
		SetSizeChild(StripSize, Strip);
		Strip->SetContent(B.Text(TEXT("StatusText"), B.Bold(31), Hex(0x572C80), FText::FromString(TEXT("운영 중")), ETextJustify::Center));
		AddV(Column, StripSize, FMargin(0.f, 14.f, 0.f, 0.f));

		// 아이콘 노치 — 리본에 걸친 흰 원 (목업 top 64, 지름 144, 테두리 7)
		USizeBox* NotchSize = B.Size(TEXT("NotchSize"), 144.f, 149.f);
		UOverlay* Notch = B.Overlay(TEXT("NotchLayer"));
		SetSizeChild(NotchSize, Notch);
		AddO(Notch, B.Image(TEXT("NotchShadow"), Rounded(FLinearColor(0.f, 0.f, 0.f, 0.12f), 72.f)), FMargin(0.f, 5.f, 0.f, 0.f));
		UImage* Ring = B.Image(TEXT("NotchRing"), Rounded(FLinearColor::White, 72.f, Hex(0x9B4EE3), 7.f));
		AddO(Notch, Ring, FMargin(0.f, 0.f, 0.f, 5.f));
		AddO(Notch, B.Image(TEXT("IconImage"), TextureBrush(Assets.Money, FVector2D(100.f, 100.f))), FMargin(0.f, 0.f, 0.f, 5.f), HAlign_Center, VAlign_Center);
		USizeBox* LockSize = B.Size(TEXT("LockBadge"), 56.f, 56.f);
		UOverlay* LockLayer = B.Overlay(TEXT("LockLayer"));
		SetSizeChild(LockSize, LockLayer);
		AddO(LockLayer, B.Image(TEXT("LockDisc"), Rounded(FLinearColor::White, 28.f, Hex(0x56616E), 3.f)));
		AddO(LockLayer, B.Image(TEXT("LockGlyph"), TextureBrush(Assets.Lock, FVector2D(36.f, 36.f))), FMargin(0.f), HAlign_Center, VAlign_Center);
		AddO(Notch, LockSize, FMargin(0.f, 0.f, -4.f, 1.f), HAlign_Right, VAlign_Bottom);
		LockSize->SetVisibility(ESlateVisibility::Collapsed);
		AddO(Root, NotchSize, FMargin(0.f, 64.f, 0.f, 0.f), HAlign_Center, VAlign_Top);

		if (!Compile(Blueprint)) return false;
		if (UHQIndustryCardWidget* Defaults = Cast<UHQIndustryCardWidget>(Blueprint->GeneratedClass->GetDefaultObject()))
		{
			FObjectProperty* MaterialProperty = FindFProperty<FObjectProperty>(UHQIndustryCardWidget::StaticClass(), TEXT("LockedIconMaterial"));
			if (MaterialProperty)
			{
				Defaults->Modify();
				MaterialProperty->SetObjectPropertyValue_InContainer(Defaults, Assets.Desaturate);
			}
		}
		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- UIE_CondStatRow → UHQConditionCardWidget ----------
	static bool BuildConditionCard(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = LoadAsset<UWidgetBlueprint>(CondPath);
		if (!Blueprint) return false;
		Blueprint->Modify();
		if (Blueprint->ParentClass != UHQConditionCardWidget::StaticClass())
		{
			Blueprint->ParentClass = UHQConditionCardWidget::StaticClass();
			FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		}
		ClearTree(Blueprint, {});
		FBuilder B{Blueprint->WidgetTree, &Assets};

		UBorder* Face = nullptr;
		UBorder* Lip = B.Card(TEXT("CardLip"), TEXT("CardFace"), FVector4(22.f, 22.f, 22.f, 22.f), Hex(0xFFFFFF), Hex(0xC5D0DD), Hex(0xB1BDCC), Face);
		B.Tree->RootWidget = Lip;
		UVerticalBox* Column = B.VBox(TEXT("CardColumn"));
		Face->SetContent(Column);

		UVerticalBox* Top = B.VBox(TEXT("CardTop"));
		AddV(Column, Top, FMargin(25.f, 19.f, 25.f, 0.f), true);

		UHorizontalBox* LabelRow = B.HBox(TEXT("LabelRow"));
		USizeBox* WellSize = B.Size(TEXT("IconWellSize"), 96.f, 96.f);
		UOverlay* Well = B.Overlay(TEXT("IconWellLayer"));
		SetSizeChild(WellSize, Well);
		AddO(Well, B.Image(TEXT("IconWell"), Assets.Well(96.f)));
		AddO(Well, B.Image(TEXT("IconImage"), TextureBrush(Assets.Money, FVector2D(66.f, 66.f))), FMargin(0.f), HAlign_Center, VAlign_Center);
		USizeBox* TierSize = B.Size(TEXT("TierBadge"), 44.f, 44.f);
		UBorder* TierDisc = B.Border(TEXT("TierDisc"), Rounded(Hex(0x3D9BE0), 22.f, FLinearColor::White, 4.f), FMargin(0.f), HAlign_Center, VAlign_Center);
		SetSizeChild(TierSize, TierDisc);
		TierDisc->SetContent(B.Text(TEXT("TierBadgeText"), B.Num(26), FLinearColor::White, FText::FromString(TEXT("4")), ETextJustify::Center));
		AddO(Well, TierSize, FMargin(0.f, 0.f, -8.f, -8.f), HAlign_Right, VAlign_Bottom);
		TierSize->SetVisibility(ESlateVisibility::Collapsed);
		AddH(LabelRow, WellSize, FMargin(0.f), false, HAlign_Left, VAlign_Center);
		AddH(LabelRow, B.Text(TEXT("LabelText"), B.Bold(34), Hex(0x16202C), FText::FromString(TEXT("보유 자금"))),
			FMargin(20.f, 0.f, 0.f, 0.f), true, HAlign_Left, VAlign_Center);
		AddV(Top, LabelRow);

		UHorizontalBox* ValueRow = B.HBox(TEXT("ValueRow"));
		AddH(ValueRow, B.Text(TEXT("CurrentText"), B.Num(64), Hex(0x16202C), FText::FromString(TEXT("720만"))), FMargin(0.f), false, HAlign_Left, VAlign_Bottom);
		AddH(ValueRow, B.Text(TEXT("TargetText"), B.NumMedium(38), Hex(0x5A6B7D), FText::FromString(TEXT("/ 1200만"))),
			FMargin(10.f, 0.f, 0.f, 5.f), false, HAlign_Left, VAlign_Bottom);
		AddV(Top, ValueRow, FMargin(0.f, 12.f, 0.f, 0.f));

		USizeBox* BarSize = B.Size(TEXT("BarSize"), 0.f, 24.f);
		SetSizeChild(BarSize, MakeBar(B, TEXT("ProgressBar"), Hex(0xD2DBE6), Hex(0xB3C0CF), 2.f, 12.f, 3.f));
		AddV(Top, BarSize, FMargin(0.f, 14.f, 0.f, 0.f));

		UBorder* Strip = B.Border(TEXT("StatusStrip"), Rounded(FLinearColor::White, FVector4(0.f, 0.f, 19.f, 19.f)), FMargin(25.f, 0.f), HAlign_Left, VAlign_Center);
		Strip->SetBrushColor(Hex(0xE3EAF3));
		USizeBox* StripSize = B.Size(TEXT("StatusStripSize"), 0.f, 66.f);
		SetSizeChild(StripSize, Strip);
		UHorizontalBox* StripRow = B.HBox(TEXT("StatusRow"));
		Strip->SetContent(StripRow);
		UWidget* Check = MakeCheck(B, TEXT("StatusCheck"));
		// BindWidget 이름은 체크 묶음 전체 — C++ 이 달성일 때만 보인다
		Check->Rename(TEXT("StatusCheck"), B.Tree, REN_DontCreateRedirectors | REN_NonTransactional);
		AddH(StripRow, Check, FMargin(0.f, 0.f, 14.f, 0.f), false, HAlign_Left, VAlign_Center);
		AddH(StripRow, B.Text(TEXT("StatusText"), B.Bold(31), Hex(0x2C4865), FText::FromString(TEXT("480만 더 필요"))), FMargin(0.f), false, HAlign_Left, VAlign_Center);
		Check->SetVisibility(ESlateVisibility::Collapsed);
		AddV(Column, StripSize, FMargin(0.f, 20.f, 0.f, 0.f));

		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- UIE_BuildingSlotHorizon ----------
	static bool BuildBuildingSlot(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = LoadAsset<UWidgetBlueprint>(SlotPath);
		if (!Blueprint) return false;
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UButtonWidget* NameButton = Cast<UButtonWidget>(Tree->FindWidget(TEXT("BuildingNameButton")));
		UButtonWidget* MoveButton = Cast<UButtonWidget>(Tree->FindWidget(TEXT("MoveBtn")));
		if (!NameButton || !MoveButton)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] UIE_BuildingSlotHorizon buttons missing"));
			return false;
		}
		Blueprint->Modify();
		NameButton->RemoveFromParent();
		MoveButton->RemoveFromParent();
		ClearTree(Blueprint, {NameButton, MoveButton});
		FBuilder B{Tree, &Assets};

		UBorder* Face = nullptr;
		UBorder* Lip = B.Card(TEXT("RowLip"), TEXT("RowFace"), FVector4(20.f, 20.f, 20.f, 20.f), Hex(0xFFFFFF), Hex(0xC5D0DD), Hex(0xB1BDCC), Face,
			FMargin(21.f, 3.f));
		USizeBox* RootSize = B.Size(TEXT("RowSize"), 0.f, 156.f);
		SetSizeChild(RootSize, Lip);
		Tree->RootWidget = RootSize;
		UHorizontalBox* Row = B.HBox(TEXT("RowHBox"));
		Face->SetContent(Row);

		UHorizontalBox* NameCol = B.HBox(TEXT("NameColumn"));
		AddH(Row, NameCol, FMargin(0.f), true, HAlign_Fill, VAlign_Center);
		USizeBox* ThumbSize = B.Size(TEXT("ThumbSize"), 120.f, 120.f);
		UOverlay* Thumb = B.Overlay(TEXT("ThumbLayer"));
		SetSizeChild(ThumbSize, Thumb);
		AddO(Thumb, B.Image(TEXT("IconPlate"), Rounded(Hex(0xDDE4EC), 18.f, Hex(0x9B4EE3), 5.f)));
		AddO(Thumb, B.Image(TEXT("IconImage"), TextureBrush(Assets.Building, FVector2D(102.f, 102.f))), FMargin(9.f));
		USizeBox* BadgeSize = B.Size(TEXT("IndustryBadgeSize"), 58.f, 58.f);
		UOverlay* Badge = B.Overlay(TEXT("IndustryBadgeLayer"));
		SetSizeChild(BadgeSize, Badge);
		AddO(Badge, B.Image(TEXT("IndustryBadgeRing"), Rounded(FLinearColor::White, 29.f, Hex(0x9B4EE3), 4.f)));
		AddO(Badge, B.Image(TEXT("IndustryBadgeIcon"), TextureBrush(Assets.Money, FVector2D(40.f, 40.f))), FMargin(0.f), HAlign_Center, VAlign_Center);
		AddO(Thumb, BadgeSize, FMargin(0.f, 0.f, -12.f, -12.f), HAlign_Right, VAlign_Bottom);
		AddH(NameCol, ThumbSize, FMargin(0.f), false, HAlign_Left, VAlign_Center);

		UVerticalBox* TextCol = B.VBox(TEXT("NameTextColumn"));
		AddH(NameCol, TextCol, FMargin(30.f, 0.f, 12.f, 0.f), true, HAlign_Fill, VAlign_Center);
		NameButton->SetTextSize(30.f);
		NameButton->SetTextColor(FSlateColor(Hex(0x16202C)));
		AddV(TextCol, NameButton, FMargin(0.f), false, HAlign_Left);
		AddV(TextCol, B.Text(TEXT("LevelText"), B.Bold(30), Hex(0x5A6B7D), FText::FromString(TEXT("게임 · 3단계"))), FMargin(14.f, 4.f, 0.f, 0.f));

		auto NumberColumn = [&](const TCHAR* Key, FName MainName, FName SubName, const TCHAR* MainSample, const TCHAR* SubSample)
		{
			USizeBox* ColumnSize = B.Size(*FString::Printf(TEXT("%sColumn"), Key), 280.f, 0.f);
			UHorizontalBox* Line = B.HBox(*FString::Printf(TEXT("%sLine"), Key));
			SetSizeChild(ColumnSize, Line, HAlign_Center, VAlign_Center);
			AddH(Line, B.Text(MainName, B.Num(44), Hex(0x16202C), FText::FromString(MainSample)), FMargin(0.f), false, HAlign_Left, VAlign_Bottom);
			AddH(Line, B.Text(SubName, B.NumMedium(30), Hex(0x5A6B7D), FText::FromString(SubSample)), FMargin(6.f, 0.f, 0.f, 4.f), false, HAlign_Left, VAlign_Bottom);
			AddH(Row, ColumnSize, FMargin(0.f), false, HAlign_Center, VAlign_Center);
		};
		NumberColumn(TEXT("Employee"), TEXT("EmployeeNumText"), TEXT("EmployeeCapText"), TEXT("4"), TEXT("/ 10"));
		NumberColumn(TEXT("Income"), TEXT("IncomeText"), TEXT("IncomeUnitText"), TEXT("600"), TEXT("/초"));

		USizeBox* MoveCol = B.Size(TEXT("MoveColumn"), 190.f, 0.f);
		USizeBox* MoveSize = B.Size(TEXT("MoveSize"), 160.f, 104.f);
		SetSizeChild(MoveCol, MoveSize, HAlign_Right, VAlign_Center);
		SetSizeChild(MoveSize, MoveButton);
		MoveButton->SetTextSize(26.f);
		// 목록 행마다 반복되는 보조 행동이라 직원창 강화 버튼과 같은 슬레이트 볼륨으로 낮춘다
		if (UClass* SlateStyle = LoadObject<UClass>(nullptr, TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_EmployeeUtility.CUI_Style2_Btn_EmployeeUtility_C")))
		{
			MoveButton->SetStyle(SlateStyle);
		}
		AddH(Row, MoveCol, FMargin(0.f), false, HAlign_Right, VAlign_Center);

		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- UI_HQManagePanel ----------
	static void ReorderRailTabs(UWidgetTree* Tree, FBuilder& B)
	{
		UIconWithButtonWidget* Growth = Cast<UIconWithButtonWidget>(Tree->FindWidget(TEXT("HQTab")));
		UIconWithButtonWidget* Operations = Cast<UIconWithButtonWidget>(Tree->FindWidget(TEXT("OperationsTab")));
		if (!Operations)
		{
			Operations = Cast<UIconWithButtonWidget>(Tree->FindWidget(TEXT("FinancialTab")));
			if (Operations) Operations->Rename(TEXT("OperationsTab"), Tree, REN_DontCreateRedirectors | REN_NonTransactional);
		}
		UIconWithButtonWidget* Buildings = Cast<UIconWithButtonWidget>(Tree->FindWidget(TEXT("BuildingListTab")));
		if (!Growth || !Operations || !Buildings)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Rail tabs missing"));
			return;
		}

		// HQTab 은 알림 점 오버레이로 감싸져 있을 수 있다 — 레일 패널은 그 바깥
		UWidget* GrowthEntry = Growth;
		if (UOverlay* Wrapper = Cast<UOverlay>(Growth->GetParent()))
		{
			if (Wrapper->GetFName() == TEXT("GrowthTabHost")) GrowthEntry = Wrapper;
		}
		UPanelWidget* Rail = Buildings->GetParent();
		if (!Rail)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Rail panel missing"));
			return;
		}
		// 떼면 슬롯 속성이 초기화된다 — 기존 탭 슬롯을 떠 두고 되돌린다
		UPanelSlot* Template = Buildings->Slot;
		FMargin RailPadding(0.f);
		if (UVerticalBoxSlot* RailSlot = Cast<UVerticalBoxSlot>(Template)) RailPadding = RailSlot->GetPadding();

		for (UIconWithButtonWidget* Tab : {Growth, Operations, Buildings})
		{
			if (FObjectProperty* IconProperty = FindFProperty<FObjectProperty>(UIconWithButtonWidget::StaticClass(), TEXT("IconTexture")))
			{
				IconProperty->SetObjectPropertyValue_InContainer(Tab, nullptr);
			}
			Tab->SetIconVisibility(ESlateVisibility::Collapsed);
			Tab->SetVisibility(ESlateVisibility::Visible);
		}
		Growth->SetButtonText(NSLOCTEXT("HQ", "TabGrowth", "성장"));
		Operations->SetButtonText(NSLOCTEXT("HQ", "TabOperations", "운영 현황"));
		Buildings->SetButtonText(NSLOCTEXT("HQ", "TabBuildings", "보유 빌딩"));

		UOverlay* Host = Cast<UOverlay>(GrowthEntry);
		if (!Host)
		{
			Host = B.Overlay(TEXT("GrowthTabHost"));
			Growth->RemoveFromParent();
			AddO(Host, Growth);
			UImage* Dot = B.Image(TEXT("GrowthTabAlert"), Rounded(Hex(0xE5484D), 11.f, FLinearColor::White, 3.f));
			Dot->SetBrush([&]() { FSlateBrush Brush = Dot->GetBrush(); Brush.ImageSize = FVector2D(22.f, 22.f); return Brush; }());
			Dot->SetVisibility(ESlateVisibility::Collapsed);
			AddO(Host, Dot, FMargin(0.f, 0.f, 36.f, 0.f), HAlign_Right, VAlign_Center);
		}
		for (UWidget* Entry : {static_cast<UWidget*>(Host), static_cast<UWidget*>(Operations), static_cast<UWidget*>(Buildings)})
		{
			Entry->RemoveFromParent();
		}
		for (UWidget* Entry : {static_cast<UWidget*>(Host), static_cast<UWidget*>(Operations), static_cast<UWidget*>(Buildings)})
		{
			UPanelSlot* NewSlot = Rail->AddChild(Entry);
			if (UVerticalBoxSlot* RailSlot = Cast<UVerticalBoxSlot>(NewSlot))
			{
				RailSlot->SetPadding(RailPadding);
				RailSlot->SetHorizontalAlignment(HAlign_Fill);
				RailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}
		// 레일 끝 여백(Spacer)이 있으면 탭 뒤로 보낸다
		TArray<UWidget*> Trailing;
		for (int32 Index = 0; Index < Rail->GetChildrenCount(); ++Index)
		{
			UWidget* Child = Rail->GetChildAt(Index);
			if (Child != Host && Child != Operations && Child != Buildings) Trailing.Add(Child);
		}
		for (UWidget* Child : Trailing)
		{
			FSlateChildSize ChildSize(ESlateSizeRule::Fill);
			if (UVerticalBoxSlot* OldSlot = Cast<UVerticalBoxSlot>(Child->Slot)) ChildSize = OldSlot->GetSize();
			Child->RemoveFromParent();
			if (UVerticalBoxSlot* NewSlot = Cast<UVerticalBoxSlot>(Rail->AddChild(Child))) NewSlot->SetSize(ChildSize);
		}
	}

	static UWidget* PageShell(FBuilder& B, FName Prefix, UVerticalBox*& OutContent)
	{
		UOverlay* Page = B.Overlay(*FString::Printf(TEXT("%sPage"), *Prefix.ToString()));
		AddO(Page, B.Border(*FString::Printf(TEXT("%sPlate"), *Prefix.ToString()), Rounded(Hex(0xE6EBF2), 14.f)));
		FSlateBrush Noise;
		Noise.SetResourceObject(B.A->NoisePlate);
		Noise.ImageSize = FVector2D(64.f, 64.f);
		Noise.DrawAs = ESlateBrushDrawType::Image;
		AddO(Page, B.Image(*FString::Printf(TEXT("%sGrain"), *Prefix.ToString()), Noise));
		OutContent = B.VBox(*FString::Printf(TEXT("%sContent"), *Prefix.ToString()));
		AddO(Page, OutContent, FMargin(28.f));
		return Page;
	}

	static UWidget* BuildGrowthPage(FBuilder& B, UUserWidget* LevelUpButton, UImage* ShineImage, UClass* CondClass, UClass* CostButtonClass)
	{
		const FAssets& A = *B.A;
		UVerticalBox* Content = nullptr;
		UWidget* Page = PageShell(B, TEXT("Growth"), Content);

		// ---- 회사 레벨 띠 ----
		UBorder* HeroFace = nullptr;
		UBorder* HeroLip = B.Card(TEXT("HeroLip"), TEXT("HeroFace"), FVector4(26.f, 26.f, 26.f, 26.f), Hex(0x1C2F45), Hex(0x0E1A28), Hex(0x0B1520), HeroFace);
		USizeBox* HeroSize = B.Size(TEXT("HeroSize"), 0.f, 296.f);
		SetSizeChild(HeroSize, HeroLip);
		AddV(Content, HeroSize);
		UOverlay* HeroLayer = B.Overlay(TEXT("HeroLayer"));
		HeroFace->SetContent(HeroLayer);
		AddO(HeroLayer, B.Image(TEXT("HeroSheen"), RoundedTexture(A.SheenV, FVector2D(8.f, 284.f), FVector4(23.f, 23.f, 23.f, 23.f)), Hex(0x2C4867)));
		AddO(HeroLayer, B.Image(TEXT("HeroHalftone"), RoundedTexture(A.Halftone, FVector2D(1060.f, 284.f), FVector4(0.f, 23.f, 23.f, 0.f)), FLinearColor(1.f, 1.f, 1.f, 0.10f)),
			FMargin(0.f), HAlign_Right, VAlign_Fill);
		UHorizontalBox* HeroRow = B.HBox(TEXT("HeroRow"));
		AddO(HeroLayer, HeroRow, FMargin(27.f, 0.f, 45.f, 0.f));

		// 명판 — 텍스처 투명 여백(45,9)-(1627,891) of 1672x940 을 칸 밖으로 밀어 그림 영역만 470x262 에 맞춘다
		USizeBox* PlaqueSize = B.Size(TEXT("PlaqueSize"), 470.f, 262.f);
		UOverlay* Plaque = B.Overlay(TEXT("PlaqueLayer"));
		SetSizeChild(PlaqueSize, Plaque);
		const float S = 470.f / 1582.f;
		AddO(Plaque, B.Image(TEXT("PlaqueImage"), TextureBrush(A.Plaque, FVector2D(1672.f * S, 940.f * S))),
			FMargin(-45.f * S, -9.f * S, -(1672.f - 1627.f) * S, -(940.f - 891.f) * S));
		// 두 자리 레벨(개발 샌드박스 HQ=20 등)은 글자 폭이 명판 면을 넘는다 — 넘칠 때만 통째로 줄인다
		// 텍스처 아래쪽은 그림자라 명판 면의 중심이 칸 중심보다 위다 — 위 여백 2 로 면 중심(캡처 실측)에 맞춘다
		UScaleBox* LevelFit = B.Make<UScaleBox>(TEXT("LevelNumberFit"));
		LevelFit->SetStretch(EStretch::ScaleToFit);
		LevelFit->SetStretchDirection(EStretchDirection::DownOnly);
		AddO(Plaque, LevelFit, FMargin(150.f, 2.f, 44.f, 0.f), HAlign_Fill, VAlign_Fill);
		UHorizontalBox* LevelRow = B.HBox(TEXT("LevelNumberRow"));
		LevelFit->SetContent(LevelRow);
		const FLinearColor PlaqueInk = Hex(0x0F2236);
		AddH(LevelRow, B.Text(TEXT("LevelBadgePrefix"), B.Num(46, 4, PlaqueInk), FLinearColor::White, FText::FromString(TEXT("Lv."))),
			FMargin(0.f, 0.f, 10.f, 14.f), false, HAlign_Left, VAlign_Bottom);
		AddH(LevelRow, B.Text(TEXT("CurLevelText"), B.Num(134, 4, PlaqueInk), FLinearColor::White, FText::FromString(TEXT("3"))),
			FMargin(0.f), false, HAlign_Left, VAlign_Center);
		// VFX 텍스처는 런타임에 DT_UIVFXTexture 에서 들어온다 — 저작 상태는 투명
		UImage* Gloss = B.Image(TEXT("LevelBadgeGloss"), TextureBrush(nullptr, FVector2D(140.f, 236.f)));
		Gloss->SetRenderOpacity(0.f);
		AddO(Plaque, Gloss, FMargin(0.f), HAlign_Center, VAlign_Center);
		AddH(HeroRow, PlaqueSize, FMargin(0.f), false, HAlign_Left, VAlign_Center);

		UVerticalBox* HeroMain = B.VBox(TEXT("HeroMain"));
		AddH(HeroRow, HeroMain, FMargin(40.f, 0.f, 0.f, 0.f), true, HAlign_Fill, VAlign_Center);
		UHorizontalBox* HeroTop = B.HBox(TEXT("HeroTopRow"));
		AddV(HeroMain, HeroTop);
		AddH(HeroTop, B.Text(TEXT("HeroTitleText"), B.Bold(40), Hex(0xF1F5FA), FText::FromString(TEXT("다음 레벨"))), FMargin(0.f), false, HAlign_Left, VAlign_Bottom);
		AddH(HeroTop, B.Text(TEXT("HeroSubText"), B.Regular(34), Hex(0xA9BDD2), FText::FromString(TEXT("Lv.4까지"))), FMargin(14.f, 0.f, 0.f, 2.f), false, HAlign_Left, VAlign_Bottom);
		AddH(HeroTop, B.Make<USpacer>(TEXT("HeroTopSpacer")), FMargin(0.f), true);
		AddH(HeroTop, B.Text(TEXT("OverallProgressText"), B.Num(80), FLinearColor::White, FText::FromString(TEXT("81%")), ETextJustify::Right),
			FMargin(0.f), false, HAlign_Right, VAlign_Bottom);
		USizeBox* BigBarSize = B.Size(TEXT("OverallBarSize"), 0.f, 40.f);
		SetSizeChild(BigBarSize, MakeBar(B, TEXT("OverallProgressBar"), Hex(0x0E1B2A), Hex(0x0A1420), 3.f, 20.f, 5.f));
		AddV(HeroMain, BigBarSize, FMargin(0.f, 18.f, 0.f, 0.f));
		UHorizontalBox* HeroStatus = B.HBox(TEXT("HeroStatusRow"));
		AddV(HeroMain, HeroStatus, FMargin(0.f, 18.f, 0.f, 0.f));
		AddH(HeroStatus, B.Text(TEXT("HeroStatusText"), B.Bold(32), Hex(0xDCE6F1), FText::FromString(TEXT("조건 2 / 4 달성"))), FMargin(0.f), false, HAlign_Left, VAlign_Center);
		AddH(HeroStatus, B.Text(TEXT("HeroMissingText"), B.Regular(30), Hex(0xA9BDD2), FText::FromString(TEXT("부족: 보유 자금 · 전체 직원"))),
			FMargin(18.f, 0.f, 0.f, 0.f), true, HAlign_Left, VAlign_Center);

		// ---- 일반 레벨 본문 ----
		UHorizontalBox* GrowthBody = B.HBox(TEXT("GrowthBody"));
		AddV(Content, GrowthBody, FMargin(0.f, 22.f, 0.f, 0.f), true);
		UVerticalBox* CondColumn = B.VBox(TEXT("CondColumn"));
		AddH(GrowthBody, CondColumn, FMargin(0.f), true);
		AddV(CondColumn, SectionTitle(B, TEXT("CondTitleText"), NSLOCTEXT("HQ", "ConditionTitle", "레벨업 조건")), FMargin(0.f, 0.f, 0.f, 20.f));
		UUniformGridPanel* Grid = B.Make<UUniformGridPanel>(TEXT("ConditionGrid"));
		Grid->SetSlotPadding(FMargin(12.f, 10.f));
		Grid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddV(CondColumn, Grid, FMargin(-12.f, -10.f, -12.f, -10.f), true);
		struct FCond { const TCHAR* Name; UTexture2D* Icon; int32 Row; int32 Column; };
		const FCond Conds[] = {
			{ TEXT("Row_Cond_Money"), A.Money, 0, 0 },
			{ TEXT("Row_Cond_Buildings"), A.Building, 0, 1 },
			{ TEXT("Row_Cond_Tier"), A.Building, 1, 0 },
			{ TEXT("Row_Cond_Employees"), A.Employees, 1, 1 },
			{ TEXT("Row_Cond_MarketCap"), A.MarketCap, 2, 0 },
		};
		FObjectProperty* CondIcon = FindFProperty<FObjectProperty>(UHQConditionCardWidget::StaticClass(), TEXT("Icon"));
		for (const FCond& Cond : Conds)
		{
			UUserWidget* Card = B.MakeUser(CondClass, Cond.Name);
			if (!Card) continue;
			if (CondIcon) CondIcon->SetObjectPropertyValue_InContainer(Card, Cond.Icon);
			UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Card, Cond.Row, Cond.Column);
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
			if (FCString::Strcmp(Cond.Name, TEXT("Row_Cond_MarketCap")) == 0) Card->SetVisibility(ESlateVisibility::Collapsed);
		}

		USizeBox* RewardSize = B.Size(TEXT("RewardSize"), 612.f, 0.f);
		AddH(GrowthBody, RewardSize, FMargin(28.f, 0.f, 0.f, 0.f), false, HAlign_Fill, VAlign_Fill);
		UBorder* RewardFace = nullptr;
		UBorder* RewardLip = B.Card(TEXT("RewardLip"), TEXT("RewardFace"), FVector4(24.f, 24.f, 24.f, 24.f), Hex(0xFAEED3), Hex(0xE0C68C), Hex(0xCDAE6B), RewardFace);
		SetSizeChild(RewardSize, RewardLip);
		UOverlay* RewardLayer = B.Overlay(TEXT("RewardLayer"));
		RewardFace->SetContent(RewardLayer);
		AddO(RewardLayer, B.Image(TEXT("RewardSheen"), RoundedTexture(A.SheenV, FVector2D(8.f, 600.f), FVector4(21.f, 21.f, 21.f, 21.f)), Hex(0xFFF8EA)));
		UVerticalBox* RewardColumn = B.VBox(TEXT("RewardColumn"));
		AddO(RewardLayer, RewardColumn);
		UBorder* Band = B.Border(TEXT("RewardBand"), Rounded(Hex(0xF4E1B4), FVector4(21.f, 21.f, 0.f, 0.f)), FMargin(27.f, 0.f), HAlign_Left, VAlign_Center);
		USizeBox* BandSize = B.Size(TEXT("RewardBandSize"), 0.f, 76.f);
		SetSizeChild(BandSize, Band);
		Band->SetContent(B.Text(TEXT("RewardTitleText"), B.Bold(34), Hex(0x5E4210), FText::FromString(TEXT("Lv.4 레벨업 보상"))));
		AddV(RewardColumn, BandSize);
		USizeBox* BandLine = B.Size(TEXT("RewardBandLine"), 0.f, 3.f);
		SetSizeChild(BandLine, B.Image(TEXT("RewardBandLineImage"), SolidBrush(Hex(0xCDAE6B, 0.45f))));
		AddV(RewardColumn, BandLine);
		// 보상 칸 영역 — 목업 .slots(남는 높이 전부, 가운데 정렬 줄바꿈, 뒤 은은한 후광). 칸은 C++ 이 보상 수만큼 채운다
		UOverlay* SlotArea = B.Overlay(TEXT("RewardSlotArea"));
		AddO(SlotArea, B.Image(TEXT("RewardSlotGlow"), TextureBrush(A.GlowOval, FVector2D(560.f, 300.f)), Hex(0xFFF0C8, 0.9f)));
		// 보상이 많아 칸이 영역보다 높아지면 아래 문구를 덮지 않게 통째로 줄인다(DownOnly — 평소엔 원래 크기).
		// ScaleBox 안에서는 배치 폭이 아니라 원하는 크기를 받으므로 줄바꿈 폭을 카드 안쪽 폭(612 - 28x2)으로 못 박는다
		UScaleBox* SlotFit = B.Make<UScaleBox>(TEXT("RewardSlotFit"));
		SlotFit->SetStretch(EStretch::ScaleToFit);
		SlotFit->SetStretchDirection(EStretchDirection::DownOnly);
		SlotFit->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddO(SlotArea, SlotFit, FMargin(28.f, 20.f, 28.f, 6.f), HAlign_Fill, VAlign_Fill);
		UWrapBox* SlotBox = B.Make<UWrapBox>(TEXT("RewardSlotBox"));
		SlotBox->SetHorizontalAlignment(HAlign_Center);
		SlotBox->SetInnerSlotPadding(FVector2D(26.f, 18.f));
		SlotBox->SetExplicitWrapSize(true);
		SlotBox->SetWrapSize(556.f);
		SlotBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SlotFit->SetContent(SlotBox);
		AddV(RewardColumn, SlotArea, FMargin(0.f), true);
		// 줄바꿈 폭을 카드 안쪽 폭(612 - 36x2)으로 못 박는다 — C++ 이 이 폭으로 재서 괄호 설명을 통째로 다음 줄로 넘긴다
		UCommonTextBlock* Unlock = B.Text(TEXT("UnlockText"), B.Bold(38), Hex(0x5E4210), FText::FromString(TEXT("세계지도 개방 (채광 · 무역)")), ETextJustify::Center);
		Unlock->SetWrapTextAt(540.f);
		AddV(RewardColumn, Unlock, FMargin(36.f, 6.f, 36.f, 26.f), false, HAlign_Fill, VAlign_Center);
		USizeBox* ActSize = B.Size(TEXT("CTAButtonSize"), 0.f, 132.f);
		UOverlay* Act = B.Overlay(TEXT("CTAStack"));
		Act->SetClipping(EWidgetClipping::ClipToBounds);
		SetSizeChild(ActSize, Act);
		UUserWidget* Button = LevelUpButton ? LevelUpButton : B.MakeUser(CostButtonClass, TEXT("LevelUpButton"));
		AddO(Act, Button);
		if (ShineImage)
		{
			AddO(Act, ShineImage, FMargin(0.f), HAlign_Center, VAlign_Fill);
		}
		AddV(RewardColumn, ActSize, FMargin(25.f, 0.f, 25.f, 27.f));

		// ---- 최고 레벨 본문 ----
		UVerticalBox* MaxBody = B.VBox(TEXT("MaxBody"));
		AddV(Content, MaxBody, FMargin(0.f, 22.f, 0.f, 0.f), true);
		AddV(MaxBody, SectionTitle(B, TEXT("MaxTitleText"), NSLOCTEXT("HQ", "LevelRecordTitle", "레벨별 해금 기록")), FMargin(0.f, 0.f, 0.f, 20.f));
		AddV(MaxBody, B.HBox(TEXT("LevelStepBox")), FMargin(0.f), true);
		MaxBody->SetVisibility(ESlateVisibility::Collapsed);
		return Page;
	}

	static UWidget* BuildOperationsPage(FBuilder& B, UClass* KpiClass)
	{
		const FAssets& A = *B.A;
		UVerticalBox* Content = nullptr;
		UWidget* Page = PageShell(B, TEXT("Operations"), Content);
		AddV(Content, SectionTitle(B, TEXT("OverviewTitleText"), NSLOCTEXT("HQ", "CompanyOverviewTitle", "회사 현황")), FMargin(0.f, 0.f, 0.f, 20.f));
		UHorizontalBox* Kpis = B.HBox(TEXT("KpiRow"));
		AddV(Content, Kpis);
		struct FKpi { const TCHAR* Name; const TCHAR* Label; UTexture2D* Icon; };
		const FKpi KpiDefs[] = {
			{ TEXT("Kpi_Money"), TEXT("보유 자금"), A.Money },
			{ TEXT("Kpi_MarketCap"), TEXT("시가총액"), A.MarketCap },
			{ TEXT("Kpi_Buildings"), TEXT("보유 빌딩"), A.Building },
			{ TEXT("Kpi_Employees"), TEXT("전체 직원"), A.Employees },
		};
		FTextProperty* LabelProperty = FindFProperty<FTextProperty>(UHQKpiCellWidget::StaticClass(), TEXT("Label"));
		FObjectProperty* IconProperty = FindFProperty<FObjectProperty>(UHQKpiCellWidget::StaticClass(), TEXT("Icon"));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(KpiDefs); ++Index)
		{
			UUserWidget* Cell = B.MakeUser(KpiClass, KpiDefs[Index].Name);
			if (!Cell) continue;
			if (LabelProperty) LabelProperty->SetPropertyValue_InContainer(Cell, FText::FromString(KpiDefs[Index].Label));
			if (IconProperty) IconProperty->SetObjectPropertyValue_InContainer(Cell, KpiDefs[Index].Icon);
			AddH(Kpis, Cell, FMargin(Index == 0 ? 0.f : 12.f, 0.f, Index == 3 ? 0.f : 12.f, 0.f), true);
		}
		UHorizontalBox* TitleRow = B.HBox(TEXT("IndustryTitleRow"));
		USizeBox* TitleSize = B.Size(TEXT("IndustryTitleSize"), 0.f, 48.f);
		SetSizeChild(TitleSize, TitleRow, HAlign_Left, VAlign_Center);
		AddH(TitleRow, B.Text(TEXT("IndustryTitleText"), B.Bold(36), Hex(0x16202C), NSLOCTEXT("HQ", "IndustryTitle", "업종별 운영")), FMargin(0.f), false, HAlign_Left, VAlign_Bottom);
		AddH(TitleRow, B.Text(TEXT("IndustryCountText"), B.Regular(30), Hex(0x5A6B7D), FText::FromString(TEXT("진출 2 / 4"))),
			FMargin(18.f, 0.f, 0.f, 3.f), false, HAlign_Left, VAlign_Bottom);
		AddV(Content, TitleSize, FMargin(0.f, 22.f, 0.f, 20.f));
		AddV(Content, B.HBox(TEXT("IndustryCardBox")), FMargin(0.f), true);
		return Page;
	}

	static UWidget* BuildBuildingsPage(FBuilder& B, UScrollBox* ListScroll, UButtonWidget* SortLevel, UButtonWidget* SortIncome, UButtonWidget* SortEmployee, UCommonTextBlock* HintText)
	{
		UVerticalBox* Content = nullptr;
		UWidget* Page = PageShell(B, TEXT("Buildings"), Content);
		UHorizontalBox* Header = B.HBox(TEXT("ListHeaderRow"));
		USizeBox* HeaderSize = B.Size(TEXT("ListHeaderSize"), 0.f, 84.f);
		SetSizeChild(HeaderSize, Header);
		AddH(Header, B.Text(TEXT("ListHeaderText"), B.Bold(40), Hex(0x16202C), FText::FromString(TEXT("보유 빌딩 7개"))), FMargin(0.f), true, HAlign_Left, VAlign_Center);
		UBorder* SortPill = B.Border(TEXT("SortPill"), Rounded(Hex(0x262A30), 44.f, Hex(0x15171A), 3.f), FMargin(8.f), HAlign_Fill, VAlign_Center);
		UHorizontalBox* SortRow = B.HBox(TEXT("SortRow"));
		SortPill->SetContent(SortRow);
		for (UButtonWidget* Sort : {SortLevel, SortIncome, SortEmployee})
		{
			if (Sort) AddH(SortRow, Sort, FMargin(2.f, 0.f), false, HAlign_Fill, VAlign_Fill);
		}
		AddH(Header, SortPill, FMargin(0.f), false, HAlign_Right, VAlign_Center);
		AddV(Content, HeaderSize);

		UHorizontalBox* Cols = B.HBox(TEXT("ListColumnRow"));
		USizeBox* ColsSize = B.Size(TEXT("ListColumnSize"), 0.f, 44.f);
		SetSizeChild(ColsSize, Cols);
		const FLinearColor Mute = Hex(0x5A6B7D);
		AddH(Cols, B.Text(TEXT("ColBuilding"), B.Bold(28), Mute, NSLOCTEXT("HQ", "ColBuilding", "빌딩")), FMargin(150.f, 0.f, 0.f, 0.f), true, HAlign_Left, VAlign_Center);
		auto Col = [&](const TCHAR* Name, float Width, const FText& Label)
		{
			USizeBox* Box = B.Size(*FString::Printf(TEXT("%sSize"), Name), Width, 0.f);
			SetSizeChild(Box, B.Text(Name, B.Bold(28), Mute, Label, ETextJustify::Center), HAlign_Center, VAlign_Center);
			AddH(Cols, Box);
		};
		Col(TEXT("ColEmployees"), 280.f, NSLOCTEXT("HQ", "ColEmployees", "직원 / 정원"));
		Col(TEXT("ColIncome"), 280.f, NSLOCTEXT("HQ", "ColIncome", "초당 수익"));
		Col(TEXT("ColMove"), 190.f, FText::GetEmpty());
		AddV(Content, ColsSize, FMargin(24.f, 0.f, 34.f, 0.f));

		if (ListScroll) AddV(Content, ListScroll, FMargin(0.f, 4.f, 0.f, 12.f), true);
		if (HintText)
		{
			HintText->SetStyle(nullptr);
			HintText->SetFont(B.Regular(28));
			HintText->SetColorAndOpacity(FSlateColor(Mute));
			HintText->SetText(NSLOCTEXT("HQ", "ListHint", "빌딩 이름을 누르면 관리 창이 열립니다"));
			AddV(Content, HintText, FMargin(0.f, -8.f, 0.f, 0.f));
		}
		return Page;
	}

	static bool BuildHQPanel(const FAssets& Assets)
	{
		UWidgetBlueprint* Blueprint = LoadAsset<UWidgetBlueprint>(HQPanelPath);
		UClass* CondClass = LoadObject<UClass>(nullptr, TEXT("/Game/CompanyGrowth/UI/Elements/Common/UIE_CondStatRow.UIE_CondStatRow_C"));
		UClass* KpiClass = LoadObject<UClass>(nullptr, TEXT("/Game/CompanyGrowth/UI/Elements/HQ/UIE_HQKpiCell.UIE_HQKpiCell_C"));
		UClass* CostClass = LoadObject<UClass>(nullptr, CostButtonClassPath);
		if (!Blueprint || !CondClass || !KpiClass || !CostClass || !CondClass->IsChildOf(UHQConditionCardWidget::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] HQ panel or child classes missing (build children first)"));
			return false;
		}
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("ContentSwitcher")));
		if (!Switcher)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] ContentSwitcher missing"));
			return false;
		}
		Blueprint->Modify();

		UUserWidget* LevelUpButton = Cast<UUserWidget>(Tree->FindWidget(TEXT("LevelUpButton")));
		UImage* Shine = Cast<UImage>(Tree->FindWidget(TEXT("CTAShineImage")));
		UScrollBox* ListScroll = Cast<UScrollBox>(Tree->FindWidget(TEXT("BuildingListScrollBox")));
		UButtonWidget* SortLevel = Cast<UButtonWidget>(Tree->FindWidget(TEXT("SortLevelBtn")));
		UButtonWidget* SortIncome = Cast<UButtonWidget>(Tree->FindWidget(TEXT("SortIncomeBtn")));
		UButtonWidget* SortEmployee = Cast<UButtonWidget>(Tree->FindWidget(TEXT("SortEmployeeBtn")));
		UCommonTextBlock* Hint = Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("ListHintText")));
		if (LevelUpButton && LevelUpButton->GetClass() != CostClass) LevelUpButton = nullptr;
		if (!ListScroll || !SortLevel || !SortIncome || !SortEmployee)
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Building list widgets missing"));
			return false;
		}

		TSet<UWidget*> Keep;
		for (UWidget* Widget : {static_cast<UWidget*>(LevelUpButton), static_cast<UWidget*>(Shine), static_cast<UWidget*>(ListScroll),
			static_cast<UWidget*>(SortLevel), static_cast<UWidget*>(SortIncome), static_cast<UWidget*>(SortEmployee), static_cast<UWidget*>(Hint)})
		{
			if (Widget)
			{
				Keep.Add(Widget);
				Widget->RemoveFromParent();
			}
		}
		// 스크롤 안 런타임 행은 저작 트리에 없다 — 저작 미리보기 자식만 비운다
		TArray<UWidget*> ScrollChildren;
		UWidgetTree::GetChildWidgets(ListScroll, ScrollChildren);
		for (UWidget* Child : ScrollChildren) Retire(Tree, Child);

		TArray<UWidget*> Pages;
		for (int32 Index = 0; Index < Switcher->GetChildrenCount(); ++Index) Pages.Add(Switcher->GetChildAt(Index));
		for (UWidget* PageWidget : Pages) RetireSubtree(Tree, PageWidget, Keep);
		// 이전 저작이 남긴 같은 이름(텍스트·탭 알림 점 등)을 먼저 비운다
		for (const TCHAR* Stale : {TEXT("ListHeaderText"), TEXT("CurLevelText"), TEXT("OverallProgressBar"), TEXT("OverallProgressText"),
			TEXT("RewardTitleText"), TEXT("UnlockText"), TEXT("LevelBadgeGloss"),
			TEXT("LevelBadgeGlow"), TEXT("LevelBadgePrefix"), TEXT("CondTitleText")})
		{
			if (UWidget* Widget = Tree->FindWidget(FName(Stale)))
			{
				if (!Keep.Contains(Widget)) RetireSubtree(Tree, Widget, Keep);
			}
		}

		FBuilder B{Tree, &Assets};
		ReorderRailTabs(Tree, B);
		Switcher->AddChild(BuildGrowthPage(B, LevelUpButton, Shine, CondClass, CostClass));
		Switcher->AddChild(BuildOperationsPage(B, KpiClass));
		Switcher->AddChild(BuildBuildingsPage(B, ListScroll, SortLevel, SortIncome, SortEmployee, Hint));
		Switcher->SetActiveWidgetIndex(0);

		if (!Compile(Blueprint)) return false;
		// 해금 표에 그림 열이 없는 보상(랜덤 보상·세계지도)의 아이콘은 패널 기본값에 싣는다
		if (UHQManagePanelWidget* Defaults = Cast<UHQManagePanelWidget>(Blueprint->GeneratedClass->GetDefaultObject()))
		{
			FObjectProperty* RandomProperty = FindFProperty<FObjectProperty>(UHQManagePanelWidget::StaticClass(), TEXT("RandomRewardIcon"));
			FObjectProperty* WorldMapProperty = FindFProperty<FObjectProperty>(UHQManagePanelWidget::StaticClass(), TEXT("WorldMapRewardIcon"));
			if (!RandomProperty || !WorldMapProperty)
			{
				UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Reward icon properties missing on UHQManagePanelWidget"));
				return false;
			}
			Defaults->Modify();
			RandomProperty->SetObjectPropertyValue_InContainer(Defaults, Assets.RandomReward);
			WorldMapProperty->SetObjectPropertyValue_InContainer(Defaults, Assets.WorldMapReward);
		}
		return Compile(Blueprint) && SaveAsset(Blueprint);
	}

	// ---------- 콘솔 명령 ----------
	static void DumpTree(const TArray<FString>& Arguments)
	{
		if (!CanAuthor() || Arguments.Num() != 1) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Usage: CGR.HQ.DumpTree /Game/Path/Asset.Asset")); return; }
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Arguments[0]);
		if (!Blueprint || !Blueprint->WidgetTree) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Not a widget blueprint: %s"), *Arguments[0]); return; }
		FString Out = FString::Printf(TEXT("%s parent=%s\n"), *Blueprint->GetPathName(), *GetNameSafe(Blueprint->ParentClass));
		TFunction<void(UWidget*, int32)> Walk = [&](UWidget* Widget, int32 Depth)
		{
			if (!Widget) return;
			FString Extra;
			if (UBorder* AsBorder = Cast<UBorder>(Widget))
			{
				const FSlateBrush& Brush = AsBorder->Background;
				Extra = FString::Printf(TEXT(" brush=%s draw=%d tint=%s pad=%s"), *GetNameSafe(Brush.GetResourceObject()), static_cast<int32>(Brush.DrawAs),
					*Brush.TintColor.GetSpecifiedColor().ToString(), *FString::Printf(TEXT("(%.0f,%.0f,%.0f,%.0f)"), AsBorder->GetPadding().Left, AsBorder->GetPadding().Top, AsBorder->GetPadding().Right, AsBorder->GetPadding().Bottom));
			}
			else if (UImage* AsImage = Cast<UImage>(Widget))
			{
				Extra = FString::Printf(TEXT(" res=%s size=%s"), *GetNameSafe(AsImage->GetBrush().GetResourceObject()), *AsImage->GetBrush().ImageSize.ToString());
			}
			else if (USizeBox* AsSize = Cast<USizeBox>(Widget))
			{
				Extra = FString::Printf(TEXT(" w=%.0f h=%.0f"), AsSize->GetWidthOverride(), AsSize->GetHeightOverride());
			}
			Out += FString::Printf(TEXT("%s%s [%s] vis=%d%s\n"), *FString::ChrN(Depth * 2, TEXT(' ')), *Widget->GetName(), *Widget->GetClass()->GetName(),
				static_cast<int32>(Widget->GetVisibility()), *Extra);
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index) Walk(Panel->GetChildAt(Index), Depth + 1);
			}
		};
		Walk(Blueprint->WidgetTree->RootWidget, 0);
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("HQ3Tab"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString File = Directory / FString::Printf(TEXT("tree_%s.txt"), *Blueprint->GetName());
		FFileHelper::SaveStringToFile(Out, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogTemp, Display, TEXT("[HQAuthor] Tree dumped to %s"), *File);
	}

	static void CreateDesaturateMaterial(const TArray<FString>& Arguments)
	{
		if (!CanAuthor()) return;
		UPackage* Package = CreatePackage(DesaturatePackage);
		Package->FullyLoad();
		const FString AssetName = FPackageName::GetLongPackageAssetName(DesaturatePackage);
		UMaterial* Material = FindObject<UMaterial>(Package, *AssetName);
		if (Material)
		{
			UE_LOG(LogTemp, Display, TEXT("[HQAuthor] M_UI_IconDesaturate already exists"));
			return;
		}
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		Material = Cast<UMaterial>(Factory->FactoryCreateNew(UMaterial::StaticClass(), Package, *AssetName, RF_Public | RF_Standalone, nullptr, GWarn));
		if (!Material) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Material creation failed")); return; }
		FAssetRegistryModule::AssetCreated(Material);
		Material->MaterialDomain = MD_UI;
		Material->BlendMode = BLEND_Translucent;
		auto* Texture = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, 0));
		Texture->ParameterName = TEXT("Icon");
		Texture->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		Texture->SamplerType = SAMPLERTYPE_Color;
		auto* One = Cast<UMaterialExpressionConstant>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant::StaticClass(), -600, 250));
		One->R = 1.f;
		UMaterialExpression* Desaturate = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionDesaturation::StaticClass(), -300, 0);
		const bool bLinked = UMaterialEditingLibrary::ConnectMaterialExpressions(Texture, TEXT("RGB"), Desaturate, TEXT(""))
			&& UMaterialEditingLibrary::ConnectMaterialExpressions(One, TEXT(""), Desaturate, TEXT("Fraction"))
			&& UMaterialEditingLibrary::ConnectMaterialProperty(Desaturate, TEXT(""), MP_EmissiveColor)
			&& UMaterialEditingLibrary::ConnectMaterialProperty(Texture, TEXT("A"), MP_Opacity);
		if (!bLinked) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Desaturate graph connection failed")); return; }
		UMaterialEditingLibrary::RecompileMaterial(Material);
		SaveAsset(Material);
	}

	static void AssignPanelIcons(const TArray<FString>& Arguments)
	{
		if (!CanAuthor()) return;
		UDataTable* Table = LoadAsset<UDataTable>(CompanyTablePath);
		if (!Table || Table->GetRowStruct() != FCompanyInfoTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] DT_CompanyInfo missing, wrong struct or dirty"));
			return;
		}
		const TPair<const TCHAR*, const TCHAR*> Icons[] = {
			{ TEXT("Game"), TEXT("/Game/CompanyGrowth/UI/Textures/Industry/T_IndustryIcon_Game.T_IndustryIcon_Game") },
			{ TEXT("IT"), TEXT("/Game/CompanyGrowth/UI/Textures/Industry/T_IndustryIcon_IT.T_IndustryIcon_IT") },
			{ TEXT("Electronics"), TEXT("/Game/CompanyGrowth/UI/Textures/Industry/T_IndustryIcon_Electronics.T_IndustryIcon_Electronics") },
			{ TEXT("Automobile"), TEXT("/Game/CompanyGrowth/UI/Textures/Industry/T_IndustryIcon_Automobile.T_IndustryIcon_Automobile") },
		};
		Table->Modify();
		int32 Updated = 0;
		for (const TPair<const TCHAR*, const TCHAR*>& Pair : Icons)
		{
			FCompanyInfoTable* Row = Table->FindRow<FCompanyInfoTable>(Pair.Key, TEXT("AssignPanelIcons"), false);
			if (!Row || !LoadObject<UTexture2D>(nullptr, Pair.Value))
			{
				UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Row or texture missing: %s"), Pair.Key);
				return;
			}
			Row->PanelIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Pair.Value));
			++Updated;
		}
		Table->HandleDataTableChanged();
		if (SaveAsset(Table))
		{
			UE_LOG(LogTemp, Display, TEXT("[HQAuthor] PanelIcon assigned to %d rows (other columns untouched)"), Updated);
		}
	}

	static void SyncUnlockDescriptions(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < 2 || (Arguments[0] != TEXT("check") && Arguments[0] != TEXT("apply")))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQUnlock] Usage: CGR.HQ.SyncUnlockDescriptions check|apply <row names>"));
			return;
		}
		if (!CanAuthor()) return;

		UDataTable* Table = LoadAsset<UDataTable>(HQLevelTablePath);
		if (!Table || Table->GetRowStruct() != FHQLevelData::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[HQUnlock] DT_HQLevel missing, wrong struct, or has unsaved edits"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport/DT_HQLevel_Import.csv"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQUnlock] Cannot read DT_HQLevel_Import.csv"));
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FHQLevelData::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[HQUnlock] CSV: %s"), *Problem);
			return;
		}

		TMap<FName, FHQLevelData> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FHQLevelData*>(Pair.Value));
		}
		TMap<FName, FHQLevelData> Expected = Before;
		TSet<FName> Requested;
		TSet<FName> ChangedRows;
		const FString TextNamespace = TextNamespaceUtil::BuildFullNamespace(
			Table->GetName(), TextNamespaceUtil::GetPackageNamespace(Table), true);
		bool bMatches = true;
		for (int32 Index = 1; Index < Arguments.Num(); ++Index)
		{
			const FName RowName(*Arguments[Index]);
			if (Requested.Contains(RowName))
			{
				UE_LOG(LogTemp, Error, TEXT("[HQUnlock] Duplicate row %s"), *Arguments[Index]);
				return;
			}
			Requested.Add(RowName);
			FHQLevelData* Target = Expected.Find(RowName);
			const FHQLevelData* Source = Incoming->FindRow<FHQLevelData>(RowName, TEXT("HQUnlock"), false);
			if (!Target || !Source)
			{
				UE_LOG(LogTemp, Error, TEXT("[HQUnlock] Missing row in asset or CSV: %s"), *Arguments[Index]);
				return;
			}
			const FString CurrentText = Target->UnlockDescription.ToString();
			const FString CsvText = Source->UnlockDescription.ToString();
			const FString TextKey = RowName.ToString() + TEXT("_UnlockDescription");
			const TOptional<FString> CurrentNamespace = FTextInspector::GetNamespace(Target->UnlockDescription);
			const TOptional<FString> CurrentKey = FTextInspector::GetKey(Target->UnlockDescription);
			const bool bIdentityMatches = CsvText.IsEmpty()
				|| (CurrentNamespace.IsSet() && CurrentNamespace.GetValue() == TextNamespace
					&& CurrentKey.IsSet() && CurrentKey.GetValue() == TextKey);
			const bool bRowMatches = CurrentText == CsvText && bIdentityMatches;
			bMatches &= bRowMatches;
			if (!bRowMatches) ChangedRows.Add(RowName);
			if (bRowMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[HQUnlock] MATCH row %s: asset='%s' CSV='%s'"),
					*RowName.ToString(), *CurrentText, *CsvText);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[HQUnlock] DIFF row %s: asset='%s' CSV='%s'"),
					*RowName.ToString(), *CurrentText, *CsvText);
			}
			// 임시 테이블의 번역 식별자가 저장된 에셋으로 넘어가지 않게 대상 테이블의 키를 사용한다.
			Target->UnlockDescription = CsvText.IsEmpty() ? FText::GetEmpty()
				: FText::ChangeKey(TextNamespace, TextKey, Source->UnlockDescription);
		}
		if (Arguments[0] == TEXT("check"))
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[HQUnlock] CHECK PASS: %d requested descriptions"), Requested.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[HQUnlock] CHECK FAIL: %d requested descriptions"), Requested.Num());
			}
			return;
		}
		if (bMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[HQUnlock] APPLY PASS: descriptions already match; no save"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDirectory = FPaths::ProjectSavedDir() / TEXT("WorldMapHQ3/Backup");
		const FString BackupFilename = BackupDirectory / TEXT("DT_HQLevel.uasset");
		if (!IFileManager::Get().FileExists(*Filename)
			|| !IFileManager::Get().MakeDirectory(*BackupDirectory, true)
			|| (!IFileManager::Get().FileExists(*BackupFilename)
				&& IFileManager::Get().Copy(*BackupFilename, *Filename, false, true) != COPY_OK))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQUnlock] Backup failed; table untouched"));
			return;
		}
		for (const FName RowName : ChangedRows) Table->AddRow(RowName, Expected[RowName]);
		bool bExact = Table->GetRowMap().Num() == Expected.Num();
		for (const TPair<FName, FHQLevelData>& Pair : Expected)
		{
			const FHQLevelData* Actual = Table->FindRow<FHQLevelData>(Pair.Key, TEXT("HQUnlock"), false);
			bExact &= Actual && FHQLevelData::StaticStruct()->CompareScriptStruct(Actual, &Pair.Value, 0);
		}
		if (bExact)
		{
			Table->HandleDataTableChanged();
		}
		if (!bExact || !SaveAsset(Table))
		{
			for (const FName RowName : Requested) Table->AddRow(RowName, Before[RowName]);
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[HQUnlock] APPLY FAIL: memory restored; backup=%s"), *BackupFilename);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[HQUnlock] APPLY PASS: %d descriptions synced; other fields preserved; backup=%s"),
			Requested.Num(), *BackupFilename);
	}

	static void SyncIndustryLevels(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < 2 || (Arguments[0] != TEXT("check") && Arguments[0] != TEXT("apply")))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQIndustry] Usage: CGR.HQ.SyncIndustryLevels check|apply <row names>"));
			return;
		}
		if (!CanAuthor()) return;

		UDataTable* Table = LoadAsset<UDataTable>(CompanyTablePath);
		if (!Table || Table->GetRowStruct() != FCompanyInfoTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[HQIndustry] DT_CompanyInfo missing, wrong struct, or has unsaved edits"));
			return;
		}
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *(FPaths::ProjectDir() / TEXT("DataImport/DT_CompanyInfo_Import.csv"))))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQIndustry] Cannot read DT_CompanyInfo_Import.csv"));
			return;
		}
		UDataTable* Incoming = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Incoming->RowStruct = FCompanyInfoTable::StaticStruct();
		const TArray<FString> Problems = Incoming->CreateTableFromCSVString(Csv);
		if (!Problems.IsEmpty())
		{
			for (const FString& Problem : Problems) UE_LOG(LogTemp, Error, TEXT("[HQIndustry] CSV: %s"), *Problem);
			return;
		}

		TMap<FName, FCompanyInfoTable> Before;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Before.Add(Pair.Key, *reinterpret_cast<const FCompanyInfoTable*>(Pair.Value));
		}
		TMap<FName, FCompanyInfoTable> Expected = Before;
		TSet<FName> Requested;
		bool bMatches = true;
		for (int32 Index = 1; Index < Arguments.Num(); ++Index)
		{
			const FName RowName(*Arguments[Index]);
			if (Requested.Contains(RowName))
			{
				UE_LOG(LogTemp, Error, TEXT("[HQIndustry] Duplicate row %s"), *Arguments[Index]);
				return;
			}
			Requested.Add(RowName);
			FCompanyInfoTable* Target = Expected.Find(RowName);
			const FCompanyInfoTable* Source = Incoming->FindRow<FCompanyInfoTable>(RowName, TEXT("HQIndustry"), false);
			if (!Target || !Source || Target->CompanyType != Source->CompanyType || Source->RequiredHQLevel < 0)
			{
				UE_LOG(LogTemp, Error, TEXT("[HQIndustry] Missing, mismatched, or invalid row %s"), *Arguments[Index]);
				return;
			}
			const int32 CurrentLevel = Target->RequiredHQLevel;
			bMatches &= CurrentLevel == Source->RequiredHQLevel;
			UE_LOG(LogTemp, Display, TEXT("[HQIndustry] Row %s: asset=%d CSV=%d"),
				*RowName.ToString(), CurrentLevel, Source->RequiredHQLevel);
			Target->RequiredHQLevel = Source->RequiredHQLevel;
		}
		if (Arguments[0] == TEXT("check"))
		{
			if (bMatches)
			{
				UE_LOG(LogTemp, Display, TEXT("[HQIndustry] CHECK PASS: %d requested levels"), Requested.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[HQIndustry] CHECK FAIL: %d requested levels"), Requested.Num());
			}
			return;
		}
		if (bMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[HQIndustry] APPLY PASS: levels already match; no save"));
			return;
		}

		UPackage* Package = Table->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const FString BackupDirectory = FPaths::ProjectSavedDir() / TEXT("WorldMapHQ3/Backup");
		const FString BackupFilename = BackupDirectory / TEXT("DT_CompanyInfo.uasset");
		if (!IFileManager::Get().FileExists(*Filename)
			|| !IFileManager::Get().MakeDirectory(*BackupDirectory, true)
			|| (!IFileManager::Get().FileExists(*BackupFilename)
				&& IFileManager::Get().Copy(*BackupFilename, *Filename, false, true) != COPY_OK))
		{
			UE_LOG(LogTemp, Error, TEXT("[HQIndustry] Backup failed; table untouched"));
			return;
		}
		for (const FName RowName : Requested)
		{
			if (Before[RowName].RequiredHQLevel != Expected[RowName].RequiredHQLevel)
			{
				Table->AddRow(RowName, Expected[RowName]);
			}
		}
		bool bExact = Table->GetRowMap().Num() == Expected.Num();
		for (const TPair<FName, FCompanyInfoTable>& Pair : Expected)
		{
			const FCompanyInfoTable* Actual = Table->FindRow<FCompanyInfoTable>(Pair.Key, TEXT("HQIndustry"), false);
			bExact &= Actual && FCompanyInfoTable::StaticStruct()->CompareScriptStruct(Actual, &Pair.Value, 0);
		}
		if (bExact) Table->HandleDataTableChanged();
		if (!bExact || !SaveAsset(Table))
		{
			for (const FName RowName : Requested) Table->AddRow(RowName, Before[RowName]);
			Table->HandleDataTableChanged();
			Package->SetDirtyFlag(false);
			UE_LOG(LogTemp, Error, TEXT("[HQIndustry] APPLY FAIL: memory restored; backup=%s"), *BackupFilename);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[HQIndustry] APPLY PASS: %d levels synced; other fields preserved; backup=%s"),
			Requested.Num(), *BackupFilename);
	}

	static void RegisterWidgets(const TArray<FString>& Arguments)
	{
		if (!CanAuthor()) return;
		UDataTable* Table = LoadAsset<UDataTable>(WidgetTablePath);
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[HQAuthor] DT_WidgetClass missing, wrong struct or dirty"));
			return;
		}
		struct FRegistration { const TCHAR* Row; EWidgetType Type; const TCHAR* ClassPath; };
		const FRegistration Registrations[] = {
			{ TEXT("HQIndustryCard"), EWidgetType::HQIndustryCard, TEXT("/Game/CompanyGrowth/UI/Elements/HQ/UIE_HQIndustryCard.UIE_HQIndustryCard_C") },
			{ TEXT("HQLevelStep"), EWidgetType::HQLevelStep, TEXT("/Game/CompanyGrowth/UI/Elements/HQ/UIE_HQLevelStep.UIE_HQLevelStep_C") },
			{ TEXT("HQRewardSlot"), EWidgetType::HQRewardSlot, TEXT("/Game/CompanyGrowth/UI/Elements/HQ/UIE_HQRewardSlot.UIE_HQRewardSlot_C") },
		};
		const int32 Before = Table->GetRowMap().Num();
		const int32 RowSize = FWidgetDataTable::StaticStruct()->GetStructureSize();
		TMap<FName, TArray<uint8>> Others;
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Others.FindOrAdd(Pair.Key).Append(Pair.Value, RowSize);
		}
		Table->Modify();
		int32 Added = 0;
		for (const FRegistration& Registration : Registrations)
		{
			UClass* WidgetClass = LoadObject<UClass>(nullptr, Registration.ClassPath);
			if (!WidgetClass) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Class missing: %s"), Registration.ClassPath); return; }
			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
				if (Existing.WidgetType == Registration.Type && Pair.Key != FName(Registration.Row))
				{
					UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Enum already used by row %s"), *Pair.Key.ToString());
					return;
				}
			}
			FWidgetDataTable Desired;
			Desired.WidgetType = Registration.Type;
			Desired.WidgetClass = WidgetClass;
			if (!Table->FindRow<FWidgetDataTable>(Registration.Row, TEXT("HQRegister"), false)) ++Added;
			Others.Remove(Registration.Row);
			Table->AddRow(Registration.Row, Desired);
		}
		bool bPreserved = Table->GetRowMap().Num() == Others.Num() + UE_ARRAY_COUNT(Registrations);
		for (const TPair<FName, TArray<uint8>>& Pair : Others)
		{
			const uint8* Current = Table->FindRowUnchecked(Pair.Key);
			bPreserved &= Current && FMemory::Memcmp(Pair.Value.GetData(), Current, Pair.Value.Num()) == 0;
		}
		if (!bPreserved) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Other widget rows changed; not saving")); return; }
		if (SaveAsset(Table))
		{
			UE_LOG(LogTemp, Display, TEXT("[HQAuthor] Widget rows %d -> %d (added %d, others byte-identical)"), Before, Table->GetRowMap().Num(), Added);
		}
	}

	static void BuildWidgets(const TArray<FString>& Arguments)
	{
		if (!CanAuthor()) return;
		FAssets Assets;
		if (!Assets.Load()) { UE_LOG(LogTemp, Error, TEXT("[HQAuthor] Asset load failed — import textures/material first")); return; }
		const bool bOk = BuildKpiCell(Assets) && BuildLevelStep(Assets) && BuildRewardSlot(Assets) && BuildIndustryCard(Assets)
			&& BuildConditionCard(Assets) && BuildBuildingSlot(Assets) && BuildHQPanel(Assets);
		UE_LOG(LogTemp, Display, TEXT("[HQAuthor] BuildWidgets %s"), bOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	}

	static FAutoConsoleCommand DumpTreeCommand(TEXT("CGR.HQ.DumpTree"), TEXT("Dump a widget blueprint tree to Saved/HQ3Tab"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpTree));
	static FAutoConsoleCommand DesaturateCommand(TEXT("CGR.HQ.CreateDesaturateMaterial"), TEXT("Create M_UI_IconDesaturate"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CreateDesaturateMaterial));
	static FAutoConsoleCommand PanelIconCommand(TEXT("CGR.HQ.AssignPanelIcons"), TEXT("Fill DT_CompanyInfo.PanelIcon for launch industries"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AssignPanelIcons));
	static FAutoConsoleCommand RegisterCommand(TEXT("CGR.HQ.RegisterWidgets"), TEXT("Register HQ dynamic widgets in DT_WidgetClass"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterWidgets));
	static FAutoConsoleCommand SyncUnlockDescriptionsCommand(TEXT("CGR.HQ.SyncUnlockDescriptions"),
		TEXT("Compare or merge selected DT_HQLevel.UnlockDescription cells from DataImport/DT_HQLevel_Import.csv: check|apply <row names>."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SyncUnlockDescriptions));
	static FAutoConsoleCommand SyncIndustryLevelsCommand(TEXT("CGR.HQ.SyncIndustryLevels"),
		TEXT("Compare or merge selected DT_CompanyInfo.RequiredHQLevel cells from DataImport/DT_CompanyInfo_Import.csv: check|apply <row names>."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SyncIndustryLevels));
	static FAutoConsoleCommand BuildCommand(TEXT("CGR.HQ.BuildWidgets"), TEXT("Author HQ 3-tab widget trees from the approved mockup"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BuildWidgets));
}

#endif
