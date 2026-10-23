#include "UI/Panel/EmployeeWindowWidget.h"
#include "UI/Element/Employee/PotentialResetWorkspaceWidget.h"
#include "UI/Panel/PotentialOddsWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentListWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "UI/Element/Employee/EquipmentDetailWindowWidget.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "Manager/TableManagerSubsystem.h"
#include "Core/CGGameInstance.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "CommonTextBlock.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"

namespace
{
	// v2.1 빈 슬롯: 부위명 22 dim + 가운데 실루엣. 라벨은 아이콘의 캡션이라 잉크를 한 단 더 내린다
	const FLinearColor HomeSlotLabelDim = FLinearColor::FromSRGBColor(FColor(0x4A, 0x55, 0x68));
	constexpr float HomeSlotSilhouetteAlpha = 0.70f;
	// 빈 슬롯과 기본 외형 착용은 같은 흰 실루엣을 쓰고 알파로 구분한다.
	// 등급 = 타일을 두르는 2px 프레임(상단 액센트 바 폐기). 공유 MI 는 5px/164 로 구워져 있어
	// 홈 6칸의 MID 에만 타일 실치수를 주입한다 — 워크스페이스 카드는 그대로 둔다
	constexpr float HomeSlotFrameThickness = 2.f;
	constexpr float HomeSlotFrameRadius = 14.f;

	// 장비 등급 Common 의 전역색은 흰빛이라 프레임이 빈 슬롯(스틸 시안)과 다른 언어로 읽힌다
	// — 직원창 표시색만 한 톤 내린다(전역 DT 는 그대로). 상위 등급은 자기 등급색 유지.
	FLinearColor HomeSlotFrameColor(const FEmployeeEquipmentGradeRow* Grade, EEmployeeEquipmentGrade Tier)
	{
		return (!Grade || Tier == EEmployeeEquipmentGrade::Common) ? EmployeeRarityDisplay::CommonColor() : Grade->Color;
	}

	// 부위 실루엣 — EEmployeeEquipmentSlot 순서(Head/Glasses/Outfit/Hand/Shoes/Bag).
	// C++ 문자열 경로라 쿠킹 그래프가 못 따라간다 → 폴더 /UI/Textures/UIIcon 은 DefaultGame.ini 에 등록돼 있다
	UTexture2D* HomeSlotSilhouette(int32 SlotIndex)
	{
		static const TCHAR* const Names[] = { TEXT("Head"), TEXT("Glasses"), TEXT("Outfit"), TEXT("Hand"), TEXT("Shoes"), TEXT("Bag") };
		if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(UE_ARRAY_COUNT(Names))) return nullptr;
		const FString Path = FString::Printf(
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Slot/T_UIIcon_Slot_%s.T_UIIcon_Slot_%s"), Names[SlotIndex], Names[SlotIndex]);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
	}
}

// 확률표는 뷰포트 오버레이(ZOrder 120)라 드로어를 걷어도 스택이 안 거둬간다 — 잠재를 떠나는 모든 경로가 이걸 통과해야 한다
static void CloseOddsOverlay(TWeakObjectPtr<UPotentialOddsWidget>& OddsPanel)
{
	if (UPotentialOddsWidget* Odds = OddsPanel.Get()) Odds->RemoveFromParent();
	OddsPanel.Reset();
}

