#include "UI/Panel/OfficeRecruitmentModalWidget.h"
#include "Utils/OfficeRecruitHireFxMath.h"

#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Enum/NotificationType.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Office/OfficeRecruitmentFlowSubsystem.h"
#include "Office/WorkstationActorBase.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/HelpButtonWidget.h"
#include "UI/Element/Buttons/IconButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Panel/EmployeeGachaProbabilityWidget.h"
#include "UI/UIBase.h"
#include "CommonButtonBase.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Texture2D.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Enum/LootBoxRarity.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SoftObjectPtr.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/EntityManager.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Data/EntitySaveData.h"
#include "Components/PanelWidget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace OfficeRecruitModalDebug
{
#if !UE_BUILD_SHIPPING
	static TAutoConsoleVariable<int32> CVarFailNextPull(TEXT("cg.OfficeRecruit.FailNextPull"), 0,
		TEXT("1이면 다음 채용 요청을 결제 전에 실패 경로로 보낸다(연출 검증용, 사용 후 0)."));
	static TAutoConsoleVariable<float> CVarFxTimeScale(TEXT("cg.OfficeRecruit.FxTimeScale"), 1.f,
		TEXT("채용 모달 사원증 출력 연출의 시간 배율(캡처 검증용, 1=정상)."));

	static UOfficeRecruitmentModalWidget* FindActiveModal(UWorld* World)
	{
		for (TObjectIterator<UOfficeRecruitmentModalWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsActivated()) return *It;
		}
		return nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSelect(TEXT("cg.OfficeRecruit.Select"),
		TEXT("cg.OfficeRecruit.Select <직무0-5> <권종0-2> - 열린 채용 모달의 선택을 바꾼다(검증용)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UOfficeRecruitmentModalWidget* Modal = FindActiveModal(World))
				Modal->DebugSelect(Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : -1, Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : -1);
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdPress(TEXT("cg.OfficeRecruit.Press"),
		TEXT("cg.OfficeRecruit.Press <single|batch|prob|desk> - 열린 채용 모달의 채용 버튼(prob = 확률 정보, desk = 빈 자리 안내의 책상 배치)을 누른다(검증용)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UOfficeRecruitmentModalWidget* Modal = FindActiveModal(World);
			if (!Modal) return;
			if (Args.IsValidIndex(0) && Args[0].Equals(TEXT("prob"), ESearchCase::IgnoreCase)) Modal->DebugOpenProbability();
			else if (Args.IsValidIndex(0) && Args[0].Equals(TEXT("desk"), ESearchCase::IgnoreCase)) Modal->DebugPressSeatGuide();
			else Modal->DebugPress(Args.IsValidIndex(0) && Args[0].Equals(TEXT("batch"), ESearchCase::IgnoreCase));
		}));
	// PIE 시작이 막힌 자동 캡처(-game)용 진입 경로. 건물 관리 창의 사무실 입장 버튼과 같은 순서다.
	static FAutoConsoleCommandWithWorldAndArgs CmdEnterOffice(TEXT("cg.OfficeRecruit.EnterOffice"),
		TEXT("cg.OfficeRecruit.EnterOffice [건물번호] - 도시에서 그 건물(생략 시 첫 회사 건물) 사무실로 들어간다(검증용)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UCGGameInstance* GameInstance = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
			UEntityManager* Entities = World ? World->GetSubsystem<UEntityManager>() : nullptr;
			if (!GameInstance || !Entities || GameInstance->IsLevelTransitionActive()) return;
			const int32 Wanted = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
			for (ABuildingBaseActor* Building : Entities->GetBuildings())
			{
				if (!IsValid(Building) || Building->GetCompanyType() == ECompanyType::None) continue;
				if (Wanted > 0 && Building->GetBuildingIndex() != Wanted) continue;
				GameInstance->SetCurrentManagedBuilding(Building);
				GameInstance->SetCurrentBuildingCompanyType(Building->GetCompanyType());
				GameInstance->SetOfficeMode(EOfficeMode::Normal);
				UE_LOG(LogTemp, Display, TEXT("[OfficeRecruitmentModal] Debug entering office %d."), Building->GetBuildingIndex());
				GameInstance->TransitionToLevel(TEXT("OfficeMap"));
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentModal] Debug EnterOffice found no company building %d."), Wanted);
		}));

	static FAutoConsoleCommandWithWorld CmdOpen(TEXT("cg.OfficeRecruit.Open"),
		TEXT("cg.OfficeRecruit.Open - 사무실 메인 화면의 채용 버튼과 같은 경로로 채용 모달을 연다(검증용)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (TObjectIterator<UOfficeMainWidget> It; It; ++It)
			{
				if (It->GetWorld() != World || It->HasAnyFlags(RF_ClassDefaultObject)) continue;
				if (UFunction* Handler = It->FindFunction(TEXT("OnRecruitmentButtonClicked")))
				{
					It->ProcessEvent(Handler, nullptr);
					return;
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentModal] Debug Open found no office main widget."));
		}));

	static FAutoConsoleCommandWithWorld CmdCapacity(TEXT("cg.OfficeRecruit.Capacity"),
		TEXT("cg.OfficeRecruit.Capacity - 세이브의 건물별 수용 인원과 직원 수를 로그에 남긴다(검증 준비용)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
			USaveLoadManager* Save = Instance ? Instance->GetSubsystem<USaveLoadManager>() : nullptr;
			UEmployeeManager* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
			const USaveGame_GameData* Data = Save ? Save->GetCurrentSaveData() : nullptr;
			if (!Data || !Employees) return;
			for (const FBuildingEntitySaveData& Building : Data->GameData.Buildings)
			{
				UE_LOG(LogTemp, Display, TEXT("[OfficeRecruitmentModal] Capacity building=%d name=%s capacity=%d employees=%d"),
					Building.BuildingIndex, *Building.InteractableName.ToString(),
					Employees->GetBuildingEmployeeCapacity(Building.BuildingIndex, false),
					Employees->GetEmployeeCountInBuilding(Building.BuildingIndex));
			}
		}));

	static FBox2D RectIn(const FGeometry& Space, const UWidget& Widget)
	{
		const FGeometry& Geometry = Widget.GetCachedGeometry();
		return FBox2D(Space.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector)),
			Space.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	}

	static bool Encloses(const FBox2D& Outer, const FBox2D& Inner)
	{
		return Inner.Min.X >= Outer.Min.X - 1.f && Inner.Min.Y >= Outer.Min.Y - 1.f
			&& Inner.Max.X <= Outer.Max.X + 1.f && Inner.Max.Y <= Outer.Max.Y + 1.f;
	}

	static bool IsShown(const UWidget* Widget)
	{
		for (const UWidget* Item = Widget; Item; Item = Item->GetParent())
		{
			if (!Item->IsVisible()) return false;
		}
		return Widget != nullptr;
	}

	// 글자가 속한 칸 = 가장 가까운 크기 상자 또는 타일(…Layer).
	static const UWidget* ContainerOf(const UWidget& Widget)
	{
		for (const UPanelWidget* Parent = Widget.GetParent(); Parent; Parent = Parent->GetParent())
		{
			if (Parent->IsA<USizeBox>() || Parent->GetName().EndsWith(TEXT("Layer"))) return Parent;
		}
		return nullptr;
	}

	// 파이썬은 FGeometry 내부값을 못 읽어서(복사본이 0) 실제 렌더 지오메트리 판정은 여기서 한다.
	static void WriteLayoutReport(UOfficeRecruitmentModalWidget& Modal, const FString& Label)
	{
		static const TCHAR* Texts[] = {
			TEXT("JobPlanName"), TEXT("JobDevName"), TEXT("JobGraphicsName"), TEXT("JobSoundName"), TEXT("JobServerName"), TEXT("JobQAName"),
			TEXT("JobPlanCount"), TEXT("JobDevCount"), TEXT("JobGraphicsCount"), TEXT("JobSoundCount"), TEXT("JobServerCount"), TEXT("JobQACount"),
			TEXT("TicketNormalName"), TEXT("TicketAdvancedName"), TEXT("TicketPremiumName"),
			TEXT("TicketNormalCount"), TEXT("TicketAdvancedCount"), TEXT("TicketPremiumCount"),
			TEXT("OddsGrade0"), TEXT("OddsGrade1"), TEXT("OddsGrade2"), TEXT("OddsGrade3"), TEXT("OddsGrade4"),
			TEXT("OddsValue0"), TEXT("OddsValue1"), TEXT("OddsValue2"), TEXT("OddsValue3"), TEXT("OddsValue4"),
			TEXT("StageTitle"), TEXT("StageOwned"), TEXT("OddsCaption"), TEXT("OddsCaptionTier"),
			TEXT("FreeSeatsText"), TEXT("SeatLabel"), TEXT("JobHeading"), TEXT("FailureText"), TEXT("BlockedHintText"),
			TEXT("SeatGuideTitle"), TEXT("SeatGuideBody") };
		static const TCHAR* Blocks[] = {
			TEXT("TicketStageSize"), TEXT("OddsStrip"), TEXT("TicketNormalSize"), TEXT("TicketAdvancedSize"), TEXT("TicketPremiumSize"),
			TEXT("PlanLayer"), TEXT("DevLayer"), TEXT("GraphicsLayer"), TEXT("SoundLayer"), TEXT("ServerLayer"), TEXT("QALayer"),
			TEXT("PullButton"), TEXT("PullButtonMulti"), TEXT("ProbabilityButton"), TEXT("SeatGuideButton") };
		const FGeometry& Space = Modal.GetCachedGeometry();
		const UWidget* Body = Modal.GetWidgetFromName(TEXT("BodyAlignment"));
		const FBox2D BodyRect = Body ? RectIn(Space, *Body) : FBox2D(ForceInit);
		TArray<TSharedPtr<FJsonValue>> Fit, Overlap, Outside;
		TArray<TPair<FString, FBox2D>> Boxes;
		auto Note = [](TArray<TSharedPtr<FJsonValue>>& List, const FString& Text) { List.Add(MakeShared<FJsonValueString>(Text)); };
		auto Check = [&](const UWidget* Widget, const FString& Name, const UWidget* Container, bool bText)
		{
			if (!IsShown(Widget)) return;
			const FBox2D Rect = RectIn(Space, *Widget);
			if (Rect.GetSize().X <= 0.f || Rect.GetSize().Y <= 0.f)
			{
				Note(Outside, Name + TEXT(" has zero geometry"));
				return;
			}
			const float Allotted = Widget->GetCachedGeometry().GetLocalSize().X;
			if (bText && Widget->GetDesiredSize().X > Allotted + 1.f)
				Note(Fit, FString::Printf(TEXT("%s desired %.0f > allotted %.0f"), *Name, Widget->GetDesiredSize().X, Allotted));
			if (Container && !Encloses(RectIn(Space, *Container), Rect)) Note(Outside, Name + TEXT(" outside ") + Container->GetName());
			if (Body && !Encloses(BodyRect, Rect)) Note(Outside, Name + TEXT(" outside body"));
			if (bText) Boxes.Emplace(Name, Rect);
		};
		for (const TCHAR* Name : Texts)
		{
			if (const UWidget* Widget = Modal.GetWidgetFromName(Name)) Check(Widget, Name, ContainerOf(*Widget), true);
			else Note(Outside, FString(Name) + TEXT(" missing"));
		}
		for (const TCHAR* Name : Blocks)
		{
			if (const UWidget* Widget = Modal.GetWidgetFromName(Name)) Check(Widget, Name, nullptr, false);
			else Note(Outside, FString(Name) + TEXT(" missing"));
		}
		for (const TCHAR* ButtonName : { TEXT("PullButton"), TEXT("PullButtonMulti") })
		{
			const UUserWidget* Button = Cast<UUserWidget>(Modal.GetWidgetFromName(ButtonName));
			if (!Button || !IsShown(Button)) continue;
			const FBox2D ButtonRect = RectIn(Space, *Button);
			const UWidget* Chip = Button->GetWidgetFromName(TEXT("RecruitCostChip"));
			for (const TCHAR* Part : { TEXT("ButtonText"), TEXT("CountText"), TEXT("RecruitCostChip") })
			{
				const UWidget* Widget = Button->GetWidgetFromName(Part);
				if (!Widget || !IsShown(Widget)) continue;
				const FBox2D Rect = RectIn(Space, *Widget);
				const FString Name = FString(ButtonName) + TEXT(".") + Part;
				if (!Encloses(ButtonRect, Rect)) Note(Outside, Name + TEXT(" outside button"));
				if (Chip && Widget != Chip && FCString::Strcmp(Part, TEXT("CountText")) == 0 && !Encloses(RectIn(Space, *Chip), Rect))
					Note(Outside, Name + TEXT(" outside cost chip"));
			}
		}
		for (int32 A = 0; A < Boxes.Num(); ++A)
		{
			for (int32 B = A + 1; B < Boxes.Num(); ++B)
			{
				const FBox2D& First = Boxes[A].Value;
				const FBox2D& Second = Boxes[B].Value;
				if (First.Min.X < Second.Max.X - 1.f && Second.Min.X < First.Max.X - 1.f
					&& First.Min.Y < Second.Max.Y - 1.f && Second.Min.Y < First.Max.Y - 1.f)
					Note(Overlap, Boxes[A].Key + TEXT(" x ") + Boxes[B].Key);
			}
		}
		const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("label"), Label);
		Report->SetNumberField(TEXT("viewport_w"), Space.GetLocalSize().X);
		Report->SetNumberField(TEXT("viewport_h"), Space.GetLocalSize().Y);
		const UWidget* Frame = Modal.GetWidgetFromName(TEXT("ModalFrame"));
		Report->SetNumberField(TEXT("frame_opacity"), Frame ? Frame->GetRenderOpacity() : -1.f);
		Report->SetNumberField(TEXT("body_w"), BodyRect.GetSize().X);
		Report->SetNumberField(TEXT("body_h"), BodyRect.GetSize().Y);
		Report->SetArrayField(TEXT("fit"), Fit);
		Report->SetArrayField(TEXT("overlap"), Overlap);
		Report->SetArrayField(TEXT("outside"), Outside);
		FString Line;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
		FJsonSerializer::Serialize(Report, Writer);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("OfficeRecruitmentModal/Redesign20261012/layout.jsonl");
		FFileHelper::SaveStringToFile(Line + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		UE_LOG(LogTemp, Display, TEXT("RECRUIT_LAYOUT %s"), *Line);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdLayout(TEXT("cg.OfficeRecruit.Layout"),
		TEXT("cg.OfficeRecruit.Layout <이름표> - 열린 채용 모달의 글자 넘침·겹침·칸 밖 이탈을 layout.jsonl 에 적는다(검증용)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UOfficeRecruitmentModalWidget* Modal = FindActiveModal(World))
				WriteLayoutReport(*Modal, Args.IsValidIndex(0) ? Args[0] : FString(TEXT("unnamed")));
			else
				UE_LOG(LogTemp, Warning, TEXT("RECRUIT_LAYOUT no active modal"));
		}));

	// 격리 캡처 세이브(-saveddirsuffix=RecruitModalCap)에서만 동작 — 실제 세이브의 층수·채용권을 바꾸지 않게 막는다.
	static FAutoConsoleCommandWithWorldAndArgs CmdFixture(TEXT("cg.OfficeRecruit.Fixture"),
		TEXT("cg.OfficeRecruit.Fixture floors <층수> | tickets <권종0-2> <장수> - 격리 캡처 세이브 전용 상태 준비(검증용)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_RecruitModalCap")))
			{
				UE_LOG(LogTemp, Error, TEXT("[OfficeRecruitmentModal] Fixture refused outside the isolated capture save."));
				return;
			}
			UCGGameInstance* Instance = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
			if (!Instance || Args.Num() < 2) return;
			const int32 ManagedIndex = Instance->GetCurrentManagedBuildingIndex();
			if (Args[0] == TEXT("floors"))
			{
				USaveLoadManager* Save = Instance->GetSubsystem<USaveLoadManager>();
				USaveGame_GameData* Data = Save ? Save->GetCurrentSaveData() : nullptr;
				if (!Data) return;
				for (FBuildingEntitySaveData& Building : Data->GameData.Buildings)
				{
					if (Building.BuildingIndex == ManagedIndex)
						Building.BuildingData.Body_Module_Copies = FMath::Max(0, FCString::Atoi(*Args[1]));
				}
			}
			else if (Args[0] == TEXT("tickets") && Args.Num() >= 3)
			{
				UItemInventoryManager* Items = Instance->GetSubsystem<UItemInventoryManager>();
				if (!Items) return;
				const EGachaTier Tiers[] = { EGachaTier::Normal, EGachaTier::Advanced, EGachaTier::Premium };
				const EItemType Ticket = URecruitmentManagerSubsystem::GetTicketTypeForTier(Tiers[FMath::Clamp(FCString::Atoi(*Args[1]), 0, 2)]);
				const int32 Delta = FMath::Max(0, FCString::Atoi(*Args[2])) - Items->GetItemCount(Ticket);
				if (Delta > 0) Items->AddItem(Ticket, Delta);
				else if (Delta < 0) Items->SpendItem(Ticket, -Delta);
			}
			if (UOfficeRecruitmentModalWidget* Modal = FindActiveModal(World)) Modal->SetBuildingIndex(ManagedIndex);
			UE_LOG(LogTemp, Display, TEXT("[OfficeRecruitmentModal] Fixture %s applied to building %d."), *FString::Join(Args, TEXT(" ")), ManagedIndex);
		}));
