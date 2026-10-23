#if WITH_EDITOR && !UE_BUILD_SHIPPING

#include "UI/Panel/PitchBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Data/TierLayout.h"
#include "Enum/QualityGrade.h"
#include "Enum/WidgetType.h"
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

// 제조업 「제품 선택」 상태별 캡처 + 글자 넘침·버튼 배선 단언.
// 세이브 쓰기가 억제된 격리 -game 프로세스에서만 돈다(상태 주입은 메모리 세이브를 직접 바꾼다).
namespace PitchBoardManufacturingCapture
{
static bool IsIsolatedProcess()
{
	return FPaths::GetCleanFilename(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()).LeftChop(1))
		.StartsWith(TEXT("Saved_OfficeExteriorCapture"));
}

static bool Draw(UUserWidget& Panel, int32 Width, const FString& Filename)
{
	FWidgetRenderer Renderer(true);
	const FVector2D DrawSize(Width, 1440);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
	if (!Target.IsValid()) return false;
	const TSharedRef<SWidget> SlateWidget = Panel.TakeWidget();
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		Renderer.DrawWidget(Target.Get(), SlateWidget, 1.f, DrawSize, 0.f);
		FlushRenderingCommands();
	}
	FImage Pixels;
	if (!FImageUtils::GetRenderTargetImage(Target.Get(), Pixels)) return false;
	Pixels.GammaSpace = EGammaSpace::sRGB;
	TArray64<uint8> Bytes;
	return FImageUtils::CompressImage(Bytes, TEXT("png"), Pixels) && FFileHelper::SaveArrayToFile(Bytes, *Filename);
}

static bool IsShown(const UWidget* Item)
{
	for (const UWidget* Ancestor = Item; Ancestor; Ancestor = Ancestor->GetParent())
	{
		if (!Ancestor->IsVisible()) return false;
	}
	return Item != nullptr;
}

static bool IsUnder(const UWidget* Item, const UWidget* Root)
{
	for (const UWidget* Ancestor = Item; Ancestor; Ancestor = Ancestor->GetParent())
	{
		if (Ancestor == Root) return true;
	}
	return false;
}

// 줄바꿈 없는 글자가 받은 칸보다 넓으면 잘린다 ― 원하는 폭 > 배치 폭.
static TArray<TSharedPtr<FJsonValue>> FindOverflow(UUserWidget& Board, const TArray<const UWidget*>& Roots)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	// 타일·탭 안 글자는 부모 트리가 끊기므로 그 타일 자체의 위치로 범위를 본다.
	auto Check = [&Result, &Roots](UTextBlock* Text, const FString& Owner, const UWidget* Anchor)
	{
		if (!IsShown(Text) || Text->GetText().IsEmpty()) return;
		bool bInScope = Roots.IsEmpty();
		for (const UWidget* Root : Roots) bInScope |= IsUnder(Anchor, Root);
		if (!bInScope) return;
		const FVector2D Desired = Text->GetDesiredSize();
		const FVector2D Allocated = Text->GetCachedGeometry().GetLocalSize();
		// 스크롤 밖이라 그려지지 않은 글자는 배치 크기가 0 으로 남는다 ― 판정 대상이 아니다.
		if (Allocated.X <= 0.f) return;
		if (Desired.X > Allocated.X + 0.5f || Desired.Y > Allocated.Y + 0.5f)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("widget"), Owner + TEXT(".") + Text->GetName());
			Entry->SetStringField(TEXT("text"), Text->GetText().ToString());
			Entry->SetNumberField(TEXT("desired_w"), Desired.X);
			Entry->SetNumberField(TEXT("allocated_w"), Allocated.X);
			Entry->SetNumberField(TEXT("desired_h"), Desired.Y);
			Entry->SetNumberField(TEXT("allocated_h"), Allocated.Y);
			Result.Add(MakeShared<FJsonValueObject>(Entry));
		}
	};
	Board.WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Item)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Item)) Check(Text, Board.GetName(), Text);
		if (UUserWidget* Child = Cast<UUserWidget>(Item); Child && Child->WidgetTree && IsShown(Child))
		{
			Child->WidgetTree->ForEachWidget([&](UWidget* Inner)
			{
				if (UTextBlock* Text = Cast<UTextBlock>(Inner)) Check(Text, Child->GetClass()->GetName(), Child);
			});
		}
	});
	return Result;
}