void UEmployeeWindowWidget::EnsurePotentialWorkspace()
{
	if (!ensureMsgf(PotentialHost, TEXT("EmployeeWindow: required PotentialHost is missing"))) return;
	if (!PotentialWorkspace)
	{
		const UTableManagerSubsystem* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const TSubclassOf<UUserWidget> WorkspaceClass = Tables ? Tables->GetWidgetClass(EWidgetType::PotentialResetWorkspace) : nullptr;
		if (!WorkspaceClass)
		{
			UE_LOG(LogTemp, Error, TEXT("EmployeeWindow: PotentialResetWorkspace is not registered"));
			return;
		}
		PotentialWorkspace = CreateWidget<UPotentialResetWorkspaceWidget>(GetOwningPlayer(), WorkspaceClass);
	}
	if (!PotentialWorkspace) return;
	if (PotentialWorkspace->GetParent() != PotentialHost)
	{
		if (UOverlaySlot* HostSlot = PotentialHost->AddChildToOverlay(PotentialWorkspace))
		{
			HostSlot->SetHorizontalAlignment(HAlign_Fill);
			HostSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	PotentialWorkspace->OnRequestOddsTable.BindUObject(this, &ThisClass::OpenOddsTable);
	PotentialWorkspace->OnRequestShop.BindUObject(this, &ThisClass::HandlePotentialShopRequested);
}

void UEmployeeWindowWidget::ShowEquipmentColumn(bool bShowEquipment)
{
	if (GrowthVBox) GrowthVBox->SetVisibility(bShowEquipment ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (EquipPlateSize) EquipPlateSize->SetVisibility(bShowEquipment ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShowEquipment) CloseInlineDetail();
	RefreshEquipmentToggleButton();
}

void UEmployeeWindowWidget::RefreshEquipmentToggleButton()
{
	if (!HeroEquipmentOpenButton) return;
	const bool bOnEquipment = CurrentEmployeePage == EEmployeeWindowPage::Equipment;
	// 라벨은 "지금 상태"가 아니라 "누르면 갈 곳"이다 — 그래서 판 제목이 상태를 따로 말한다.
	HeroEquipmentOpenButton->SetButtonText(bOnEquipment
		? NSLOCTEXT("EmployeeWindow", "ShowAbility", "능력")
		: NSLOCTEXT("EmployeeWindow", "ShowEquipment", "장비"));
}

void UEmployeeWindowWidget::RefreshEquipmentOwnedCount()
{
	if (!EquipCountText) return;
	UGameInstance* GI = GetGameInstance();
	auto* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables) return;
	int32 Owned = 0;
	for (const auto& Pair : Equipment->GetState().Instances)
		if (Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId)) ++Owned;
	EquipCountText->SetText(FText::Format(NSLOCTEXT("EmployeeWindow", "OwnedEquipment", "보유 {0}개"), FText::AsNumber(Owned)));
}

void UEmployeeWindowWidget::HandleEquipmentItemPicked(const FGuid& InstanceId)
{
	const bool bNewSelection = EquipmentList && EquipmentList->IsNewSelectionNotification(InstanceId);
	InlineDetailInstanceId = InstanceId;
	RefreshInlineDetail();
	if (bNewSelection && InlineDetail && InlineDetailInstanceId == InstanceId)
		InlineDetail->TriggerSelectedIconPulse();
}

void UEmployeeWindowWidget::RefreshInlineDetail()
{
	if (!EquipDetailHost) return;
	if (!InlineDetailInstanceId.IsValid()) { CloseInlineDetail(); return; }

	UGameInstance* GI = GetGameInstance();
	auto* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables) return;

	FEquipmentDetailContent Content;
	if (!CGEquipmentDetail::BuildContent(*Equipment, *Tables,
		GI->GetSubsystem<UEmployeeManager>(), SelectedEmployeeID, InlineDetailInstanceId, Content))
	{
		CloseInlineDetail();
		return;
	}

	if (!InlineDetail)
	{
		const TSubclassOf<UUserWidget> Class = Tables->GetWidgetClass(EWidgetType::EquipmentDetailWindow);
		if (!Class) { UE_LOG(LogTemp, Error, TEXT("[EquipmentUI] EquipmentDetailWindow is not registered")); return; }
		InlineDetail = CreateWidget<UEquipmentDetailWindowWidget>(GetOwningPlayer(), Class);
		if (!InlineDetail) return;
	}
	if (!InlineDetail->GetParent())
	{
		if (UPanelSlot* HostSlot = EquipDetailHost->AddChild(InlineDetail))
		{
			if (UOverlaySlot* AsOverlay = Cast<UOverlaySlot>(HostSlot))
			{
				AsOverlay->SetHorizontalAlignment(HAlign_Fill);
				AsOverlay->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
	EquipDetailHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (EquipDetailEmpty) EquipDetailEmpty->SetVisibility(ESlateVisibility::Collapsed);
	// 재사용 위젯의 델리게이트는 매 show 마다 다시 묶는다.
	InlineDetail->OnPrimary.BindUObject(this, &ThisClass::HandleInlineDetailPrimary);
	InlineDetail->OnSecondary.BindUObject(this, &ThisClass::HandleInlineDetailSecondary);
	InlineDetail->OnClose.BindUObject(this, &ThisClass::CloseInlineDetail);
	InlineDetail->Configure(Content);
}

void UEmployeeWindowWidget::CloseInlineDetail()
{
	InlineDetailInstanceId.Invalidate();
	if (InlineDetail)
	{
		InlineDetail->OnPrimary.Unbind();
		InlineDetail->OnSecondary.Unbind();
		InlineDetail->OnClose.Unbind();
		InlineDetail->RemoveFromParent();
	}
	// 자리는 비우지 않는다 ― 접으면 목록 폭이 튀어 고르던 자리를 잃는다.
	if (EquipDetailHost) EquipDetailHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (EquipDetailEmpty) EquipDetailEmpty->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (EquipmentList) EquipmentList->ClearSelection();
}

void UEmployeeWindowWidget::HandleInlineDetailPrimary()
{
	UGameInstance* GI = GetGameInstance();
	auto* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	auto* Employees = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables || !Employees || !Employees->GetEmployeeData(SelectedEmployeeID)) return;
	const auto* Item = Equipment->FindInstance(InlineDetailInstanceId);
	const auto* Def = Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (!Def) return;

	const FGuid InstanceId = InlineDetailInstanceId;
	const int32 EmployeeId = SelectedEmployeeID;
	const EEmployeeEquipmentSlot Part = Def->EquipmentSlot;
	// 요청은 누르는 순간에 만든다 ― 확인 창이 떠 있는 동안 리비전이 바뀌면 미리 만든 요청은 거절된다
	auto Commit = [this, InstanceId, EmployeeId, Part]()
	{
		auto* Equip = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
		if (!Equip || SelectedEmployeeID != EmployeeId || InlineDetailInstanceId != InstanceId) return;
		// 장착/해제 둘 다 SetSlot 이다 — 해제는 InstanceId 를 비워 보낸다.
		const int32 Wearer = Equip->GetInstanceEmployee(InstanceId);
		FEmployeeEquipmentRequest Request = Equip->MakeRequest(EEquipmentCommand::SetSlot);
		Request.EmployeeId = EmployeeId;
		Request.EquipmentSlot = Part;
		if (Wearer != EmployeeId)
		{
			Request.InstanceId = InstanceId;
			Request.ExpectedSourceEmployeeId = Wearer;
		}
		Equip->Execute(Request);
	};
	const int32 Holder = Equipment->GetInstanceEmployee(InstanceId);
	if (Holder != INDEX_NONE && Holder != EmployeeId)
	{
		// 다른 직원이 낀 장비는 한 번 묻고 가져온다 ― 그 직원 칸이 말없이 비지 않게
		CGEquipmentDetail::ShowTransferConfirm(*GI, Holder, EmployeeId, *Def, FSimpleDelegate::CreateWeakLambda(this, Commit));
		return;
	}
	Commit();
}

void UEmployeeWindowWidget::HandleInlineDetailSecondary()
{
	// 강화는 아직 이 경로에 없다 — 스타포스 모달이 붙기 전까지 조용히 두지 않고 로그로 남긴다.
	UE_LOG(LogTemp, Log, TEXT("[EquipmentUI] inline detail enhance is not wired yet"));
}

void UEmployeeWindowWidget::HandlePotentialShopRequested()
{
	auto* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	const auto* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	const auto Class = Tables ? Tables->GetWidgetClass(EWidgetType::ShopPanel) : nullptr;
	if (!UI || !UI->GetUIBase() || !Class) return;
	// 상점이 위에 쌓여도 선택 직원과 명함을 그대로 이어간다.
	bHasExplicitEmployeeContext = GetSelectedEmployee() != nullptr;
	bPotentialReturningFromShop = true;
	if (!UI->GetUIBase()->PushPromptClass(Class.Get())) bPotentialReturningFromShop = false;
}

void UEmployeeWindowWidget::OpenEquipmentItem(int32 EmployeeId, FGuid InstanceId)
{
	if (!EmployeeManager) EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (!EmployeeManager || !EmployeeManager->GetEmployeeData(EmployeeId)) return;
	PendingEquipmentEmployeeId = EmployeeId;
	PendingEquipmentInstanceId = InstanceId;
	if (IsActivated()) ApplyPendingEquipmentItem();
}

void UEmployeeWindowWidget::ApplyPendingEquipmentItem()
{
	if (PendingEquipmentEmployeeId == INDEX_NONE) return;
	const int32 EmployeeId = PendingEquipmentEmployeeId;
	const FGuid InstanceId = PendingEquipmentInstanceId;
	PendingEquipmentEmployeeId = INDEX_NONE;
	PendingEquipmentInstanceId.Invalidate();
	if (!EmployeeManager || !EmployeeManager->GetEmployeeData(EmployeeId)) return;
	OpenWithContext(EmployeeId, true);
	// 바깥(상점·미션)에서 특정 장비로 들어온 경로 — 그 부위로 걸러 두고 상세를 바로 편다.
	UGameInstance* GI = GetGameInstance();
	auto* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const auto* Item = Equipment ? Equipment->FindInstance(InstanceId) : nullptr;
	const auto* Def = Item && Tables ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (EquipmentList && Def)
	{
		EquipmentList->SelectAll();
		EquipmentList->SelectPart(Def->EquipmentSlot);
		EquipmentList->SelectItem(InstanceId);
	}
}

void UEmployeeWindowWidget::ShowEmployeePage(EEmployeeWindowPage Page)
{
	if (Page != EEmployeeWindowPage::Home && !GetSelectedEmployee()) Page = EEmployeeWindowPage::Home;
	const bool bPageChanged = CurrentEmployeePage != Page;
	if (bPageChanged) FinishPageTransition();
	if (Page != EEmployeeWindowPage::Home)
	{
		if (PotentialWorkspace) PotentialWorkspace->Suspend();
		CloseOddsOverlay(OpenOddsPanel);
	}
	CurrentEmployeePage = Page;
	// 장비는 칼럼 교체다 — 초상과 장착 6칸은 계속 보여야 하므로 무대를 반납하지 않는다.
	ShowEquipmentColumn(Page == EEmployeeWindowPage::Equipment);
	if (Page == EEmployeeWindowPage::Equipment)
	{
		bHasExplicitEmployeeContext = true;
		if (EquipmentList) EquipmentList->Configure(SelectedEmployeeID);
		RefreshEquipmentOwnedCount();
	}
	else if (EmployeeManager)
	{
		RefreshSelected();
	}
	if (bPageChanged && IsActivated() && !FMotionGate::IsReduced())
	{
		// 사용자 전환은 창 등장 스태거를 기다리지 않는다.
		IntroElapsed = -1.f;
		for (const TWeakObjectPtr<UWidget>& WeakPanel : IntroPanels)
		{
			if (UWidget* Panel = WeakPanel.Get())
			{
				Panel->SetRenderOpacity(1.f);
				Panel->SetRenderTranslation(FVector2D::ZeroVector);
			}
		}
		PageTransitionTarget = Page == EEmployeeWindowPage::Equipment ? EquipPlateSize.Get() : GrowthVBox.Get();
		if (UWidget* Target = PageTransitionTarget.Get())
		{
			PageTransitionElapsed = 0.f;
			Target->SetRenderOpacity(0.35f);
		}
	}
}

void UEmployeeWindowWidget::FinishPageTransition()
{
	if (UWidget* Target = PageTransitionTarget.Get()) Target->SetRenderOpacity(1.f);
	PageTransitionTarget.Reset();
	PageTransitionElapsed = -1.f;
}

void UEmployeeWindowWidget::HandleHeroEquipmentOpen()
{
	if (!GetSelectedEmployee()) return;
	// 같은 버튼이 왕복한다 — 장비를 보고 있으면 능력으로 돌아간다.
	const bool bOnEquipment = CurrentEmployeePage == EEmployeeWindowPage::Equipment;
	ShowEmployeePage(bOnEquipment ? EEmployeeWindowPage::Home : EEmployeeWindowPage::Equipment);
	if (!bOnEquipment && EquipmentList) EquipmentList->SelectAll();
}

void UEmployeeWindowWidget::HandleHeroEquipmentSlot(int32 Index)
{
	if (!GetSelectedEmployee() || Index < 0 || Index >= static_cast<int32>(EEmployeeEquipmentSlot::Count)) return;
	ShowEmployeePage(EEmployeeWindowPage::Equipment);
	if (!EquipmentList) return;
	const EEmployeeEquipmentSlot EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Index);
	EquipmentList->SelectAll();
	EquipmentList->SelectPart(EquipmentSlot);
	if (const auto* EquippedItem = EquipmentManager ? EquipmentManager->GetEquippedInstance(SelectedEmployeeID, EquipmentSlot) : nullptr)
	{
		EquipmentList->SelectItem(EquippedItem->InstanceId, true);
	}
}

void UEmployeeWindowWidget::RefreshHeroEquipment()
{
	const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UButtonWidget* HeroSlots[] = {HeroEquipmentSlot0, HeroEquipmentSlot1, HeroEquipmentSlot2, HeroEquipmentSlot3, HeroEquipmentSlot4, HeroEquipmentSlot5};
	HeroSlotIconMaterials.SetNum(UE_ARRAY_COUNT(HeroSlots));
	const bool bHasEmployee = GetSelectedEmployee() != nullptr;
	if (HeroEquipmentOpenButton) HeroEquipmentOpenButton->SetIsEnabled(bHasEmployee);
	RefreshEquipmentToggleButton();

	SyncEffectBaselineOwner();
	// 드로어 안 갱신은 기준선을 밀지 않는다 — 홈으로 돌아온 refresh 가 바뀐 슬롯을 한 번에 본다(수용 기준 ③)
	const bool bCompareSlots = LastSlotItemIds.Num() == UE_ARRAY_COUNT(HeroSlots) && CurrentEmployeePage == EEmployeeWindowPage::Home;
	TArray<FGuid> CurrentSlotIds;
	CurrentSlotIds.SetNum(UE_ARRAY_COUNT(HeroSlots));

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(HeroSlots); ++Index)
	{
		if (!HeroSlots[Index]) continue;
		HeroSlots[Index]->SetIsEnabled(bHasEmployee);
		const auto Part = static_cast<EEmployeeEquipmentSlot>(Index);
		const auto* Item = EquipmentManager && bHasEmployee ? EquipmentManager->GetEquippedInstance(SelectedEmployeeID, Part) : nullptr;
		auto* SlotButton = Cast<UIconWithButtonWidget>(HeroSlots[Index]);
		// 의상·신발은 전원이, 안경은 직급이 되면 기본 파츠를 이미 입고 있다 — 진짜 빈 칸과 같은 회색으로 두지 않는다
		const bool bWearsBase = !Item && bHasEmployee && EmployeeManager && EmployeeManager->HasBaseAppearance(SelectedEmployeeID, Part);
		CGEquipmentIcon::ApplySlotButton(this, SlotButton, Tables, Part, Item, HeroSlotIconMaterials[Index], bWearsBase);
		// 공용 ApplySlotButton 은 라이트 판 잉크를 쓴다 — 다크 무대의 빈 슬롯 라벨만 뮤트로 되돌린다
		if (!Item && SlotButton) SlotButton->SetTextColor(FSlateColor(HomeSlotLabelDim));
		if (UImage* Silhouette = Cast<UImage>(HeroSlots[Index]->GetWidgetFromName(TEXT("SlotEmptyIcon"))))
		{
			if (!Item)
			{
				if (UTexture2D* Glyph = HomeSlotSilhouette(Index))
				{
					if (UMaterialInstanceDynamic* IconMaterial = Silhouette->GetDynamicMaterial())
					{
						IconMaterial->SetTextureParameterValue(TEXT("IconTexture"), Glyph);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("EmployeeWindow: SlotEmptyIcon material missing for slot %d: %s"), Index, *Silhouette->GetPathName());
						Silhouette->SetBrushFromTexture(Glyph);
					}
				}
				Silhouette->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bWearsBase ? 1.f : HomeSlotSilhouetteAlpha));
			}
			Silhouette->SetVisibility(Item ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		// 밴드·Lv 는 v2 홈 슬롯 전용 표기 — 등급색 출처는 ApplySlotButton 과 같은 DT_EmployeeEquipmentGrade
		const auto* Definition = Tables && Item ? Tables->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
		const auto* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
		// 등급 액자 = 타일 전면 2px 프레임. 빈 슬롯도 일반 표시색으로 두른다(6칸 외곽 두께가 같아야 한다)
		if (UImage* Frame = Cast<UImage>(HeroSlots[Index]->GetWidgetFromName(TEXT("GradeFrame"))))
		{
			Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
			Frame->SetColorAndOpacity(FLinearColor::White);
			if (UMaterialInstanceDynamic* FrameMaterial = Frame->GetDynamicMaterial())
			{
				FrameMaterial->SetVectorParameterValue(TEXT("LineCol"),
					HomeSlotFrameColor(Grade, Definition ? Definition->Grade : EEmployeeEquipmentGrade::Common));
				FrameMaterial->SetScalarParameterValue(TEXT("ThickPx"), HomeSlotFrameThickness);
				FrameMaterial->SetScalarParameterValue(TEXT("RadiusPx"), HomeSlotFrameRadius);
				FrameMaterial->SetScalarParameterValue(TEXT("BaseA"), 1.f);
			}
		}
		if (UTextBlock* LevelLabel = Cast<UTextBlock>(HeroSlots[Index]->GetWidgetFromName(TEXT("SlotLevelText"))))
		{
			// Lv.0 은 정보가 아니다 — 강화된 장비만 표기
			const bool bShowLevel = Item && Item->EnhanceLevel > 0;
			LevelLabel->SetVisibility(bShowLevel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bShowLevel) LevelLabel->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), Item->EnhanceLevel)));
		}
		// 스파클 틴트는 등급 프레임과 같은 등급색 출처를 쓴다 (등급 없으면 화이트)
		CurrentSlotIds[Index] = Item ? Item->InstanceId : FGuid();
		SlotSparkleColor[Index] = Grade ? Grade->Color : FLinearColor::White;
	}

	if (bCompareSlots)
	{
		for (int32 Index = 0; Index < CurrentSlotIds.Num(); ++Index)
		{
			if (LastSlotItemIds[Index] != CurrentSlotIds[Index])
			{
				SlotPunch[Index].Start(1.12f, 0.25f);
				SlotSparkleElapsed[Index] = 0.f;
			}
		}
	}
	if (bCompareSlots || LastSlotItemIds.Num() != CurrentSlotIds.Num())
	{
		LastSlotItemIds = MoveTemp(CurrentSlotIds);
	}
}