#endif

	static float FxTimeScale()
	{
#if !UE_BUILD_SHIPPING
		return FMath::Max(0.01f, CVarFxTimeScale.GetValueOnGameThread());
#else
		return 1.f;
#endif
	}

	static bool ConsumeFailNextPull()
	{
#if !UE_BUILD_SHIPPING
		if (CVarFailNextPull.GetValueOnGameThread() == 0) return false;
		// 콘솔로 켠 값은 SetByCode 로 못 덮어 계속 실패하므로 같은 우선순위로 끈다.
		CVarFailNextPull->Set(0, ECVF_SetByConsole);
		return true;
#else
		return false;
#endif
	}
}

void UOfficeRecruitmentModalWidget::SetBuildingIndex(int32 InBuildingIndex)
{
	BuildingIndex = InBuildingIndex;
	if (IsActivated())
	{
		SyncRequiredJobSelection();
		Refresh();
		if (JobGroup) JobGroup->SelectButtonAtIndex(static_cast<int32>(SelectedJob), false);
	}
}

void UOfficeRecruitmentModalWidget::SetPreferredWorkstation(AWorkstationActorBase* InWorkstation)
{
	PreferredWorkstation = InWorkstation;
}

UWidget* UOfficeRecruitmentModalWidget::GetJobSelectionWidget(EProductionDiscipline Discipline) const
{
	UCommonButtonBase* Buttons[] = { JobPlan.Get(), JobDev.Get(), JobGraphics.Get(), JobSound.Get(), JobServer.Get(), JobQA.Get() };
	const int32 Index = static_cast<int32>(Discipline);
	if (Index >= 0 && Index < UE_ARRAY_COUNT(Buttons)) return Buttons[Index];
	return JobSelectionArea.Get();
}