static FString TextOf(UUserWidget& Board, const TCHAR* Name)
{
	const UTextBlock* Text = Cast<UTextBlock>(Board.GetWidgetFromName(Name));
	return Text ? Text->GetText().ToString() : FString();
}

struct FContext
{
	UWorld* World = nullptr;
	UCGGameInstance* GI = nullptr;
	UUIBase* UIBase = nullptr;
	UOfficeStageProgressManager* Stage = nullptr;
	UManufacturingDevelopmentManager* Development = nullptr;
	UProductionOrderManager* Orders = nullptr;
	FOfficeSaveData* Office = nullptr;
	TSubclassOf<UUserWidget> BoardClass;
	int32 BuildingID = INDEX_NONE;
};

static bool Resolve(UWorld* World, FContext& Out, FString& OutError)
{
	Out.World = World;
	Out.GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	UUIManagerSubsystem* UIMgr = Out.GI ? Out.GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UTableManagerSubsystem* Tables = Out.GI ? Out.GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	USaveLoadManager* Saves = Out.GI ? Out.GI->GetSubsystem<USaveLoadManager>() : nullptr;
	Out.UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	Out.Stage = World ? World->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	Out.Development = Out.GI ? Out.GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	Out.Orders = Out.GI ? Out.GI->GetSubsystem<UProductionOrderManager>() : nullptr;
	Out.BoardClass = Tables ? Tables->GetWidgetClass(EWidgetType::PitchBoard) : nullptr;
	Out.BuildingID = Out.GI ? Out.GI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	USaveGame_GameData* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
	Out.Office = Save ? Save->GameData.OfficeDataMap.Find(Out.BuildingID) : nullptr;
	if (!Out.UIBase || !Out.Stage || !Out.BoardClass || !Out.Office || !Out.Development || !Out.Orders)
	{
		OutError = TEXT("office context missing (UIBase/Stage/BoardClass/Office/managers)");
		return false;
	}
	return true;
}

static UPitchBoardWidget* FindBoard(UWorld* World)
{
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World && It->IsActivated()) return *It;
	}
	return nullptr;
}

static void ApplyTier(const FContext& Context, const FProjectTierProgress& Progress)
{
	Context.Office->TierProgress = Progress;
	Context.Stage->SetTierProgress(Progress);
}

static TSharedPtr<FJsonObject> Shoot(UPitchBoardWidget& Board, const FString& Directory, const FString& Case, bool bManufacturing)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("case"), Case);
	TArray<TSharedPtr<FJsonValue>> Widths;
	bool bPassed = true;
	for (const int32 Width : {2304, 3120})
	{
		const FString File = Directory / FString::Printf(TEXT("%s_%d.png"), *Case, Width);
		TSharedPtr<FJsonObject> Shot = MakeShared<FJsonObject>();
		Shot->SetNumberField(TEXT("width"), Width);
		Shot->SetBoolField(TEXT("saved"), Draw(Board, Width, File));
		Shot->SetStringField(TEXT("file"), File);
		TArray<const UWidget*> Roots;
		if (bManufacturing)
		{
			for (const TCHAR* Name : {TEXT("ManufacturingDetailRoot"), TEXT("MfgListHeader"), TEXT("TileGrid"), TEXT("TierRailBox")})
			{
				if (const UWidget* Root = Board.GetWidgetFromName(Name)) Roots.Add(Root);
			}
		}
		const TArray<TSharedPtr<FJsonValue>> Overflow = FindOverflow(Board, Roots);
		Shot->SetArrayField(TEXT("overflow"), Overflow);
		if (const UWidget* Body = Board.GetWidgetFromName(TEXT("MfgBody")); bManufacturing && Body)
		{
			const float Need = Body->GetDesiredSize().Y;
			const float Have = Body->GetCachedGeometry().GetLocalSize().Y;
			Shot->SetNumberField(TEXT("detail_need_h"), Need);
			Shot->SetNumberField(TEXT("detail_have_h"), Have);
			bPassed &= Need <= Have + 0.5f;
		}
		bPassed &= Overflow.IsEmpty() && Shot->GetBoolField(TEXT("saved"));
		Widths.Add(MakeShared<FJsonValueObject>(Shot));
	}
	Entry->SetArrayField(TEXT("widths"), Widths);
	Entry->SetBoolField(TEXT("passed"), bPassed);
	if (bManufacturing)
	{
		TSharedPtr<FJsonObject> Texts = MakeShared<FJsonObject>();
		for (const TCHAR* Name : {TEXT("MfgStatusSecuredText"), TEXT("ManufacturingQuantityText"), TEXT("MfgReserveLabel"),
			TEXT("MfgReserveSub"), TEXT("MfgShortageTitle"), TEXT("MfgFieldValue0"), TEXT("MfgFieldSub0"), TEXT("MfgFieldSub2"),
			TEXT("MfgPresetQuarterCount"), TEXT("MfgPresetMaxCount"), TEXT("MfgTimeValue"), TEXT("MfgUnlockCount")})
		{
			Texts->SetStringField(Name, TextOf(Board, Name));
		}
		Entry->SetObjectField(TEXT("texts"), Texts);
		Entry->SetBoolField(TEXT("factory_shown"), IsShown(Board.GetWidgetFromName(TEXT("MfgFactoryRoot"))));
		Entry->SetBoolField(TEXT("shortage_shown"), IsShown(Board.GetWidgetFromName(TEXT("MfgShortageNotice"))));
	}
	return Entry;
}