void UEmployeeWindowWidget::UpdateHeroEquipmentFrameMaterialSize()
{
	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (const TWeakObjectPtr<UImage>& FramePtr : HeroSlotFrameImages)
	{
		UImage* Frame = FramePtr.Get();
		if (!Frame) continue;
		// 첫 배치 전의 0 크기는 건너뛰고, 터치 영역 대신 실제 프레임 면을 측정한다.
		const FVector2D LocalSize = Frame->GetCachedGeometry().GetLocalSize();
		if (LocalSize.X < 1.f || LocalSize.Y < 1.f) continue;
		UMaterialInstanceDynamic* FrameMaterial = Frame->GetDynamicMaterial();
		if (!FrameMaterial) continue;
		if (FMath::IsNearlyEqual(FrameMaterial->K2_GetScalarParameterValue(WpxParam), LocalSize.X, 0.5f)
			&& FMath::IsNearlyEqual(FrameMaterial->K2_GetScalarParameterValue(HpxParam), LocalSize.Y, 0.5f)) continue;
		FrameMaterial->SetScalarParameterValue(WpxParam, LocalSize.X);
		FrameMaterial->SetScalarParameterValue(HpxParam, LocalSize.Y);
	}
}

void UEmployeeWindowWidget::HandleHeroEquipmentChanged()
{
	// 슬롯 펀치·스파클은 RefreshHeroEquipment 가 이전/현재 InstanceId 를 비교해 건다(드로어 안에서는 미룬다)
	RefreshHeroEquipment();
	RefreshHeroStats();
	// 장착이 바뀌면 목록의 "착용" 표시와 펼친 상세의 전/후 비교가 둘 다 낡는다.
	if (CurrentEmployeePage == EEmployeeWindowPage::Equipment)
	{
		if (EquipmentList) EquipmentList->Refresh();
		RefreshEquipmentOwnedCount();
		if (InlineDetailInstanceId.IsValid()) RefreshInlineDetail();
	}
	if (IsActivated()) UpdateHeroLiveView(GetSelectedEmployee());
}