UWidget* UOfficeRecruitmentModalWidget::GetPullButtonMultiWidget() const
{
	return PullButtonMulti && PullButtonMulti->GetIsEnabled() ? static_cast<UWidget*>(PullButtonMulti) : PullButton.Get();
}

UWidget* UOfficeRecruitmentModalWidget::GetPullButtonSingleWidget() const
{
	return PullButton;
}

void UOfficeRecruitmentModalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWidget* Dim = GetWidgetFromName(TEXT("Dim"))) Dim->SetVisibility(ESlateVisibility::Collapsed);
	if (ViewportExtent && WidgetTree)
	{
		UButton* BackgroundButton = Cast<UButton>(ViewportExtent->GetContent());
		if (!BackgroundButton)
		{
			BackgroundButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackgroundBtn"));
			// UE 5.4 has no public focus setter; initialize before the Slate button is built.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			BackgroundButton->IsFocusable = false;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			ViewportExtent->SetContent(BackgroundButton);
		}
		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle BackgroundStyle;
		BackgroundStyle.SetNormal(EmptyBrush).SetHovered(EmptyBrush).SetPressed(EmptyBrush).SetDisabled(EmptyBrush);
		BackgroundStyle.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		BackgroundButton->SetStyle(BackgroundStyle);
		BackgroundButton->SetClickMethod(EButtonClickMethod::DownAndUp);
		BackgroundButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		BackgroundButton->SetVisibility(ESlateVisibility::Visible);
		BackgroundButton->OnClicked.RemoveAll(this);
		BackgroundButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
		ViewportExtent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (UWidget* ViewportFit = GetWidgetFromName(TEXT("ViewportFit")))
		ViewportFit->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshViewportExtent();
	bIsBackHandler = true;
	UGameInstance* Instance = GetGameInstance();
	RecruitmentFlow = GetWorld() ? GetWorld()->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
	RecruitmentManager = Instance ? Instance->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	ItemInventory = Instance ? Instance->GetSubsystem<UItemInventoryManager>() : nullptr;
	EmployeeManager = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
	Resources = Instance ? Instance->GetSubsystem<UResourceItemManager>() : nullptr;

	if (ModalFrame)
	{
		ModalFrame->SetVisibility(ESlateVisibility::Visible);
		ModalFrame->SetConsumePointerInput(true);
		ModalFrame->SetTitle(NSLOCTEXT("OfficeRecruitment", "Title", "직원 채용"));
		ModalFrame->SetHelpKey(FName(TEXT("EmployeeGacha")));
		ModalFrame->SetAppearance(FName(TEXT("Neutral")), NAME_None, NAME_None);
		if (UCloseButtonWidget* Close = ModalFrame->GetCloseButton())
		{
			Close->OnCloseClicked.RemoveAll(this);
			Close->OnCloseClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
		}
	}
	for (UCommonButtonBase* Button : { JobPlan.Get(), JobDev.Get(), JobGraphics.Get(), JobSound.Get(), JobServer.Get(), JobQA.Get(),
		TicketNormal.Get(), TicketAdvanced.Get(), TicketPremium.Get() })
	{
		UIconButtonWidget* JobButton = Cast<UIconButtonWidget>(Button);
		if (!JobButton) continue;
		JobButton->SetButtonText(FText::GetEmpty());
		if (UImage* InternalIcon = Cast<UImage>(JobButton->GetWidgetFromName(TEXT("IconImage"))))
			InternalIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	JobGroup = NewObject<UCommonButtonGroupBase>(this);
	JobGroup->SetSelectionRequired(true);
	for (UCommonButtonBase* Button : { JobPlan.Get(), JobDev.Get(), JobGraphics.Get(), JobSound.Get(), JobServer.Get(), JobQA.Get() })
	{
		if (!Button) continue;
		Button->SetIsSelectable(true);
		Button->SetIsInteractableWhenSelected(true);
		JobGroup->AddWidget(Button);
	}
	JobGroup->OnButtonBaseClicked.AddDynamic(this, &ThisClass::HandleJobClicked);

	TicketGroup = NewObject<UCommonButtonGroupBase>(this);
	TicketGroup->SetSelectionRequired(true);
	for (UCommonButtonBase* Button : { TicketNormal.Get(), TicketAdvanced.Get(), TicketPremium.Get() })
	{
		if (!Button) continue;
		Button->SetIsSelectable(true);
		TicketGroup->AddWidget(Button);
	}
	TicketGroup->OnButtonBaseClicked.AddDynamic(this, &ThisClass::HandleTicketClicked);

	if (PullButton)
	{
		PullButton->OnClicked().RemoveAll(this);
		PullButton->OnClicked().AddUObject(this, &ThisClass::HandlePullClicked);
	}
	if (PullButtonMulti)
	{
		PullButtonMulti->OnClicked().RemoveAll(this);
		PullButtonMulti->OnClicked().AddUObject(this, &ThisClass::HandlePullMultiClicked);
	}
	if (ProbabilityButton)
	{
		ProbabilityButton->OnClicked().RemoveAll(this);
		ProbabilityButton->OnClicked().AddUObject(this, &ThisClass::HandleProbabilityClicked);
	}
	if (SeatGuideButton)
	{
		SeatGuideButton->OnClicked().RemoveAll(this);
		SeatGuideButton->OnClicked().AddUObject(this, &ThisClass::HandleSeatGuideClicked);
	}
	Refresh();
	JobGroup->SelectButtonAtIndex(static_cast<int32>(SelectedJob), false);
	TicketGroup->SelectButtonAtIndex(static_cast<int32>(SelectedTier), false);
	ResetHireFx(true);
}

void UOfficeRecruitmentModalWidget::NativeDestruct()
{
	// 풀에서 재사용되므로 책상 지정 없이 다시 열린 채용이 지난 책상을 물려받지 않게 한다(다른 창에 가려질 때는 Destruct 가 오지 않아 유지된다).
	PreferredWorkstation.Reset();
	StopDepartmentIntro();
	RestoreOfficeIdleZone();
	if (ViewportExtent)
		if (UButton* BackgroundButton = Cast<UButton>(ViewportExtent->GetContent())) BackgroundButton->OnClicked.RemoveAll(this);
	if (UOfficeRecruitmentFlowSubsystem* Flow = RecruitmentFlow.Get()) Flow->OnRequestFinished.RemoveAll(this);
	if (UGameInstance* Instance = GetGameInstance())
		if (UGoalBoardSubsystem* Goals = Instance->GetSubsystem<UGoalBoardSubsystem>()) Goals->OnTrackedGoalChanged.RemoveAll(this);
	if (UItemInventoryManager* Items = ItemInventory.Get()) Items->OnItemChanged.RemoveAll(this);
	if (UGameInstance* Instance = GetGameInstance())
		if (UUIManagerSubsystem* UI = Instance->GetSubsystem<UUIManagerSubsystem>()) UI->OnUIResourceChanged.RemoveAll(this);
	if (UEmployeeManager* Employees = EmployeeManager.Get()) Employees->OnEmployeeRosterChanged.RemoveAll(this);
	if (ModalFrame)
		if (UCloseButtonWidget* Close = ModalFrame->GetCloseButton()) Close->OnCloseClicked.RemoveAll(this);
	if (PullButton) PullButton->OnClicked().RemoveAll(this);
	if (PullButtonMulti) PullButtonMulti->OnClicked().RemoveAll(this);
	if (ProbabilityButton) ProbabilityButton->OnClicked().RemoveAll(this);
	if (SeatGuideButton) SeatGuideButton->OnClicked().RemoveAll(this);
	for (UCommonButtonGroupBase* Group : { JobGroup.Get(), TicketGroup.Get() })
	{
		if (!Group) continue;
		Group->OnButtonBaseClicked.RemoveAll(this);
		Group->SetSelectionRequired(false);
		Group->RemoveAll();
	}
	JobGroup = nullptr;
	TicketGroup = nullptr;
	ReleaseHeldEntrance();
	ResetHireFx(true);
	Super::NativeDestruct();
}

void UOfficeRecruitmentModalWidget::NativeOnActivated()
{
	ResetHireFx(true);
	Super::NativeOnActivated();
	HideOfficeIdleZone();
	RefreshViewportExtent();
	if (AMainMapPlayerController* Controller = Cast<AMainMapPlayerController>(GetOwningPlayer())) Controller->GoToUIMode();
	if (UOfficeRecruitmentFlowSubsystem* Flow = RecruitmentFlow.Get())
	{
		Flow->OnRequestFinished.RemoveAll(this);
		Flow->OnRequestFinished.AddUObject(this, &ThisClass::HandleRequestFinished);
	}
	if (UItemInventoryManager* Items = ItemInventory.Get())
	{
		Items->OnItemChanged.RemoveAll(this);
		Items->OnItemChanged.AddUObject(this, &ThisClass::HandleItemChanged);
	}
	if (UEmployeeManager* Employees = EmployeeManager.Get())
	{
		Employees->OnEmployeeRosterChanged.RemoveAll(this);
		Employees->OnEmployeeRosterChanged.AddUObject(this, &ThisClass::Refresh);
	}
	if (UGameInstance* Instance = GetGameInstance())
		if (UGoalBoardSubsystem* Goals = Instance->GetSubsystem<UGoalBoardSubsystem>())
		{
			Goals->OnTrackedGoalChanged.RemoveAll(this);
			Goals->OnTrackedGoalChanged.AddUObject(this, &ThisClass::HandleTrackedGoalChanged);
		}
	if (UGameInstance* Instance = GetGameInstance())
		if (UUIManagerSubsystem* UI = Instance->GetSubsystem<UUIManagerSubsystem>())
		{
			UI->OnUIResourceChanged.RemoveAll(this);
			UI->OnUIResourceChanged.AddUObject(this, &ThisClass::HandleResourceChanged);
		}
	bRequestPending = RecruitmentFlow.IsValid() && RecruitmentFlow->IsBusy();
	SyncRequiredJobSelection();
	// 지난번 실패·좌석 안내는 닫혀 있는 동안 상황이 바뀌었을 수 있어 다시 열 때 지운다.
	if (FailureText) FailureText->SetVisibility(ESlateVisibility::Collapsed);
	Refresh();
	if (JobGroup) JobGroup->SelectButtonAtIndex(static_cast<int32>(SelectedJob), false);
	if (TicketGroup) TicketGroup->SelectButtonAtIndex(static_cast<int32>(SelectedTier), false);
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
		MissionMgr->NotifyOfficeRecruitmentModalOpened(this);
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(DepartmentIntroTimer, this,
		&ThisClass::TryPlayDepartmentIntro, 0.5f, false);
}

void UOfficeRecruitmentModalWidget::TryPlayDepartmentIntro()
{
	if (!IsActivated() || bRequestPending || HireFxPhase != EHireFxPhase::Idle) return;
	if (UPanelIntroSubsystem* Intro = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>())
	{
		const FName Key = Intro->GetRecruitDepartmentIntroKey(BuildingIndex);
		if (!Key.IsNone()) DepartmentIntroOverlay = Intro->PlayIntro(this, Key, false);
	}
}

void UOfficeRecruitmentModalWidget::StopDepartmentIntro()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DepartmentIntroTimer);
	if (DepartmentIntroOverlay.IsValid()) DepartmentIntroOverlay->RemoveFromParent();
	DepartmentIntroOverlay.Reset();
}