static TSharedPtr<FJsonObject> Click(UPitchBoardWidget& Board, const TCHAR* ButtonName, const TCHAR* Expect)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	UButtonWidget* Button = Cast<UButtonWidget>(Board.GetWidgetFromName(ButtonName));
	const FString Before = TextOf(Board, TEXT("ManufacturingQuantityText"));
	if (Button) Button->OnClicked().Broadcast();
	const FString After = TextOf(Board, TEXT("ManufacturingQuantityText"));
	Entry->SetStringField(TEXT("button"), ButtonName);
	Entry->SetBoolField(TEXT("found"), Button != nullptr);
	Entry->SetStringField(TEXT("before"), Before);
	Entry->SetStringField(TEXT("after"), After);
	Entry->SetStringField(TEXT("expect"), Expect);
	Entry->SetBoolField(TEXT("passed"), Button != nullptr && After == Expect);
	return Entry;
}

// 한 프레임에 닫고 다시 열면 스택이 처리 전의 옛 인스턴스를 돌려준다 ― 바꾸기·열기·찍기를 명령 단위로 나눈다.
struct FSession
{
	bool bValid = false;
	FProjectTierProgress Tier;
	FManufacturingResearchState Research;
	TArray<double> Points;
	TArray<FProductionOrder> Orders;
	TArray<double> Unit;
	int32 Product = INDEX_NONE;
	float RequiredSeconds = 0.f;
};
static FSession Session;

static FString OutputDirectory()
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("PitchBoardManufacturing") / TEXT("run");
	IFileManager::Get().MakeDirectory(*Directory, true);
	return Directory;
}