void UEmployeeWindowWidget::HandleHeroPortraitChanged(int32 EmployeeId, int64 Revision)
{
	if (EmployeeId == SelectedEmployeeID && IsActivated() && !bLiveHeroViewEnabled
		&& CurrentEmployeePage == EEmployeeWindowPage::Home)
		UpdateHeroLiveView(GetSelectedEmployee());
}


bool UEmployeeWindowWidget::NativeOnHandleBackAction()
{
	if (OpenOddsPanel.IsValid())
	{
		CloseOddsOverlay(OpenOddsPanel);
		return true;
	}
	if (CurrentEmployeePage != EEmployeeWindowPage::Home)
	{
		// 상세가 펼쳐져 있으면 그것부터 접는다 — 한 번에 홈까지 가면 고르던 자리를 잃는다.
		if (InlineDetailInstanceId.IsValid()) { CloseInlineDetail(); return true; }
		ShowEmployeePage(EEmployeeWindowPage::Home);
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

void UEmployeeWindowWidget::OpenWithContext(int32 EmployeeId, bool bOpenEquipment)
{
	if (!EmployeeManager) EmployeeManager = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->GetEmployeeData(EmployeeId) : nullptr;
	if (Employee && RosterFilterDiscipline != EProductionDiscipline::Count
		&& static_cast<int32>(RosterFilterDiscipline) != GetPrimaryDisciplineSlot(*Employee))
	{
		RosterFilterDiscipline = EProductionDiscipline::Count;
	}
	const UCGGameInstance* Instance = Cast<UCGGameInstance>(GetGameInstance());
	BuildingIndex = Employee ? Employee->AssignedBuildingIndex : Instance ? Instance->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	bHasExplicitEmployeeContext = Employee != nullptr;
	SetupRosterFilterCombo();
	RefreshRoster();
	if (Employee) SelectEmployee(EmployeeId);
	ShowEmployeePage(bOpenEquipment && Employee ? EEmployeeWindowPage::Equipment : EEmployeeWindowPage::Home);
}