void UOfficeRecruitmentModalWidget::NativeOnDeactivated()
{
	StopDepartmentIntro();
	RestoreOfficeIdleZone();
	ReleaseHeldEntrance();
	if (UOfficeRecruitmentFlowSubsystem* Flow = RecruitmentFlow.Get()) Flow->OnRequestFinished.RemoveAll(this);
	if (UGameInstance* Instance = GetGameInstance())
		if (UGoalBoardSubsystem* Goals = Instance->GetSubsystem<UGoalBoardSubsystem>()) Goals->OnTrackedGoalChanged.RemoveAll(this);
	if (UItemInventoryManager* Items = ItemInventory.Get()) Items->OnItemChanged.RemoveAll(this);
	if (UEmployeeManager* Employees = EmployeeManager.Get()) Employees->OnEmployeeRosterChanged.RemoveAll(this);
	if (!RecruitmentFlow.IsValid() || !RecruitmentFlow->IsBusy())
		if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
			MissionMgr->NotifyRecruitPanelClosed();
	if (UGameInstance* Instance = GetGameInstance())
		if (UUIManagerSubsystem* UI = Instance->GetSubsystem<UUIManagerSubsystem>()) UI->OnUIResourceChanged.RemoveAll(this);
	if (AMainMapPlayerController* Controller = Cast<AMainMapPlayerController>(GetOwningPlayer()))
		if ((!RecruitmentFlow.IsValid() || !RecruitmentFlow->IsBusy())
			&& Controller->GetCurrentInputMode() == EInputMode::UI) Controller->GoToNormalMode();
	Super::NativeOnDeactivated();
}

bool UOfficeRecruitmentModalWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	HandleCloseClicked();
	return true;
}

void UOfficeRecruitmentModalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshViewportExtent();
	TickHireFx(InDeltaTime);
	if (SeatGuidePulse > 0.f)
	{
		SeatGuidePulse = FMath::Max(0.f, SeatGuidePulse - InDeltaTime / 0.35f);
		if (SeatGuide) SeatGuide->SetRenderOpacity(1.f - 0.65f * SeatGuidePulse);
	}
	// 다른 창에 밀렸다 돌아오면 요청 끝 알림을 놓쳐 대기 상태가 남을 수 있다.
	if (OfficeRecruitHireFx::ShouldReleaseStaleRequest(bRequestPending, HireFxPhase == EHireFxPhase::Idle,
		RecruitmentFlow.IsValid() && RecruitmentFlow->IsBusy()))
	{
		bRequestPending = false;
		Refresh();
	}
}

void UOfficeRecruitmentModalWidget::RefreshViewportExtent()
{
	if (!ViewportExtent || !GetWorld()) return;
	const FVector2D PixelSize = UWidgetLayoutLibrary::GetViewportSize(this);
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (PixelSize.X <= 0.f || PixelSize.Y <= 0.f || Scale <= 0.f) return;
	const FVector2D LocalSize = PixelSize / Scale;
	if (LocalSize.Equals(LastViewportExtent, 1.f)) return;
	LastViewportExtent = LocalSize;
	ViewportExtent->SetWidthOverride(LocalSize.X);
	ViewportExtent->SetHeightOverride(LocalSize.Y);
}

void UOfficeRecruitmentModalWidget::HandleJobClicked(UCommonButtonBase* Button, int32 Index)
{
	(void)Button;
	if (Index < 0 || Index >= static_cast<int32>(EProductionDiscipline::Count)) return;
	const UCGGameInstance* GameInst = Cast<UCGGameInstance>(GetGameInstance());
	if (GameInst && Index >= GetDisciplineCountForCompany(GameInst->GetCurrentBuildingCompanyType())) return;
	const EProductionDiscipline Required = GetRequiredRecruitDiscipline();
	if (Required != EProductionDiscipline::Count && Index != static_cast<int32>(Required))
	{
		if (JobGroup) JobGroup->SelectButtonAtIndex(static_cast<int32>(SelectedJob), false);
		return;
	}
	SelectedJob = static_cast<EProductionDiscipline>(Index);
	Refresh();
	if (UGameInstance* Instance = GetGameInstance())
		if (UMissionManagerSubsystem* MissionMgr = Instance->GetSubsystem<UMissionManagerSubsystem>())
			MissionMgr->NotifyRecruitJobSelected(this, SelectedJob);
}

void UOfficeRecruitmentModalWidget::HandleTicketClicked(UCommonButtonBase* Button, int32 Index)
{
	(void)Button;
	if (Index < 0 || Index > static_cast<int32>(EGachaTier::Premium)) return;
	SelectedTier = static_cast<EGachaTier>(Index);
	Refresh();
}