static void SaveJson(const TSharedPtr<FJsonObject>& Object, const FString& Name)
{
	FString Json;
	FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *(OutputDirectory() / (Name + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8);
}

static bool ApplyState(const FContext& Context, const FString& State)
{
	if (!Session.bValid) return false;
	const TArray<double>& Unit = Session.Unit;
	auto SetPoints = [&Context, &Unit](double A, double B, double C)
	{
		Context.Office->ManufacturingDevelopmentPoints = {Unit[0] * A, Unit[1] * B, Unit[2] * C};
	};
	auto WithProduct = [&Context](bool bDeveloped, bool bResearching)
	{
		FProjectTierProgress Tier = Session.Tier;
		if (bDeveloped) Tier.ClearedProjects.AddUnique(Session.Product);
		else Tier.ClearedProjects.Remove(Session.Product);
		ApplyTier(Context, Tier);
		Context.Office->ManufacturingResearch.ProjectIndex = bResearching ? Session.Product : INDEX_NONE;
		Context.Office->ManufacturingResearch.CompletedWorkSeconds = bResearching ? Session.RequiredSeconds * 0.43 : 0.0;
	};
	auto AddOrder = [&Context](int32 Quantity)
	{
		Context.Orders->CreateTestOrder(Context.GI->GetCurrentBuildingCompanyType(), Session.Product, TEXT("Capture"),
			Quantity, EQualityGrade::A, Context.BuildingID);
	};
	Context.Orders->SetActiveOrders(Session.Orders);
	if (State == TEXT("none")) { WithProduct(false, false); SetPoints(40, 40, 40); }
	else if (State == TEXT("progress")) { WithProduct(false, true); SetPoints(40, 40, 40); }
	else if (State == TEXT("dev")) { WithProduct(true, false); SetPoints(1000.37, 1180.5, 1000.37); }
	else if (State == TEXT("huge")) { WithProduct(true, false); SetPoints(1234567.2, 1300000.9, 1234567.2); }
	else if (State == TEXT("short")) { WithProduct(true, false); SetPoints(55.4, 61.2, 0.6); }
	else if (State == TEXT("secured")) { WithProduct(true, false); SetPoints(240.6, 300.1, 240.6); AddOrder(17660); }
	else if (State == TEXT("secured_huge")) { WithProduct(true, false); SetPoints(0.2, 0.2, 0.2); AddOrder(9982340); }
	else return false;
	return true;
}

static void Step(const TArray<FString>& Args, UWorld* World)
{
	const FString Action = Args.Num() > 0 ? Args[0] : FString();
	const FString Arg = Args.Num() > 1 ? Args[1] : FString();
	const bool bWorldOk = World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
	FContext Context;
	FString Error;
	if (!bWorldOk || !IsIsolatedProcess() || !Resolve(World, Context, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.MfgStep %s passed=0: isolated office required %s"), *Action, *Error);
		return;
	}
	bool bPassed = true;
	if (Action == TEXT("begin"))
	{
		Session = FSession();
		Session.Tier = Context.Office->TierProgress;
		Session.Research = Context.Office->ManufacturingResearch;
		Session.Points = Context.Office->ManufacturingDevelopmentPoints;
		Session.Orders = Context.Orders->GetAllOrders();
		int32 Start = 0, End = -1;
		FTierLayout::GetRange(Session.Tier.CurrentTier, Start, End);
		for (int32 Index = Start; Index <= End && Session.Product == INDEX_NONE; ++Index)
		{
			if (Context.Development->GetResearchStatus(Context.BuildingID, Index).bValid) Session.Product = Index;
		}
		Session.Unit = Context.Orders->GetDevelopmentPointCosts(Context.BuildingID, Session.Product);
		Session.RequiredSeconds = Session.Product > 0 ? Context.Development->GetResearchStatus(Context.BuildingID, Session.Product).RequiredWorkSeconds : 0.f;
		Session.bValid = Session.Product > 0 && Session.Unit.Num() == 3;
		bPassed = Session.bValid;
	}
	else if (Action == TEXT("state"))
	{
		bPassed = ApplyState(Context, Arg);
		if (UPitchBoardWidget* Board = FindBoard(World)) Board->DeactivateWidget();
	}
	else if (Action == TEXT("close"))
	{
		if (UPitchBoardWidget* Board = FindBoard(World)) Board->DeactivateWidget();
	}
	else if (Action == TEXT("open"))
	{
		UPitchBoardWidget* Board = Cast<UPitchBoardWidget>(Context.UIBase->PushPromptClass(Context.BoardClass.Get()));
		if (Board && Session.bValid && IsManufacturingType(Context.GI->GetCurrentBuildingCompanyType())) Board->OpenProject(Session.Product);
		bPassed = Board != nullptr;
	}
	else if (Action == TEXT("shoot"))
	{
		UPitchBoardWidget* Board = FindBoard(World);
		const bool bManufacturing = IsManufacturingType(Context.GI->GetCurrentBuildingCompanyType());
		TSharedPtr<FJsonObject> Entry = Board ? Shoot(*Board, OutputDirectory(), Arg, bManufacturing) : MakeShared<FJsonObject>();
		bPassed = Board && Entry->GetBoolField(TEXT("passed"));
		SaveJson(Entry, Arg);
	}
	else if (Action == TEXT("clicks"))
	{
		UPitchBoardWidget* Board = FindBoard(World);
		const int32 Max = Session.bValid ? Context.Orders->GetMaxReservableQuantity(Context.BuildingID, Session.Product) : 0;
		const auto Num = [](int32 Value) { return FText::AsNumber(Value).ToString(); };
		TArray<TSharedPtr<FJsonValue>> Clicks;
		if (Board && Max > 1)
		{
			Clicks.Add(MakeShared<FJsonValueObject>(Click(*Board, TEXT("MfgPresetHalfButton"), *Num(FMath::Max(1, Max / 2)))));
			Clicks.Add(MakeShared<FJsonValueObject>(Click(*Board, TEXT("ManufacturingMaxButton"), *Num(Max))));
			Clicks.Add(MakeShared<FJsonValueObject>(Click(*Board, TEXT("ManufacturingMinusButton"), *Num(Max - 1))));
			Clicks.Add(MakeShared<FJsonValueObject>(Click(*Board, TEXT("ManufacturingPlusButton"), *Num(Max))));
			Clicks.Add(MakeShared<FJsonValueObject>(Click(*Board, TEXT("MfgPresetQuarterButton"), *Num(FMath::Max(1, Max / 4)))));
		}
		bPassed = !Clicks.IsEmpty();
		for (const TSharedPtr<FJsonValue>& Entry : Clicks) bPassed &= Entry->AsObject()->GetBoolField(TEXT("passed"));
		TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetNumberField(TEXT("max"), Max);
		Report->SetArrayField(TEXT("clicks"), Clicks);
		Report->SetBoolField(TEXT("passed"), bPassed);
		SaveJson(Report, TEXT("clicks"));
	}
	else if (Action == TEXT("measure"))
	{
		// 시안 대조용 ― 기준 부품 안에서의 상자(WBP 단위). 기준: @board=세 칸 줄, @tile0=첫 타일, @tab0=첫 단계 탭.
		UPitchBoardWidget* Board = FindBoard(World);
		const int32 Width = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 2560;
		const FString MeasureName = Args.Num() > 2 ? Args[2] : FString(TEXT("measure"));
		bPassed = Board && Draw(*Board, Width, OutputDirectory() / (MeasureName + TEXT(".png")));
		TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
		auto FirstChild = [Board](const TCHAR* Name) -> UUserWidget*
		{
			const UPanelWidget* Panel = Board ? Cast<UPanelWidget>(Board->GetWidgetFromName(Name)) : nullptr;
			return Panel && Panel->GetChildrenCount() > 0 ? Cast<UUserWidget>(Panel->GetChildAt(0)) : nullptr;
		};
		const UWidget* Rail = Board ? Board->GetWidgetFromName(TEXT("RailSizer")) : nullptr;
		// 기준 부품 아래의 이름 붙은 부품을 전부 잰다 ― 비교 쪽(pairs.json)이 필요한 것만 고른다.
		struct FRef { const TCHAR* Key; const UWidget* Ref; UUserWidget* Owner; };
		UUserWidget* Tile = FirstChild(TEXT("TileGrid"));
		UUserWidget* Tab = FirstChild(TEXT("TierRailBox"));
		const TArray<FRef> Refs = {
			{TEXT("cols"), Rail ? Rail->GetParent() : nullptr, Board},
			{TEXT("detail"), Board ? Board->GetWidgetFromName(TEXT("ManufacturingDetailRoot")) : nullptr, Board},
			{TEXT("list"), Board ? Board->GetWidgetFromName(TEXT("ProjectColumnWidth")) : nullptr, Board},
			{TEXT("rail"), Rail, Board},
			{TEXT("tile"), Tile, Tile},
			{TEXT("tier"), Tab, Tab}};
		for (const FRef& Ref : Refs)
		{
			TSharedPtr<FJsonObject> Group = MakeShared<FJsonObject>();
			if (Ref.Ref && Ref.Owner)
			{
				const FGeometry& RefGeometry = Ref.Ref->GetCachedGeometry();
				auto Box = [&RefGeometry](const UWidget* Item)
				{
					TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
					const FGeometry& Geometry = Item->GetCachedGeometry();
					const FVector2D TopLeft = RefGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
					const FVector2D BottomRight = RefGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
					Result->SetNumberField(TEXT("x"), TopLeft.X);
					Result->SetNumberField(TEXT("y"), TopLeft.Y);
					Result->SetNumberField(TEXT("w"), BottomRight.X - TopLeft.X);
					Result->SetNumberField(TEXT("h"), BottomRight.Y - TopLeft.Y);
					return Result;
				};
				Group->SetObjectField(TEXT("__ref"), Box(Ref.Ref));
				Ref.Owner->WidgetTree->ForEachWidget([&](UWidget* Item)
				{
					if (Item != Ref.Ref && IsShown(Item) && (Ref.Owner == Ref.Ref || IsUnder(Item, Ref.Ref))) Group->SetObjectField(Item->GetName(), Box(Item));
				});
				if (Tile && Ref.Ref != Tile && IsUnder(Tile->GetParent(), Ref.Ref)) Group->SetObjectField(TEXT("Tile0"), Box(Tile));
				if (Tab && Ref.Ref != Tab && IsUnder(Tab->GetParent(), Ref.Ref)) Group->SetObjectField(TEXT("Tab0"), Box(Tab));
			}
			Report->SetObjectField(Ref.Key, Group);
		}
		SaveJson(Report, MeasureName);
	}
	else if (Action == TEXT("restore") && Session.bValid)
	{
		ApplyTier(Context, Session.Tier);
		Context.Office->ManufacturingResearch = Session.Research;
		Context.Office->ManufacturingDevelopmentPoints = Session.Points;
		Context.Orders->SetActiveOrders(Session.Orders);
		if (UPitchBoardWidget* Board = FindBoard(World)) Board->DeactivateWidget();
	}
	else
	{
		bPassed = false;
	}
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.MfgStep %s %s passed=%d"), *Action, *Arg, bPassed);
}

static FAutoConsoleCommandWithWorldAndArgs StepCommand(TEXT("PitchBoardUI.MfgStep"),
	TEXT("begin | state <none|progress|dev|huge|short|secured|secured_huge> | close | open | shoot <case> | clicks | restore. Isolated -game only; run one step per frame."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Step));

// 격리 복사본에 제조 건물이 없을 때만 쓴다 ― 지정 건물의 업종을 메모리에서 전자로 바꾼다.
static void Prepare(const TArray<FString>& Args, UWorld* World)
{
	UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	USaveLoadManager* Saves = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	USaveGame_GameData* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
	const int32 BuildingID = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
	FBuildingEntitySaveData* Entry = Save ? Save->GameData.Buildings.FindByPredicate(
		[BuildingID](const FBuildingEntitySaveData& Item) { return Item.BuildingIndex == BuildingID; }) : nullptr;
	if (!IsIsolatedProcess() || !Entry || !Save->GameData.OfficeDataMap.Contains(BuildingID))
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.MfgPrepare passed=0: isolated process and an existing office required"));
		return;
	}
	// 맵 전환이 건물 업종을 다시 맞추므로 오피스에 들어온 뒤에 부른다.
	Entry->BuildingData.CompanyType = ECompanyType::Electronics;
	GI->SetCurrentBuildingCompanyType(ECompanyType::Electronics);
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.MfgPrepare passed=1 building=%d"), BuildingID);
}

static FAutoConsoleCommandWithWorldAndArgs PrepareCommand(TEXT("PitchBoardUI.MfgPrepare"),
	TEXT("[BuildingIndex] Isolated -game only: treat this office as Electronics in memory."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Prepare));

static void ClickFactory(UWorld* World)
{
	FContext Context;
	FString Error;
	if (!World || !IsIsolatedProcess() || !Resolve(World, Context, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.MfgClickFactory passed=0: isolated office context required %s"), *Error);
		return;
	}
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World && It->IsActivated())
		{
			UButtonWidget* Button = Cast<UButtonWidget>(It->GetWidgetFromName(TEXT("MfgFactoryButton")));
			const bool bShown = IsShown(Button);
			if (Button && bShown) Button->OnClicked().Broadcast();
			UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.MfgClickFactory clicked=%d"), Button && bShown);
			return;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.MfgClickFactory passed=0: no active board"));
}

static FAutoConsoleCommandWithWorld ClickFactoryCommand(TEXT("PitchBoardUI.MfgClickFactory"),
	TEXT("Isolated -game only: press the factory move button on the open manufacturing board."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ClickFactory));
}

#endif
