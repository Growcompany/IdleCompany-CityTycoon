#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Enum/WidgetType.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "AssetCompilingManager.h"
#include "JsonObjectConverter.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "Table/CompanyInfoTable.h"
#include "Table/FusionProjectTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/EmptyStateWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/Fusion/FusionBuildingRowWidget.h"
#include "UI/Element/Fusion/FusionCardWidget.h"
#include "UI/Element/Fusion/FusionSlotWidget.h"
#include "UI/Panel/FusionProjectPanelWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace FusionProjectRenderValidation
{
	static constexpr int32 Height = 1440;
	static const TCHAR* States[] = { TEXT("unassigned"), TEXT("partial_industry"), TEXT("ready"),
		TEXT("short_power"), TEXT("missing_industry"), TEXT("three_slot"), TEXT("running"),
		TEXT("claim_ready"), TEXT("time_warning"), TEXT("no_crew") };

	struct FLayoutBox
	{
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		bool Positive() const { return Size.X > 0.0 && Size.Y > 0.0; }
		bool Inside(const FLayoutBox& OuterBox) const
		{
			return Positive() && OuterBox.Positive() && Position.X >= OuterBox.Position.X - 1.0
				&& Position.Y >= OuterBox.Position.Y - 1.0
				&& Position.X + Size.X <= OuterBox.Position.X + OuterBox.Size.X + 1.0
				&& Position.Y + Size.Y <= OuterBox.Position.Y + OuterBox.Size.Y + 1.0;
		}
	};

	static FLayoutBox Measure(UUserWidget& Panel, UWidget* Widget)
	{
		FLayoutBox Result;
		if (!Widget || !Panel.GetRootWidget()) return Result;
		const FGeometry& RootGeometry = Panel.GetRootWidget()->GetCachedGeometry();
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		Result.Position = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
		Result.Size = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize())) - Result.Position;
		return Result;
	}

	static TSharedPtr<FJsonObject> BoxJson(const FLayoutBox& Box)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("x"), Box.Position.X);
		Result->SetNumberField(TEXT("y"), Box.Position.Y);
		Result->SetNumberField(TEXT("width"), Box.Size.X);
		Result->SetNumberField(TEXT("height"), Box.Size.Y);
		return Result;
	}

	template<typename T> T* Required(UUserWidget& Panel, const TCHAR* Name, FAutomationTestBase& Test)
	{
		T* Widget = Cast<T>(Panel.GetWidgetFromName(FName(Name)));
		Test.TestNotNull(FString::Printf(TEXT("%s binding %s"), *Panel.GetName(), Name), Widget);
		return Widget;
	}

	static bool SetText(UUserWidget& Panel, const TCHAR* Name, const FText& Text, FAutomationTestBase& Test)
	{
		UCommonTextBlock* Widget = Required<UCommonTextBlock>(Panel, Name, Test);
		if (!Widget) return false;
		Widget->SetText(Text);
		return Test.TestEqual(FString::Printf(TEXT("%s fixture text"), Name), Widget->GetText().ToString(), Text.ToString());
	}

	static bool SetText(UUserWidget& Panel, const TCHAR* Name, const TCHAR* Text, FAutomationTestBase& Test)
	{
		return SetText(Panel, Name, FText::FromString(Text), Test);
	}

	static bool Draw(const TSharedRef<SWidget>& SlateWidget, int32 Width, const FString& Filename)
	{
		FWidgetRenderer Renderer(true);
		const FVector2D DrawSize(Width, Height);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
		if (!Target.IsValid()) return false;
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), SlateWidget, 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		FImage Image;
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), Image) || Image.Format != ERawImageFormat::BGRA8) return false;
		Image.GammaSpace = EGammaSpace::sRGB;
		TArray64<uint8> Png;
		return FImageUtils::CompressImage(Png, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Png, *Filename);
	}

	// 셰이더 맵이 완성되기 전에 DrawWidget 을 돌리면 그 면만 **조용히 빠진다** — 기하·단언은 전부
	// 통과하고 픽셀만 없다. 4b 의 30컷에서 셸(MI_Mailbox_shell)·크림 판(MI_Pitch_TileFace)·골드 CTA
	// 면이 전부 `(0,0,0,140)`(= BackgroundBtn 딤만, 즉 아무것도 안 그려짐)이었다.
	// 에셋 결함이 아니다 — 같은 `UIE_ModalFrame` 이 에디터 하네스에서는 그려진다(피어 캡처
	// uiaudit1001/1005/1006 = 셸 면 (38,40,43,255) / 1004 = (0,0,0,140), 같은 에셋·같은 하네스).
	// 프로세스 상태다: `-game` 에는 이 UI 머티리얼을 쓰는 다른 화면이 없어 셰이더 맵이 한 번도
	// 완성되지 않은 채 30컷이 끝난다. 에디터는 자기 UI 가 이미 그것들을 데워 둔다.
	// ⚠ 에셋 컴파일러는 메시·텍스처만 기다린다 — 머티리얼 셰이더 맵은 **자기 컴파일러를 따로** 가진다
	// (선례: EmployeeEquipmentIconCapture.cpp 의 Toon 워밍업이 같은 함정을 같은 방법으로 막는다).
	static TSharedPtr<FJsonObject> WarmUserInterfaceMaterials(FAutomationTestBase& Test)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		FAssetCompilingManager::Get().FinishAllCompilation();
		TArray<UMaterialInterface*> Targets;
		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			UMaterialInterface* Material = *It;
			const UMaterial* Base = Material ? Material->GetMaterial() : nullptr;
			if (Base && Base->MaterialDomain == MD_UI) { Targets.Add(Material); }
		}
		TArray<TSharedPtr<FJsonValue>> Rows;
		TSet<UMaterial*> Recompiled;
		int32 Requested = 0;
		int32 Incomplete = 0;
		for (UMaterialInterface* Material : Targets)
		{
			FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIFeatureLevel);
			if (Resource) { Resource->FinishCompilation(); }
			const bool bWasIncomplete = !Resource || !Resource->IsGameThreadShaderMapComplete();
			if (bWasIncomplete)
			{
				// 기다리는 것만으로는 안 된다 — 아무도 요청하지 않은 셰이더 맵은 영원히 미완성이다.
				UMaterial* Base = Material->GetMaterial();
				if (Base && !Recompiled.Contains(Base))
				{
					FMaterialUpdateContext UpdateContext;
					UpdateContext.AddMaterial(Base);
					Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
					Recompiled.Add(Base);
				}
				Resource = Material->GetMaterialResource(GMaxRHIFeatureLevel);
				if (Resource) { Resource->FinishCompilation(); }
				++Requested;
			}
			const bool bComplete = Resource && Resource->IsGameThreadShaderMapComplete();
			if (!bComplete) { ++Incomplete; }
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("material"), Material->GetPathName());
			Row->SetBoolField(TEXT("was_incomplete"), bWasIncomplete);
			Row->SetBoolField(TEXT("complete"), bComplete);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		FlushRenderingCommands();
		// 미완성이 남으면 캡처의 "면이 보인다/안 보인다" 판정을 믿을 수 없다 — 조용히 넘기지 않는다.
		// 하드 실패로 걸지 않는 이유: 이 루프는 **로드된 UI 머티리얼 전부**를 돈다. 한 장이라도
		// 남의 깨진 에셋이면 콜라보 테스트가 그 때문에 빨개진다(내 범위 밖의 영구 실패).
		// 대신 로그와 manifest 에 남긴다 — 캡처의 픽셀 판정은 still_incomplete == 0 일 때만 유효하다.
		if (Incomplete > 0)
		{
			Test.AddWarning(FString::Printf(
				TEXT("%d UI material(s) still have an incomplete shader map; face pixels may be missing from these captures."),
				Incomplete));
		}
		Result->SetNumberField(TEXT("ui_materials"), Targets.Num());
		Result->SetNumberField(TEXT("compilation_requested"), Requested);
		Result->SetNumberField(TEXT("still_incomplete"), Incomplete);
		Result->SetArrayField(TEXT("materials"), Rows);
		return Result;
	}

	static UGameInstance* FindGameInstance()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				if (UWorld* GameWorld = Context.World())
				{
					if (UGameInstance* Instance = GameWorld->GetGameInstance()) return Instance;
				}
			}
		}
		return nullptr;
	}

	// 3열 보드는 pane 이 없다 — 레일·배치·상세를 한 화면에 동시에 세우므로 상태 분기가 픽스처 하나로 합쳐진다.
	static bool FillBoard(UFusionProjectPanelWidget& Panel, UGameInstance& GI, UTableManagerSubsystem& Tables,
		int32 State, FAutomationTestBase& Test, TArray<UWidget*>& Bounds)
	{
		const bool bThree = State == 5;
		const bool bRunning = State == 6 || State == 7 || State == 8;
		const bool bMissing = State == 4;        // 요구 업종을 하나도 안 가진 경우 = 후보 0
		const bool bPartial = State == 1;        // 두 업종 중 하나만 미보유 = 후보는 살아 있어야 한다
		const bool bShortPower = State == 3;
		const bool bNoCrew = State == 9;
		const bool bStartable = State == 2 || State == 5;

		UVerticalBox* Rail = Required<UVerticalBox>(Panel, TEXT("CollabRail"), Test);
		UHorizontalBox* SlotRow = Required<UHorizontalBox>(Panel, TEXT("SlotBox"), Test);
		UUniformGridPanel* Grid = Required<UUniformGridPanel>(Panel, TEXT("CandidateGrid"), Test);
		UHorizontalBox* Rewards = Required<UHorizontalBox>(Panel, TEXT("RewardBox"), Test);
		UHorizontalBox* Powers = Required<UHorizontalBox>(Panel, TEXT("PowerColumnBox"), Test);
		UEmptyStateWidget* Empty = Required<UEmptyStateWidget>(Panel, TEXT("EmptyBuildings"), Test);
		UWidget* InfoWell = Required<UWidget>(Panel, TEXT("InfoWell"), Test);
		UWidget* RunWell = Required<UWidget>(Panel, TEXT("RunWell"), Test);
		UProgressBar* Progress = Required<UProgressBar>(Panel, TEXT("ProgressBar"), Test);
		// CTA 는 여기서 한 번만 잡는다 — 아래 상태별 단언이 널 포인터를 역참조하면 자동화가 통째로 죽는다.
		UButtonWidget* Start = Required<UButtonWidget>(Panel, TEXT("StartButton"), Test);
		UButtonWidget* Claim = Required<UButtonWidget>(Panel, TEXT("ClaimButton"), Test);
		if (!Rail || !SlotRow || !Grid || !Rewards || !Powers || !Empty || !InfoWell || !RunWell
			|| !Progress || !Start || !Claim) return false;

		const TSubclassOf<UUserWidget> CardClass = Tables.GetWidgetClass(EWidgetType::FusionCard);
		const TSubclassOf<UUserWidget> TileClass = Tables.GetWidgetClass(EWidgetType::FusionBuildingRow);
		const TSubclassOf<UUserWidget> SlotClass = Tables.GetWidgetClass(EWidgetType::FusionSlot);
		if (!Test.TestNotNull(TEXT("FusionCard registered in DT_WidgetClass"), CardClass.Get())) return false;
		if (!Test.TestNotNull(TEXT("FusionBuildingRow registered in DT_WidgetClass"), TileClass.Get())) return false;
		if (!Test.TestNotNull(TEXT("FusionSlot registered in DT_WidgetClass"), SlotClass.Get())) return false;

		// 업종 표시명은 DT 가 단일 진실 소스 — 이름을 픽스처에 적어 넣으면 행과 화면이 조용히 갈린다.
		const auto IndustryName = [&Tables](ECompanyType Industry) -> FText
		{
			bool bFound = false;
			const FCompanyInfoTable Info = Tables.GetCompanyInfo(Industry, bFound);
			return bFound ? Info.DisplayName : FText::GetEmpty();
		};

		// 오른쪽 열의 준비/진행 well 교체가 pane 전환을 대신하는 유일한 분기다.
		InfoWell->SetVisibility(bRunning ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		RunWell->SetVisibility(bRunning ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		// 후보가 전부 비었을 때만 안내가 뜬다 — 부분 미보유(Review Focus 1)는 그리드가 살아 있어야 한다.
		Empty->SetVisibility(bMissing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Grid->SetVisibility(bMissing ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

		Rail->ClearChildren();
		// 제품 레일은 GetAvailableFusionKeys() 로 미가용 업종 요구 행을 빼지만(출시 로스터에서 3종),
		// 레이아웃 판정은 DT 원본 7행을 그대로 세운다 — 스크롤·첫행 가시성의 최악 케이스가 여기다.
		const TArray<FName>& Keys = Tables.GetFusionProjectOrder();
		if (!Test.TestEqual(TEXT("Seven collaboration definitions"), Keys.Num(), 7)) return false;
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			FFusionProjectTable CardRow;
			if (!Tables.GetFusionProject(Keys[Index], CardRow)) return false;
			UFusionCardWidget* Card = CreateWidget<UFusionCardWidget>(&GI, CardClass);
			if (!Test.TestNotNull(TEXT("Registered FusionCard class creates widget"), Card)) return false;
			Card->TakeWidget();
			if (UVerticalBoxSlot* BoxSlot = Rail->AddChildToVerticalBox(Card))
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Index == Keys.Num() - 1 ? 0.f : 6.f));
			const bool bLocked = Index >= 4;
			const bool bSelected = Index == 0;
			const int32 CardLevel = Index == 2 ? 4 : Index % 3;
			// 잠금 사유의 업종명도 그 행에서 뽑는다 — DisplayOrder 가 바뀌면 적어 둔 이름은 거짓이 된다.
			const ECompanyType LockedIndustry = CardRow.RequiredIndustries.Num() > 0
				? CardRow.RequiredIndustries.Last() : ECompanyType::None;
			const FText Status = bLocked
				? FText::Format(FText::FromString(TEXT("{0} 건물 필요")), IndustryName(LockedIndustry))
				: FText::FromString(bSelected ? TEXT("선택 중") : TEXT("선택하여 개발"));
			Card->Configure(Keys[Index], CardRow, CardLevel, FusionRules::Requirement(CardRow, CardLevel),
				FusionRules::Rewards(CardRow, CardLevel), Status, bLocked, bSelected);
			Test.TestTrue(TEXT("Locked cards remain available for requirement preview"), Card->IsInteractionEnabled());
			if (UCommonTextBlock* StatusText = Required<UCommonTextBlock>(*Card, TEXT("StatusText"), Test))
				Test.TestEqual(TEXT("Card status survives Configure"), StatusText->GetText().ToString(), Status.ToString());
			if (UImage* Ring = Required<UImage>(*Card, TEXT("SelectedRing"), Test))
				Test.TestEqual(TEXT("Rail marks the selected collaboration"), Ring->GetVisibility(),
					bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			// 카드를 Bounds 에 넣지 않는다 — 레일은 스크롤한다(좌열 953 < 7행 1023, 확정 2026-10-01)므로
			// 아래쪽 행이 뷰포트 밖에 놓이는 것이 정상이다. 수용 판정은 RunTest 의 레일 블록이 한다.
		}
		Test.TestEqual(TEXT("Rail lists every collaboration in one column"), Rail->GetChildrenCount(), Keys.Num());

		FFusionProjectTable Row;
		// 2슬롯 픽스처 = autodrive(IT·자동차, 520). game_platform 은 게임·IT 라 아래 상세 문구와 어긋난다.
		if (!Tables.GetFusionProject(bThree ? FName(TEXT("smartcar")) : FName(TEXT("autodrive")), Row)) return false;
		const int32 Level = 2;
		const float RequiredPower = FusionRules::Requirement(Row, Level);
		const int32 SlotCount = Row.RequiredIndustries.Num();
		Test.TestEqual(TEXT("Fixture row has the expected industry count"), SlotCount, bThree ? 3 : 2);

		// 미보유는 이 인덱스부터 — 전부 미보유(4)는 0 번부터, 부분 미보유(1)는 1 번만.
		const int32 MissingSlotStart = bMissing ? 0 : bPartial ? 1 : INDEX_NONE;
		// 생산의 MissingIndustryText 와 같은 규약: 업종을 못 찾으면 빈 FText(안내 자체가 접힌다).
		const FText MissingName = IndustryName(Row.RequiredIndustries.IsValidIndex(MissingSlotStart)
			? Row.RequiredIndustries[MissingSlotStart] : ECompanyType::None);
		const FText MissingMessage = MissingName.IsEmpty() ? FText::GetEmpty()
			: FText::Format(FText::FromString(TEXT("{0} 건물 필요")), MissingName);
		Empty->SetMessage(MissingMessage);
		// SetMessage 가 내부 EmptyMessage 까지 흐르는지 — 부품이 문구를 삼키면 안내가 빈 칸으로 렌더된다.
		if (UCommonTextBlock* EmptyMessage = Required<UCommonTextBlock>(*Empty, TEXT("EmptyMessage"), Test))
			Test.TestEqual(TEXT("Empty state propagates the missing industry message"),
				EmptyMessage->GetText().ToString(), MissingMessage.ToString());

		SlotRow->ClearChildren();
		TArray<float> SlotPowers;
		TArray<int32> SlotBuildings;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			UFusionSlotWidget* Entry = CreateWidget<UFusionSlotWidget>(&GI, SlotClass);
			if (!Test.TestNotNull(TEXT("Registered FusionSlot class creates widget"), Entry)) return false;
			Entry->TakeWidget();
			if (UHorizontalBoxSlot* BoxSlot = SlotRow->AddChildToHorizontalBox(Entry))
			{
				BoxSlot->SetPadding(FMargin(Index == 0 ? 0.f : 8.f, 0.f, 0.f, 0.f));
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			const bool bSlotMissing = MissingSlotStart != INDEX_NONE && Index >= MissingSlotStart;
			const bool bAssigned = !bSlotMissing && State != 0 && !bNoCrew;
			const float Power = !bAssigned ? 0.f
				: bShortPower && Index == 0 ? RequiredPower * .72f : RequiredPower * (1.15f + Index * .1f);
			const int32 BuildingIndex = bAssigned ? 100 + Index : INDEX_NONE;
			Entry->Configure(Index, Row.RequiredIndustries[Index], BuildingIndex, Power, RequiredPower,
				Index == 0 && !bRunning, bSlotMissing);
			// Configure 가 Power 와 Requirement 로 상태 문구를 직접 유도하는지 — 픽스처가 쓴 값을 되읽는 게 아니다.
			const FText ExpectedState = bSlotMissing
				? FText::Format(FText::FromString(TEXT("{0} 건물 필요")), IndustryName(Row.RequiredIndustries[Index]))
				: !bAssigned ? FText::FromString(TEXT("건물을 골라 주세요"))
				: Power >= RequiredPower ? FText::FromString(TEXT("조건 충족")) : FText::FromString(TEXT("팀 파워 부족"));
			if (UCommonTextBlock* StateText = Required<UCommonTextBlock>(*Entry, TEXT("StateText"), Test))
				Test.TestEqual(TEXT("Slot state matches power"), StateText->GetText().ToString(), ExpectedState.ToString());
			SlotPowers.Add(Power);
			SlotBuildings.Add(BuildingIndex);
			Bounds.Add(Entry);
		}
		Test.TestEqual(TEXT("Every required industry has a visible slot"), SlotRow->GetChildrenCount(), SlotCount);

		Grid->ClearChildren();
		const int32 TileCount = bMissing ? 0 : 4;
		const ECompanyType TileIndustry = Row.RequiredIndustries.IsValidIndex(0) ? Row.RequiredIndustries[0] : ECompanyType::None;
		for (int32 Index = 0; Index < TileCount; ++Index)
		{
			UFusionBuildingRowWidget* Tile = CreateWidget<UFusionBuildingRowWidget>(&GI, TileClass);
			if (!Test.TestNotNull(TEXT("Registered FusionBuildingRow class creates widget"), Tile)) return false;
			Tile->TakeWidget();
			if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Tile, Index / 4, Index % 4))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
			// no_crew 는 직원 0명 건물만 보유한 상태 — 타일은 남고 Configure 가 상호작용만 닫는다.
			const int32 CrewCount = bNoCrew ? 0 : 4 - Index;
			const float TilePower = bShortPower ? RequiredPower * .72f : RequiredPower * (1.3f - Index * .2f);
			Tile->Configure(100 + Index, TileIndustry, TilePower, CrewCount, RequiredPower, Index == 0 && !bNoCrew, false);
			Bounds.Add(Tile);
		}
		Test.TestEqual(TEXT("Candidate grid lists the owned buildings"), Grid->GetChildrenCount(), TileCount);

		// 파워 칸 3벌은 WBP 저작물이다 — 비워 두면 PowerColumnBox 가 0 으로 측정되고 캡처에도 빈 띠만 남는다.
		// 값 규칙은 생산의 RefreshPowerColumns 와 같게 맞춘다.
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UWidget* Column = Panel.GetWidgetFromName(FName(*FString::Printf(TEXT("PowerCol%d"), Index)));
			if (!Column) continue;
			// 슬롯이 2개면 3번째 칸은 자리까지 회수한다 — Hidden 이면 폭이 남는다.
			if (Index >= SlotCount) { Column->SetVisibility(ESlateVisibility::Collapsed); continue; }
			Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			const float Ratio = RequiredPower > 0.f ? SlotPowers[Index] / RequiredPower : 0.f;
			if (UCommonTextBlock* Pct = Required<UCommonTextBlock>(Panel, *FString::Printf(TEXT("PowerPct%d"), Index), Test))
				Pct->SetText(SlotBuildings[Index] == INDEX_NONE
					? FText::FromString(TEXT("―"))   // U+2015 — NEXON 에 U+2014 글리프가 없다
					: FText::Format(FText::FromString(TEXT("{0}%")), FText::AsNumber(FMath::RoundToInt(Ratio * 100.f))));
			if (UProgressBar* Meter = Required<UProgressBar>(Panel, *FString::Printf(TEXT("PowerMeter%d"), Index), Test))
				Meter->SetPercent(FMath::Clamp(Ratio, 0.f, 1.f));
			if (UCommonTextBlock* ColumnName = Required<UCommonTextBlock>(Panel, *FString::Printf(TEXT("PowerName%d"), Index), Test))
				ColumnName->SetText(IndustryName(Row.RequiredIndustries[Index]));
		}

		CGRewardChip::RenderIcons(*Panel.WidgetTree, &GI, *Rewards, FusionRules::Rewards(Row, Level));

		// 상세 문구는 전부 로드한 행에서 유도한다 — 적어 넣으면 행을 바꾼 순간 캡처가 조용히 거짓이 된다.
		TArray<FString> Names;
		for (ECompanyType Industry : Row.RequiredIndustries)
		{
			const FText Name = IndustryName(Industry);
			if (!Name.IsEmpty()) Names.Add(Name.ToString());
		}
		bool bOk = SetText(Panel, TEXT("DetailNameText"), Row.DisplayName, Test);
		bOk &= SetText(Panel, TEXT("DetailIndustryText"), FText::FromString(FString::Join(Names, TEXT(" + "))), Test);
		bOk &= SetText(Panel, TEXT("PowerLabelText"), FText::Format(FText::FromString(TEXT("현재 팀 파워 (요구 {0})")),
			FText::AsNumber(FMath::CeilToInt(RequiredPower))), Test);
		bOk &= SetText(Panel, TEXT("CandidateHintText"), bRunning ? FText::FromString(TEXT("진행 중에는 바꿀 수 없습니다"))
			: FText::Format(FText::FromString(TEXT("{0} 업종만 배치할 수 있습니다")),
				FText::FromString(FString::Join(Names, TEXT(" · ")))), Test);
		bOk &= SetText(Panel, TEXT("RewardTitleText"), TEXT("완료 보상"), Test);
		bOk &= SetText(Panel, TEXT("RewardHintText"), FText::Format(FText::FromString(TEXT("Lv.{0} → Lv.{1} 상승합니다")),
			FText::AsNumber(Level), FText::AsNumber(Level + 1)), Test);
		// 진행 중에는 차단 사유가 없다(생산도 같다) — 서버 시간 안내는 ServerTimeWarn 밴드가 맡는다.
		bOk &= SetText(Panel, TEXT("BlockMessageText"), bRunning ? TEXT("")
			: State == 0 || State == 1 ? TEXT("모든 업종에 건물을 배치해야 합니다")
			: State == 3 ? TEXT("팀 파워가 요구치에 미치지 못합니다")
			: State == 9 ? TEXT("직원이 배치된 건물이 필요합니다") : TEXT(""), Test);
		// RunWell 의 실제 표시물 — 진행 상태 9컷의 우열 내용은 이 둘이 전부다.
		bOk &= SetText(Panel, TEXT("RemainingText"), State == 6 ? TEXT("02:19:12") : TEXT("완료"), Test);
		Progress->SetPercent(State == 6 ? .42f : 1.f);

		// 두 CTA 는 CTAStack 안에서 겹쳐 있다 — 하나를 접지 않으면 30컷 전부에서 두 버튼이 포개진다.
		Start->SetVisibility(bRunning ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Start->SetButtonText(FText::FromString(TEXT("개발 시작")));
		Start->SetIsEnabled(bStartable);
		Claim->SetVisibility(bRunning ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Claim->SetButtonText(FText::FromString(State == 7 ? TEXT("수령") : TEXT("진행 중")));
		Claim->SetIsEnabled(State == 7);
		Test.TestEqual(TEXT("Start CTA opens only when every slot is satisfied"), Start->GetIsEnabled(), bStartable);
		Test.TestEqual(TEXT("Claim CTA has authority only in claim_ready"), Claim->GetIsEnabled(), State == 7);

		// Review Focus 1 — 부분 미보유에서 안내가 그리드를 덮어 버리면 남은 업종을 고를 수 없다.
		if (bPartial)
		{
			Test.TestEqual(TEXT("partial_industry keeps the candidate grid visible"),
				Empty->GetVisibility(), ESlateVisibility::Collapsed);
			Test.TestTrue(TEXT("partial_industry still lists the owned industry's tiles"),
				Grid->GetChildrenCount() > 0);
		}
		// Review Focus 4 — 직원 0명 건물은 타일에 보이되 CTA 만 막힌다.
		if (bNoCrew)
		{
			Test.TestTrue(TEXT("no_crew still shows the tile"), Grid->GetChildrenCount() > 0);
			Test.TestFalse(TEXT("no_crew blocks the start CTA"), Start->GetIsEnabled());
		}

		// CollabRail 자신은 스크롤 콘텐츠라 뷰포트보다 크다 — 셸 수용은 좌열(ColL)로 건다.
		if (UWidget* LeftColumn = Required<UWidget>(Panel, TEXT("ColL"), Test)) Bounds.Add(LeftColumn);
		Bounds.Add(SlotRow);
		Bounds.Add(Rewards);
		Bounds.Add(Powers);
		// 접힌 well 안의 위젯은 기하가 0 이라 "화면 안" 판정이 거짓 실패한다 — 그 상태의 주인공만 담는다.
		if (bRunning) { Bounds.Add(Claim); Bounds.Add(Progress); } else Bounds.Add(Start);
		if (Grid->GetChildrenCount() > 0) Bounds.Add(Grid); else Bounds.Add(Empty);
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFusionProjectRenderTest, "CGR.SideContent.Fusion.Render",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFusionProjectRenderTest::RunTest(const FString& Parameters)
{
	using namespace FusionProjectRenderValidation;
	if (GUsingNullRHI) { AddError(TEXT("Fusion.Render requires real RHI; nullrhi cannot validate UI.")); return false; }
	UGameInstance* GI = FindGameInstance();
	UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	USideContentSubsystem* Side = GI ? GI->GetSubsystem<USideContentSubsystem>() : nullptr;
	if (!Tables || !Side) { AddError(TEXT("Fusion.Render requires an existing Game/PIE instance with imported Fusion tables.")); return false; }
	const TSubclassOf<UUserWidget> PanelClass = Tables->GetWidgetClass(EWidgetType::FusionProjectPanel);
	if (!TestNotNull(TEXT("FusionProjectPanel registered in DT_WidgetClass"), PanelClass.Get())) return false;
	FString BeforeState;
	if (!TestTrue(TEXT("Read-only side-content snapshot serialized"),
		FJsonObjectConverter::UStructToJsonObjectString(Side->GetSideContentData(), BeforeState))) return false;
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("FusionUI/render"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	TArray<TSharedPtr<FJsonValue>> Captures;
	TSharedPtr<FJsonObject> Warmup;
	for (int32 Width : { 2304, 2560, 3120 })
	{
		for (int32 State = 0; State < static_cast<int32>(UE_ARRAY_COUNT(States)); ++State)
		{
			TStrongObjectPtr<UFusionProjectPanelWidget> Panel(CreateWidget<UFusionProjectPanelWidget>(GI, PanelClass));
			if (!TestNotNull(TEXT("Fusion panel instance"), Panel.Get())) continue;
			// 자동 활성화를 막아 라이브 시스템 갱신/타이머와 테스트 픽스처를 분리한다. Save setter는 호출하지 않는다.
			FBoolProperty* AutoActivate = FindFProperty<FBoolProperty>(Panel->GetClass(), TEXT("bAutoActivate"));
			if (!TestNotNull(TEXT("CommonUI auto activation guard"), AutoActivate)) continue;
			AutoActivate->SetPropertyValue_InContainer(Panel.Get(), false);
			const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
			if (!TestFalse(TEXT("Render panel stays inactive"), Panel->IsActivated())) continue;
			Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Panel->SetRenderOpacity(1.f);
			UWidget* Warn = Required<UWidget>(*Panel, TEXT("ServerTimeWarn"), *this);
			UWidget* ShellWidget = Required<UWidget>(*Panel, TEXT("AppearSlideTarget"), *this);
			UWidget* ModalFrame = Required<UWidget>(*Panel, TEXT("ModalFrame"), *this);
			UWidget* Background = Required<UWidget>(*Panel, TEXT("BackgroundBtn"), *this);
			if (!Warn || !ShellWidget || !ModalFrame || !Background) continue;
			Warn->SetVisibility(State == 8 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			TArray<UWidget*> Bounds;
			const bool bFixture = FillBoard(*Panel, *GI, *Tables, State, *this, Bounds);
			if (!TestTrue(TEXT("Fixture populated through production widgets"), bFixture)) continue;
			// 첫 픽스처가 패널·부품의 브러시 머티리얼을 전부 로드한 뒤에 데운다 — 그 전에 돌리면
			// 아직 로드되지 않은 부품 머티리얼을 놓친다.
			if (!Warmup.IsValid()) { Warmup = WarmUserInterfaceMaterials(*this); }
			const FString Name = FString::Printf(TEXT("%s_%d"), States[State], Width);
			const bool bRendered = Draw(PanelSlate, Width, Directory / (Name + TEXT(".png")));
			TestTrue(Name + TEXT(" actual RHI PNG"), bRendered);
			const FLayoutBox Shell = Measure(*Panel, ShellWidget);
			const FLayoutBox Canvas { FVector2D::ZeroVector, FVector2D(Width, Height) };
			TestTrue(Name + TEXT(" positive shell within canvas"), Shell.Inside(Canvas));
			TestTrue(Name + TEXT(" fixed 2160x1280 shell"),
				FMath::Abs(Shell.Size.X - 2160.0) < 2.0 && FMath::Abs(Shell.Size.Y - 1280.0) < 2.0);
			TSharedPtr<FJsonObject> Capture = MakeShared<FJsonObject>();
			Capture->SetStringField(TEXT("name"), Name);
			Capture->SetBoolField(TEXT("rendered"), bRendered);
			Capture->SetObjectField(TEXT("shell"), BoxJson(Shell));

			// 공용 헤더의 실제 높이는 저작 아키타입에서 측정할 수 없다(GetDesiredSize 가 0) — 라이브
			// 기하만이 답을 준다. 28개 패널이 같은 부품을 쓰므로 manifest 에 남긴다.
			UUserWidget* FrameWidget = Cast<UUserWidget>(ModalFrame);
			UWidget* Header = FrameWidget ? FrameWidget->GetWidgetFromName(TEXT("PanelHeader")) : nullptr;
			Capture->SetObjectField(TEXT("frame"), BoxJson(Measure(*Panel, ModalFrame)));
			if (TestNotNull(Name + TEXT(" modal frame exposes PanelHeader"), Header))
			{
				const FLayoutBox HeaderBox = Measure(*Panel, Header);
				// 0 은 "작다" 가 아니라 측정 실패다 — 미페인트/Collapsed/진짜 0 이 한 값에 뭉쳐 있다.
				TestTrue(Name + TEXT(" header height is measured, not zero"), HeaderBox.Positive());
				Capture->SetObjectField(TEXT("header"), BoxJson(HeaderBox));
				// 헤더 높이는 HeaderContent(Overlay) = max(HeaderRow, 버튼들) 이다. "패딩 −24 = 이전 높이"
				// 유도는 HeaderRow 가 그 max 를 쥐고 있을 때만 성립하므로 형제 기여분까지 같이 남긴다.
				if (UUserWidget* HeaderWidget = Cast<UUserWidget>(Header))
				{
					TSharedPtr<FJsonObject> Parts = MakeShared<FJsonObject>();
					for (const TCHAR* Part : { TEXT("HeaderSize"), TEXT("HeaderContent"), TEXT("HeaderRow"),
						TEXT("TitleText"), TEXT("HelpButton"), TEXT("UIE_CloseButton") })
					{
						UWidget* Child = HeaderWidget->GetWidgetFromName(FName(Part));
						if (!Child) continue;
						TSharedPtr<FJsonObject> Entry = BoxJson(Measure(*Panel, Child));
						Entry->SetNumberField(TEXT("visibility"), static_cast<int32>(Child->GetVisibility()));
						Parts->SetObjectField(Part, Entry);
					}
					Capture->SetObjectField(TEXT("header_parts"), Parts);
				}
			}

			// 레일은 스크롤한다(좌열 953 < 7행 1023, 확정 2026-10-01). "다 보인다" 가 아니라 셋을 건다:
			// ① 7개가 다 만들어졌나 ② 뷰포트가 좌열 안에 있나 ③ 첫 행이 잘린 채로 시작하지 않나.
			// 7 은 제품의 카드 수가 아니라 DT 원본 행 수다(제품은 가용 업종으로 걸러 더 적을 수 있다) —
			// 스크롤이 필요해지는 최악 케이스를 재는 스트레스 픽스처라 일부러 거르지 않는다.
			UVerticalBox* Rail = Required<UVerticalBox>(*Panel, TEXT("CollabRail"), *this);
			UWidget* RailViewport = Required<UWidget>(*Panel, TEXT("RailScroll"), *this);
			UWidget* LeftColumn = Required<UWidget>(*Panel, TEXT("ColL"), *this);
			if (Rail && RailViewport && LeftColumn)
			{
				const FLayoutBox ViewportBox = Measure(*Panel, RailViewport);
				const FLayoutBox ColumnBox = Measure(*Panel, LeftColumn);
				Capture->SetObjectField(TEXT("rail_viewport"), BoxJson(ViewportBox));
				Capture->SetObjectField(TEXT("rail_content"), BoxJson(Measure(*Panel, Rail)));
				TestEqual(Name + TEXT(" rail builds all seven collaborations"), Rail->GetChildrenCount(), 7);
				TestTrue(Name + TEXT(" rail viewport sits inside the left column"), ViewportBox.Inside(ColumnBox));
				if (Rail->GetChildrenCount() == 7)
				{
					const FLayoutBox FirstRow = Measure(*Panel, Rail->GetChildAt(0));
					Capture->SetObjectField(TEXT("rail_first_row"), BoxJson(FirstRow));
					Capture->SetObjectField(TEXT("rail_last_row"), BoxJson(Measure(*Panel, Rail->GetChildAt(6))));
					// Positive() 를 먼저 본다 — 0 짜리 박스는 어떤 상자에도 "들어가" 버린다.
					TestTrue(Name + TEXT(" first rail row is fully visible"),
						FirstRow.Positive() && FirstRow.Position.Y >= ViewportBox.Position.Y - 1.0
						&& FirstRow.Position.Y + FirstRow.Size.Y <= ViewportBox.Position.Y + ViewportBox.Size.Y + 1.0);
				}
			}

			TArray<TSharedPtr<FJsonValue>> Measured;
			for (UWidget* Widget : Bounds)
			{
				const FLayoutBox Box = Measure(*Panel, Widget);
				const bool bInside = Box.Inside(Shell);
				TestTrue(Name + TEXT(" positive child inside shell: ") + GetNameSafe(Widget), bInside);
				TSharedPtr<FJsonObject> Entry = BoxJson(Box);
				Entry->SetStringField(TEXT("widget"), GetNameSafe(Widget));
				Entry->SetBoolField(TEXT("inside"), bInside);
				Measured.Add(MakeShared<FJsonValueObject>(Entry));
			}
			if (State == 8) TestTrue(Name + TEXT(" time warning remains inside shell"), Measure(*Panel, Warn).Inside(Shell));
			Capture->SetArrayField(TEXT("children"), Measured);
			Captures.Add(MakeShared<FJsonValueObject>(Capture));
			static_cast<UUserWidget*>(Panel.Get())->ReleaseSlateResources(true);
		}
	}
	FString AfterState;
	TestTrue(TEXT("Post-render side-content snapshot serialized"),
		FJsonObjectConverter::UStructToJsonObjectString(Side->GetSideContentData(), AfterState));
	const bool bUnchanged = TestEqual(TEXT("Render fixtures never change player side-content state"), AfterState, BeforeState);
	TestEqual(TEXT("All states and aspect ratios rendered"), Captures.Num(), int32(UE_ARRAY_COUNT(States) * 3));
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("fixture_only"), true);
	if (Warmup.IsValid()) { Report->SetObjectField(TEXT("material_warmup"), Warmup); }
	Report->SetBoolField(TEXT("side_content_state_unchanged"), bUnchanged);
	Report->SetArrayField(TEXT("captures"), Captures);
	FString ReportText;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&ReportText));
	TestTrue(TEXT("Capture manifest written"), FFileHelper::SaveStringToFile(ReportText, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return !HasAnyErrors();
}

#endif