void UOfficeRecruitmentModalWidget::HandleCloseClicked()
{
	if (HireFxPhase == EHireFxPhase::Idle && !bRequestPending && (!RecruitmentFlow.IsValid() || !RecruitmentFlow->IsBusy())) DeactivateWidget();
}
void UOfficeRecruitmentModalWidget::HandlePullClicked() { RequestPull(1, PullButton); }
void UOfficeRecruitmentModalWidget::HandlePullMultiClicked() { RequestPull(GetAvailableCount(), PullButtonMulti); }

void UOfficeRecruitmentModalWidget::HandleSeatGuideClicked()
{
	if (HireFxPhase != EHireFxPhase::Idle || bRequestPending) return;
	for (TObjectIterator<UOfficeMainWidget> It; It; ++It)
	{
		if (It->HasAnyFlags(RF_ClassDefaultObject) || It->GetWorld() != GetWorld() || !It->IsVisible()) continue;
		// 스택 전환은 닫기 먼저, 배치 패널 push 는 그 뒤(위에 남은 창이 닫히며 되살아나는 것을 막는다).
		UOfficeMainWidget* OfficeMain = *It;
		DeactivateWidget();
		OfficeMain->OpenWorkstationPanel();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentModal] Seat guide found no office main widget."));
}

void UOfficeRecruitmentModalWidget::HandleProbabilityClicked()
{
	UGameInstance* Instance = GetGameInstance();
	UTableManagerSubsystem* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> WidgetClass = Tables ? Tables->GetWidgetClass(EWidgetType::EmployeeGachaProbabilityPanel) : nullptr;
	if (!WidgetClass) return;
	UEmployeeGachaProbabilityWidget* Probability = CreateWidget<UEmployeeGachaProbabilityWidget>(GetOwningPlayer(), WidgetClass);
	if (!Probability) return;
	Probability->AddToViewport(1000);
	Probability->SetupTable(RecruitmentManager.IsValid() ? RecruitmentManager->GetHRPower(BuildingIndex) : 0);
}

void UOfficeRecruitmentModalWidget::HandleItemChanged(EItemType Type, int32 NewCount, int32 Delta)
{
	(void)Type;
	(void)NewCount;
	(void)Delta;
	Refresh();
}

void UOfficeRecruitmentModalWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	(void)NewValue;
	if (Type == EResourceType::Diamond) Refresh();
}

void UOfficeRecruitmentModalWidget::HandleTrackedGoalChanged(FName GoalID)
{
	(void)GoalID;
	if (!IsActivated()) return;
	SyncRequiredJobSelection();
	Refresh();
	if (JobGroup) JobGroup->SelectButtonAtIndex(static_cast<int32>(SelectedJob), false);
}

EProductionDiscipline UOfficeRecruitmentModalWidget::GetRequiredRecruitDiscipline() const
{
	UGameInstance* Instance = GetGameInstance();
	if (!Instance) return EProductionDiscipline::Count;
	if (const UMissionManagerSubsystem* MissionMgr = Instance->GetSubsystem<UMissionManagerSubsystem>())
	{
		const EProductionDiscipline Required = MissionMgr->GetRequiredRecruitDiscipline();
		if (Required != EProductionDiscipline::Count) return Required;
	}
	const UGoalBoardSubsystem* Goals = Instance->GetSubsystem<UGoalBoardSubsystem>();
	return Goals ? Goals->GetGuidedRecruitDiscipline(BuildingIndex) : EProductionDiscipline::Count;
}

void UOfficeRecruitmentModalWidget::SyncRequiredJobSelection()
{
	const EProductionDiscipline Required = GetRequiredRecruitDiscipline();
	if (Required != EProductionDiscipline::Count) SelectedJob = Required;
}

void UOfficeRecruitmentModalWidget::HandleRequestFinished(bool bSuccess, const FName& Reason)
{
	const FText FailedMessage = NSLOCTEXT("OfficeRecruitment", "Failed", "채용에 실패했습니다. 다시 시도해 주세요.");
	// 창이 떠 있는 동안 알림(토스트)은 창 제목을 가리므로 실패는 창 안 실패 문구로만 알린다.
	if (!bSuccess) UE_LOG(LogTemp, Warning, TEXT("[OfficeRecruitmentModal] Request failed: %s"), *Reason.ToString());
	if (HireFxPhase == EHireFxPhase::Idle)
	{
		// 연출 없이 끝난 요청은 이전 동작대로 즉시 정리한다.
		bRequestPending = false;
		if (bSuccess)
		{
			DeactivateWidget();
			ReleaseHeldEntrance();
			return;
		}
		if (FailureText)
		{
			FailureText->SetText(FailedMessage);
			FailureText->SetVisibility(ESlateVisibility::Visible);
		}
		Refresh();
		return;
	}
	ResolveHireFx(bSuccess, FailedMessage);
}

int32 UOfficeRecruitmentModalWidget::GetAvailableCount() const
{
	const UOfficeRecruitmentFlowSubsystem* Flow = RecruitmentFlow.Get();
	const URecruitmentManagerSubsystem* Recruitment = RecruitmentManager.Get();
	const UItemInventoryManager* Items = ItemInventory.Get();
	if (!Flow || !Recruitment || !Items || BuildingIndex == INDEX_NONE || bRequestPending || Flow->IsBusy()
		|| !JobCaptionText || JobCaptionText->GetText().IsEmpty()) return 0;
	const EItemType Ticket = URecruitmentManagerSubsystem::GetTicketTypeForTier(SelectedTier);
	return FMath::Min(5, FMath::Min(Items->GetItemCount(Ticket),
		FMath::Min(Flow->GetFreeSeatCount(), Recruitment->GetFreeCapacity(BuildingIndex))));
}

bool UOfficeRecruitmentModalWidget::CanSinglePullWithDiamond() const
{
	if (SelectedTier == EGachaTier::Normal || bRequestPending || BuildingIndex == INDEX_NONE
		|| !RecruitmentFlow.IsValid() || RecruitmentFlow->IsBusy() || !RecruitmentManager.IsValid()
		|| !ItemInventory.IsValid() || !Resources.IsValid() || !JobCaptionText || JobCaptionText->GetText().IsEmpty()) return false;
	const EItemType Ticket = URecruitmentManagerSubsystem::GetTicketTypeForTier(SelectedTier);
	if (ItemInventory->GetItemCount(Ticket) > 0) return false;
	if (RecruitmentFlow->GetFreeSeatCount() < 1 || RecruitmentManager->GetFreeCapacity(BuildingIndex) < 1) return false;
	return Resources->GetResourceAmount(EResourceType::Diamond) >= RecruitmentManager->GetDiamondCost(SelectedTier);
}

void UOfficeRecruitmentModalWidget::RequestPull(int32 Count, UWidget* SourceButton)
{
	UOfficeRecruitmentFlowSubsystem* Flow = RecruitmentFlow.Get();
	if (!Flow || bRequestPending || Flow->IsBusy() || HireFxPhase != EHireFxPhase::Idle) return;
	const EProductionDiscipline Required = GetRequiredRecruitDiscipline();
	if (Required != EProductionDiscipline::Count && Required != SelectedJob) return;
	const URecruitmentManagerSubsystem* Recruitment = RecruitmentManager.Get();
	const OfficeRecruitHireFx::ESeatBlock Block = OfficeRecruitHireFx::SeatBlock(Flow->GetFreeSeatCount(),
		Recruitment ? Recruitment->GetFreeCapacity(BuildingIndex) : 0);
	if (Block != OfficeRecruitHireFx::ESeatBlock::None)
	{
		// 이유와 [책상 배치] 는 버튼 위 안내가 이미 보여 준다 — 누름은 그 안내를 한 번 깜빡여 가리킨다.
		SeatGuidePulse = 1.f;
		Refresh();
		return;
	}
	if (Count < 1 || (Count > GetAvailableCount() && !(Count == 1 && CanSinglePullWithDiamond()))) return;
	bRequestPending = true;
	if (FailureText) FailureText->SetVisibility(ESlateVisibility::Collapsed);
	// 결제 콜백이 같은 프레임에 실패해도 버튼 피드백과 입력 잠금이 먼저 적용된다.
	BeginHireFx(SourceButton);
	Refresh();
	bHoldingEntrance = true;
	const bool bAccepted = !OfficeRecruitModalDebug::ConsumeFailNextPull()
		&& Flow->RequestRecruitment(SelectedTier, Count, SelectedJob, PreferredWorkstation.Get(), true);
	if (!bAccepted)
	{
		ResolveHireFx(false, NSLOCTEXT("OfficeRecruitment", "Unavailable", "지금은 채용할 수 없습니다."));
		ReleaseHeldEntrance();
		return;
	}
}

void UOfficeRecruitmentModalWidget::RefreshJobNames()
{
	UGameInstance* Instance = GetGameInstance();
	UTableManagerSubsystem* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const UCGGameInstance* CGInstance = Cast<UCGGameInstance>(Instance);
	const ECompanyType Industry = CGInstance ? CGInstance->GetCurrentBuildingCompanyType() : ECompanyType::None;
	const int32 DisciplineCount = GetDisciplineCountForCompany(Industry);
	if (static_cast<int32>(SelectedJob) >= DisciplineCount)
	{
		SelectedJob = EProductionDiscipline::Plan;
		if (JobGroup) { JobGroup->SelectButtonAtIndex(0, false); }
	}
	const EProductionDiscipline Required = GetRequiredRecruitDiscipline();
	UCommonButtonBase* Buttons[] = { JobPlan.Get(), JobDev.Get(), JobGraphics.Get(), JobSound.Get(), JobServer.Get(), JobQA.Get() };
	UTextBlock* Labels[] = { JobPlanName.Get(), JobDevName.Get(), JobGraphicsName.Get(), JobSoundName.Get(), JobServerName.Get(), JobQAName.Get() };
	for (int32 Index = 0; Index < static_cast<int32>(EProductionDiscipline::Count); ++Index)
	{
		const FText Name = Tables && Index < DisciplineCount ? Tables->GetDisciplineDisplayName(Industry, Index) : FText::GetEmpty();
		if (Labels[Index]) Labels[Index]->SetText(Name);
		// 연출 중에는 몸통 딤이 입력을 막으므로 비활성 면(빈 브러시)으로 바꾸지 않는다.
		if (Buttons[Index])
		{
			const bool bEnabled = !Name.IsEmpty() && !(bRequestPending && HireFxPhase == EHireFxPhase::Idle)
				&& (Required == EProductionDiscipline::Count || Index == static_cast<int32>(Required));
			Buttons[Index]->SetIsEnabled(bEnabled);
			// 타일 면은 버튼 밖(같은 층의 형제)에 구워져 있어 비활성은 층 전체를 흐리게 보여 준다.
			if (UWidget* Layer = Buttons[Index]->GetParent())
			{
				Layer->SetVisibility(Index < DisciplineCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
				Layer->SetRenderOpacity(bEnabled ? 1.f : 0.5f);
			}
		}
		if (Index == static_cast<int32>(SelectedJob) && JobCaptionText) JobCaptionText->SetText(Name);
	}
}

void UOfficeRecruitmentModalWidget::Refresh()
{
	RefreshJobNames();
	// 표시·피드백 모두 실제로 뽑을 수 있는 수(빈 책상과 남은 정원 중 작은 값)를 쓴다.
	const int32 FreeSeats = FMath::Min(RecruitmentFlow.IsValid() ? RecruitmentFlow->GetFreeSeatCount() : 0,
		RecruitmentManager.IsValid() ? RecruitmentManager->GetFreeCapacity(BuildingIndex) : 0);
	// 제목 줄 오른쪽 "빈 자리 N" 의 숫자. 0 이면 빨갛게 — 이유 문장은 버튼 위 안내 줄이 맡는다.
	if (FreeSeatsText)
	{
		FreeSeatsText->SetText(FText::AsNumber(FreeSeats));
		FreeSeatsText->SetColorAndOpacity(FSlateColor(FreeSeats > 0 ? SeatCountColor : SeatEmptyColor));
	}
	// 빈 자리 0 이면 버튼 위 빈 공간에 이유와 할 일을 크게 — 책상이 없을 때만 [책상 배치] 바로가기.
	const OfficeRecruitHireFx::ESeatBlock SeatBlock = BuildingIndex == INDEX_NONE ? OfficeRecruitHireFx::ESeatBlock::None
		: OfficeRecruitHireFx::SeatBlock(RecruitmentFlow.IsValid() ? RecruitmentFlow->GetFreeSeatCount() : 0,
			RecruitmentManager.IsValid() ? RecruitmentManager->GetFreeCapacity(BuildingIndex) : 0);
	const bool bShowSeatGuide = OfficeRecruitHireFx::ShouldShowSeatGuide(SeatBlock, bRequestPending, HireFxPhase == EHireFxPhase::Idle);
	if (SeatGuide) SeatGuide->SetVisibility(bShowSeatGuide ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowSeatGuide)
	{
		const bool bCapacity = SeatBlock == OfficeRecruitHireFx::ESeatBlock::Capacity;
		if (SeatGuideTitle) SeatGuideTitle->SetText(bCapacity
			? NSLOCTEXT("OfficeRecruitment", "GuideCapacityTitle", "정원이 가득 찼습니다")
			: NSLOCTEXT("OfficeRecruitment", "GuideDesksTitle", "빈 자리가 없습니다"));
		if (SeatGuideBody) SeatGuideBody->SetText(bCapacity
			? NSLOCTEXT("OfficeRecruitment", "GuideCapacityBody", "건물 층을 올리면 정원이 늘어납니다")
			: NSLOCTEXT("OfficeRecruitment", "GuideDesksBody", "책상을 배치하면 바로 채용할 수 있습니다"));
		if (SeatGuideButton) SeatGuideButton->SetVisibility(bCapacity ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		// 같은 자리의 실패 문구는 안내가 덮으므로 함께 띄우지 않는다.
		if (FailureText) FailureText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UTextBlock* JobCounts[] = { JobPlanCount.Get(), JobDevCount.Get(), JobGraphicsCount.Get(), JobSoundCount.Get(), JobServerCount.Get(), JobQACount.Get() };
	int32 Counts[static_cast<int32>(EProductionDiscipline::Count)] = {};
	if (EmployeeManager.IsValid() && BuildingIndex != INDEX_NONE)
		for (const FEmployeeInstance& Employee : EmployeeManager->GetEmployeesInBuilding(BuildingIndex))
		{
			const int32 JobIndex = GetPrimaryDisciplineSlot(Employee);
			if (JobIndex >= 0 && JobIndex < static_cast<int32>(EProductionDiscipline::Count)) ++Counts[JobIndex];
		}
	UWidget* JobSelections[] = { JobPlanSelected.Get(), JobDevSelected.Get(), JobGraphicsSelected.Get(), JobSoundSelected.Get(), JobServerSelected.Get(), JobQASelected.Get() };
	for (int32 Index = 0; Index < static_cast<int32>(EProductionDiscipline::Count); ++Index)
	{
		if (JobCounts[Index])
			JobCounts[Index]->SetText(FText::Format(NSLOCTEXT("OfficeRecruitment", "Assigned", "현재 {0}명"), FText::AsNumber(Counts[Index])));
		if (JobSelections[Index])
			JobSelections[Index]->SetVisibility(Index == static_cast<int32>(SelectedJob) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const EGachaTier Tiers[] = { EGachaTier::Normal, EGachaTier::Advanced, EGachaTier::Premium };
	UTextBlock* TicketCounts[] = { TicketNormalCount.Get(), TicketAdvancedCount.Get(), TicketPremiumCount.Get() };
	UTextBlock* TicketNames[] = { TicketNormalName.Get(), TicketAdvancedName.Get(), TicketPremiumName.Get() };
	const FString CommonTicketLabel = TicketHeading ? TicketHeading->GetText().ToString() : FString();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const EItemType Type = URecruitmentManagerSubsystem::GetTicketTypeForTier(Tiers[Index]);
		UCommonButtonBase* TicketButtons[] = { TicketNormal.Get(), TicketAdvanced.Get(), TicketPremium.Get() };
		if (TicketButtons[Index]) TicketButtons[Index]->SetIsEnabled(!(bRequestPending && HireFxPhase == EHireFxPhase::Idle));
		if (TicketNames[Index])
		{
			FString Label = GetItemTypeDisplayName(Type).ToString();
			// 공통 종류명은 섹션 제목에 있으므로 같은 접미사만 생략한다.
			if (!CommonTicketLabel.IsEmpty()) Label.RemoveFromEnd(CommonTicketLabel);
			Label.TrimStartAndEndInline();
			TicketNames[Index]->SetText(FText::FromString(Label));
			TicketNames[Index]->SetColorAndOpacity(FSlateColor(Tiers[Index] == SelectedTier
				? SelectedTicketLabelColor : TicketLabelColor));
		}
		if (TicketCounts[Index])
		{
			const int32 Owned = ItemInventory.IsValid() ? ItemInventory->GetItemCount(Type) : 0;
			TicketCounts[Index]->SetText(FText::Format(NSLOCTEXT("OfficeRecruitment", "TicketCount", "{0}장"), FText::AsNumber(Owned)));
			TicketCounts[Index]->SetColorAndOpacity(FSlateColor(Owned == 0 ? TicketEmptyCountColor
				: Tiers[Index] == SelectedTier ? SelectedTicketLabelColor : TicketLabelColor));
		}
	}
	const int32 Available = GetAvailableCount();
	const bool bDiamondSingle = CanSinglePullWithDiamond();
	const bool bNoSeatFeedback = FreeSeats < 1 && !bRequestPending && BuildingIndex != INDEX_NONE
		&& RecruitmentFlow.IsValid() && !RecruitmentFlow->IsBusy() && RecruitmentManager.IsValid()
		&& ItemInventory.IsValid() && JobCaptionText && !JobCaptionText->GetText().IsEmpty();
	UGameInstance* Instance = GetGameInstance();
	UTableManagerSubsystem* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const EItemType SelectedTicket = URecruitmentManagerSubsystem::GetTicketTypeForTier(SelectedTier);
	const int32 OwnedTickets = ItemInventory.IsValid() ? ItemInventory->GetItemCount(SelectedTicket) : 0;
	const int32 DiamondCost = RecruitmentManager.IsValid() ? RecruitmentManager->GetDiamondCost(SelectedTier) : 0;
	if (BlockedHintText)
	{
		const OfficeRecruitHireFx::EHireBlock Block = OfficeRecruitHireFx::SingleHireBlock(OwnedTickets, FreeSeats,
			SelectedTier != EGachaTier::Normal, Resources.IsValid() ? Resources->GetResourceAmount(EResourceType::Diamond) : 0, DiamondCost);
		FText Hint;
		if (Block == OfficeRecruitHireFx::EHireBlock::NoTickets)
			Hint = FText::Format(NSLOCTEXT("OfficeRecruitment", "NoTicketsHint", "{0}이 없습니다"), GetItemTypeDisplayName(SelectedTicket));
		else if (Block == OfficeRecruitHireFx::EHireBlock::NoDiamonds)
			Hint = FText::Format(NSLOCTEXT("OfficeRecruitment", "NoDiamondsHint", "다이아가 부족합니다 ({0}개 필요)"), FText::AsNumber(DiamondCost));
		// 실패 문구와 같은 자리라 실패 문구가 떠 있거나 요청 중이면 숨긴다.
		const bool bFailureShown = FailureText && FailureText->GetVisibility() == ESlateVisibility::Visible;
		BlockedHintText->SetText(Hint);
		BlockedHintText->SetVisibility(!Hint.IsEmpty() && !bFailureShown && !bRequestPending && !bShowSeatGuide
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const bool bDiamondCost = SelectedTier != EGachaTier::Normal && ItemInventory.IsValid()
		&& ItemInventory->GetItemCount(SelectedTicket) == 0;
	bool bTicketIconFound = false;
	UObject* TicketIcon = Tables ? Tables->GetItemIconResource(SelectedTicket, bTicketIconFound) : nullptr;
	if (PullButton)
	{
		if (HireFxPhase == EHireFxPhase::Idle) PullButton->SetIsEnabled(Available >= 1 || bDiamondSingle || bNoSeatFeedback);
		PullButton->SetButtonText(NSLOCTEXT("OfficeRecruitment", "Single", "1명 채용"));
		if (bDiamondCost && RecruitmentManager.IsValid())
		{
			const int32 Cost = RecruitmentManager->GetDiamondCost(SelectedTier);
			PullButton->SetCountText(FText::Format(NSLOCTEXT("OfficeRecruitment", "DiamondCost", "{0}개"), FText::AsNumber(Cost)));
			const TSoftObjectPtr<UTexture2D> DiamondIcon(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/DiamondIcon.DiamondIcon")));
			PullButton->SetIconResource(DiamondIcon.LoadSynchronous());
		}
		else
		{
			PullButton->SetCountText(NSLOCTEXT("OfficeRecruitment", "OneTicket", "1장"));
			PullButton->SetIconResource(bTicketIconFound ? TicketIcon : nullptr);
		}
	}
	if (PullButtonMulti)
	{
		if (HireFxPhase == EHireFxPhase::Idle) PullButtonMulti->SetIsEnabled(Available >= 2 || bNoSeatFeedback);
		PullButtonMulti->SetButtonText(NSLOCTEXT("OfficeRecruitment", "Multi", "일괄 채용"));
		// 요청 중에는 가용 수가 0으로 읽혀 연출 동안 "0장"이 보이므로 마지막 비용을 유지한다.
		if (!bRequestPending)
			PullButtonMulti->SetCountText(bNoSeatFeedback
				? NSLOCTEXT("OfficeRecruitment", "MaxMultiTickets", "최대 5장")
				: Available >= 2 ? FText::Format(NSLOCTEXT("OfficeRecruitment", "MultiTickets", "{0}장"), FText::AsNumber(Available))
				// "0장"은 비용 0으로 읽히므로 일괄의 조건을 적는다.
				: OfficeRecruitHireFx::BatchNeedsTickets(OwnedTickets) ? NSLOCTEXT("OfficeRecruitment", "MultiNeedsTickets", "2장 필요")
				: NSLOCTEXT("OfficeRecruitment", "MultiNeedsSeats", "2명부터"));
		PullButtonMulti->SetIconResource(bTicketIconFound ? TicketIcon : nullptr);
	}
	for (UIconWithButtonWidget* ActionButton : { PullButton.Get(), PullButtonMulti.Get() })
	{
		if (!ActionButton) continue;
		const FSlateColor ActionTextColor(ActionButton->GetIsEnabled() ? PullTextColor : PullDisabledTextColor);
		ActionButton->SetTextColor(ActionTextColor);
		if (UTextBlock* CostText = Cast<UTextBlock>(ActionButton->GetWidgetFromName(TEXT("CountText"))))
			CostText->SetColorAndOpacity(ActionTextColor);
	}
	RefreshStageAndOdds();
}

void UOfficeRecruitmentModalWidget::RefreshStageAndOdds()
{
	UGameInstance* Instance = GetGameInstance();
	UTableManagerSubsystem* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const EGachaTier Tiers[] = { EGachaTier::Normal, EGachaTier::Advanced, EGachaTier::Premium };
	UImage* TabIcons[] = { TicketNormalIcon.Get(), TicketAdvancedIcon.Get(), TicketPremiumIcon.Get() };
	UWidget* Rings[] = { TicketNormalSelectRing.Get(), TicketAdvancedSelectRing.Get(), TicketPremiumSelectRing.Get() };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Tiers)); ++Index)
	{
		bool bFound = false;
		UObject* Icon = Tables ? Tables->GetItemIconResource(URecruitmentManagerSubsystem::GetTicketTypeForTier(Tiers[Index]), bFound) : nullptr;
		if (TabIcons[Index] && bFound) CGIconBrush::ApplyResource(TabIcons[Index], Icon);
		// RoundedBox 외곽선은 렌더 불투명도를 무시하므로(2026-10-12 캡처) 켜고 끄기는 Visibility 로 한다.
		if (Rings[Index]) Rings[Index]->SetVisibility(Tiers[Index] == SelectedTier ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const EItemType Ticket = URecruitmentManagerSubsystem::GetTicketTypeForTier(SelectedTier);
	const FText TicketName = GetItemTypeDisplayName(Ticket);
	bool bHeroFound = false;
	UObject* HeroIcon = Tables ? Tables->GetItemIconResource(Ticket, bHeroFound) : nullptr;
	if (TicketHero && bHeroFound) CGIconBrush::ApplyResource(TicketHero, HeroIcon);
	if (StageGlow)
	{
		const FLinearColor Body = SelectedTier == EGachaTier::Premium ? StageGlowPremiumColor
			: SelectedTier == EGachaTier::Advanced ? StageGlowAdvancedColor : StageGlowNormalColor;
		StageGlow->SetColorAndOpacity(FLinearColor(Body.R, Body.G, Body.B, 1.f));
	}
	if (StageTitle) StageTitle->SetText(TicketName);
	if (StageOwned)
	{
		StageOwned->SetText(FText::Format(NSLOCTEXT("OfficeRecruitment", "StageOwned", "보유 {0}장"),
			FText::AsNumber(ItemInventory.IsValid() ? ItemInventory->GetItemCount(Ticket) : 0)));
	}
	if (OddsCaptionTier) OddsCaptionTier->SetText(TicketName);

	TArray<FGachaRarityChance> Rows;
	if (RecruitmentManager.IsValid())
		Rows = RecruitmentManager->GetProbabilityTableForUI(SelectedTier, RecruitmentManager->GetHRPower(BuildingIndex));
	UTextBlock* Grades[] = { OddsGrade0.Get(), OddsGrade1.Get(), OddsGrade2.Get(), OddsGrade3.Get(), OddsGrade4.Get() };
	UTextBlock* Values[] = { OddsValue0.Get(), OddsValue1.Get(), OddsValue2.Get(), OddsValue3.Get(), OddsValue4.Get() };
	FNumberFormattingOptions PercentFormat;
	PercentFormat.MinimumFractionalDigits = 0;
	PercentFormat.MaximumFractionalDigits = 1;
	// 일반~레전드 5칸. 신화는 가챠에서 나오지 않아(큐브 전용) 칸이 없다.
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Grades)); ++Index)
	{
		const ELootBoxRarity Rarity = static_cast<ELootBoxRarity>(Index);
		const FGachaRarityChance* Row = Rows.FindByPredicate([Rarity](const FGachaRarityChance& Chance) { return Chance.Rarity == Rarity; });
		const float Percent = Row ? Row->Percent : 0.f;
		const bool bAvailable = Percent > KINDA_SMALL_NUMBER;
		if (Grades[Index])
		{
			// 전역 표는 sRGB 16진값을 선형 자리에 담아 옅게 나온다 — 설계의 16진 등급색 그대로 변환한다.
			FLinearColor Ink = FLinearColor::FromSRGBColor(FLootBoxRarityUtility::GetRarityColor(Rarity).ToFColor(false));
			Ink.A = bAvailable ? 1.f : 0.45f;
			Grades[Index]->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Rarity)));
			Grades[Index]->SetColorAndOpacity(FSlateColor(Ink));
		}
		if (Values[Index])
		{
			Values[Index]->SetText(bAvailable
				? FText::Format(NSLOCTEXT("OfficeRecruitment", "OddsPercent", "{0}%"), FText::AsNumber(Percent, &PercentFormat))
				: FText::FromString(TEXT("\u2015")));
			Values[Index]->SetColorAndOpacity(FSlateColor(bAvailable ? OddsValueColor : OddsEmptyColor));
		}
	}
}

void UOfficeRecruitmentModalWidget::ResetHireFx(bool bResetFrame)
{
	HireFxPhase = EHireFxPhase::Idle;
	HireFxFailureMessage = FText::GetEmpty();
	HireFxClock = 0.f;
	if (BodyDim)
	{
		BodyDim->SetRenderOpacity(0.f);
		BodyDim->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (ModalFrame)
	{
		if (UWidget* Close = ModalFrame->GetCloseButton()) Close->SetRenderOpacity(1.f);
		if (UWidget* Help = ModalFrame->GetHelpButton()) Help->SetRenderOpacity(1.f);
	}
	if (UWidget* Button = HireFxButton.Get()) Button->SetRenderTransform(FWidgetTransform());
	HireFxButton.Reset();
	if (bResetFrame && ModalFrame)
	{
		ModalFrame->SetRenderTransform(FWidgetTransform());
		ModalFrame->SetRenderOpacity(1.f);
	}
}

void UOfficeRecruitmentModalWidget::BeginHireFx(UWidget* SourceButton)
{
	ResetHireFx(false);
	HireFxPhase = EHireFxPhase::Waiting;
	HireFxButton = SourceButton;
	bHireFxCalm = USettingsManagerSubsystem::IsReduceMotion(this);
	if (BodyDim) BodyDim->SetVisibility(ESlateVisibility::Visible);
	if (ModalFrame)
	{
		if (UWidget* Close = ModalFrame->GetCloseButton()) Close->SetRenderOpacity(0.45f);
		if (UWidget* Help = ModalFrame->GetHelpButton()) Help->SetRenderOpacity(0.45f);
	}
}

void UOfficeRecruitmentModalWidget::ResolveHireFx(bool bSuccess, const FText& FailureMessage)
{
	if (HireFxPhase != EHireFxPhase::Waiting) return;
	HireFxPhase = bSuccess ? EHireFxPhase::Succeeding : EHireFxPhase::Failing;
	HireFxFailureMessage = FailureMessage;
	HireFxClock = 0.f;
	if (bSuccess)
	{
		// RoundedBox 선택 테두리는 부모 페이드를 무시하므로 퇴장 시작에 숨긴다.
		for (UWidget* Ring : { TicketNormalSelectRing.Get(), TicketAdvancedSelectRing.Get(), TicketPremiumSelectRing.Get() })
			if (Ring) Ring->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOfficeRecruitmentModalWidget::TickHireFx(float DeltaTime)
{
	namespace Fx = OfficeRecruitHireFx;
	if (HireFxPhase == EHireFxPhase::Idle) return;
	HireFxClock += DeltaTime * OfficeRecruitModalDebug::FxTimeScale();
	const float Time = HireFxClock;
	switch (HireFxPhase)
	{
	case EHireFxPhase::Waiting:
		if (BodyDim) BodyDim->SetRenderOpacity(bHireFxCalm ? Fx::DimTarget * FMath::Clamp(Time / Fx::CalmFadeDuration, 0.f, 1.f) : Fx::DimOpacityAt(Time));
		if (!bHireFxCalm)
			if (UWidget* Button = HireFxButton.Get()) Button->SetRenderScale(FVector2D(Fx::PressScaleAt(Time)));
		break;
	case EHireFxPhase::Succeeding:
		if (ModalFrame) ModalFrame->SetRenderOpacity(1.f - FMath::Clamp(Time / Fx::CalmFadeDuration, 0.f, 1.f));
		if (Time >= Fx::CalmFadeDuration) FinishHireFxSuccess();
		break;
	case EHireFxPhase::Failing:
		if (BodyDim) BodyDim->SetRenderOpacity(Fx::DimTarget * (1.f - FMath::Clamp(Time / Fx::DimOutDuration, 0.f, 1.f)));
		if (!bHireFxCalm)
			if (UWidget* Button = HireFxButton.Get()) Button->SetRenderTranslation(FVector2D(Fx::ShakeOffsetAt(Time), 0.f));
		if (Time >= (bHireFxCalm ? Fx::DimOutDuration : Fx::ShakeDuration)) FinishHireFxFailure();
		break;
	default:
		break;
	}
}

void UOfficeRecruitmentModalWidget::FinishHireFxSuccess()
{
	// 채용 창 퇴장이 끝나면 책상 위 공개를 시작한다.
	ReleaseHeldEntrance();
	HireFxPhase = EHireFxPhase::Idle;
	bRequestPending = false;
	DeactivateWidget();
}

void UOfficeRecruitmentModalWidget::FinishHireFxFailure()
{
	const FText Message = HireFxFailureMessage;
	ResetHireFx(true);
	bRequestPending = false;
	ReleaseHeldEntrance();
	if (FailureText)
	{
		FailureText->SetText(Message);
		FailureText->SetVisibility(ESlateVisibility::Visible);
	}
	Refresh();
}

void UOfficeRecruitmentModalWidget::ReleaseHeldEntrance()
{
	if (!bHoldingEntrance) return;
	bHoldingEntrance = false;
	if (UOfficeRecruitmentFlowSubsystem* Flow = RecruitmentFlow.Get()) Flow->ReleaseEntrance();
}

void UOfficeRecruitmentModalWidget::DebugSelect(int32 JobIndex, int32 TierIndex)
{
	if (JobGroup && JobIndex >= 0 && JobIndex < JobGroup->GetButtonCount())
	{
		JobGroup->SelectButtonAtIndex(JobIndex, false);
		HandleJobClicked(nullptr, JobIndex);
	}
	if (TicketGroup && TierIndex >= 0 && TierIndex < TicketGroup->GetButtonCount())
	{
		TicketGroup->SelectButtonAtIndex(TierIndex, false);
		HandleTicketClicked(nullptr, TierIndex);
	}
}

void UOfficeRecruitmentModalWidget::DebugPress(bool bBatch)
{
	if (bBatch) HandlePullMultiClicked();
	else HandlePullClicked();
}

void UOfficeRecruitmentModalWidget::DebugOpenProbability()
{
	HandleProbabilityClicked();
}

void UOfficeRecruitmentModalWidget::DebugPressSeatGuide()
{
	HandleSeatGuideClicked();
}

void UOfficeRecruitmentModalWidget::HideOfficeIdleZone()
{
	RestoreOfficeIdleZone();
	for (TObjectIterator<UOfficeMainWidget> It; It; ++It)
	{
		if (It->HasAnyFlags(RF_ClassDefaultObject) || It->GetWorld() != GetWorld() || !It->IsVisible()) continue;
		// 창 오른쪽 뒤로 반쯤 보이는 [기획안 보기]가 초록 채용 버튼과 시선을 다투므로 창이 떠 있는 동안만 숨긴다.
		UWidget* Zone = It->GetWidgetFromName(TEXT("RightIdleZone"));
		if (Zone && Zone->IsVisible())
		{
			Zone->SetVisibility(ESlateVisibility::Hidden);
			HiddenOfficeIdleZone = Zone;
		}
		return;
	}
}

void UOfficeRecruitmentModalWidget::RestoreOfficeIdleZone()
{
	// 사무실 메인이 그사이 상태를 바꿨으면(Hidden 이 아니면) 그 값을 그대로 둔다.
	if (UWidget* Zone = HiddenOfficeIdleZone.Get())
		if (Zone->GetVisibility() == ESlateVisibility::Hidden) Zone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HiddenOfficeIdleZone.Reset();
}
