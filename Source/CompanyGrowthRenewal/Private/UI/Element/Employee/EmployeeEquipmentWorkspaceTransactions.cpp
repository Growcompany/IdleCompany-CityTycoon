#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Employee/EquipmentDetailWindowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/ResourceItemManager.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CommonTextBlock.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#define LOCTEXT_NAMESPACE "EmployeeEquipmentUI"

void UEmployeeEquipmentWorkspaceWidget::ShowEnhance(EEquipmentMaterialMode Mode)
{
	const auto* Item = EquipmentManager->FindInstance(SelectedInstanceId);
	const auto* Definition = Item ? TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	const auto* Economy = TableManager->GetEmployeeEquipmentEconomy();
	if (!Definition || !Economy) return;
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::Enhance);
	PendingRequest.InstanceId = Item->InstanceId;
	PendingRequest.ExpectedLevel = Item->EnhanceLevel;
	PendingRequest.MaterialMode = Mode;
	int32 CoreCost = 0, DuplicateCount = 0;
	const bool bQuoted = EquipmentManager->QuoteEnhance(SelectedInstanceId, Mode, CoreCost, DuplicateCount);
	if (Mode == EEquipmentMaterialMode::Duplicate)
	{
		TArray<FGuid> Eligible;
		for (const auto& Pair : EquipmentManager->GetState().Instances)
		{
			if (Pair.Key != SelectedInstanceId && Pair.Value.DefinitionId == Item->DefinitionId && Pair.Value.EnhanceLevel == 0 && !Pair.Value.bLocked && EquipmentManager->GetInstanceEmployee(Pair.Key) < 0) Eligible.Add(Pair.Key);
		}
		Eligible.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
		for (int32 Index = 0; Index < FMath::Min(DuplicateCount, Eligible.Num()); ++Index) PendingRequest.InstanceIds.Add(Eligible[Index]);
	}
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("EquipmentEnhance", "장비 강화"); View.bShowCurrent = false;
	FillDefinitionView(View, *Definition, Item->EnhanceLevel);
	const auto Before = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(*Definition, Item->EnhanceLevel, TableManager->GetEmployeeEquipmentEffects(), *Economy);
	const auto After = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(*Definition, FMath::Min(Item->EnhanceLevel + 1, Economy->MaxEnhanceLevel), TableManager->GetEmployeeEquipmentEffects(), *Economy);
	if (const auto* Primary = TableManager->FindEmployeeEquipmentEffect(Definition->PrimaryEffectId))
	{
		const float BeforeValue = Before.BasicValues.FindRef(Definition->PrimaryEffectId);
		const float AfterValue = After.BasicValues.FindRef(Definition->PrimaryEffectId);
		const int32 Digits = FMath::Max(CGEquipmentDetail::FractionDigits(CGEquipmentDetail::DisplayValue(*Primary, BeforeValue)),
			CGEquipmentDetail::FractionDigits(CGEquipmentDetail::DisplayValue(*Primary, AfterValue)));
		View.Comparison = FText::Format(LOCTEXT("EnhanceComparison", "{0} {1} → {2}\n주옵션 효과만 강화됩니다. 보조옵션·고유효과는 유지됩니다."), Primary->DisplayName,
			FText::FromString(CGEquipmentDetail::FormatEffectValue(*Primary, BeforeValue, Digits)),
			FText::FromString(CGEquipmentDetail::FormatEffectValue(*Primary, AfterValue, Digits)));
	}
	const auto* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
	View.Notice = Mode == EEquipmentMaterialMode::Core
		? FText::Format(LOCTEXT("CoreCost", "코어 {0}개 / 보유 {1}개 · 성공률 100%"), FText::AsNumber(CoreCost), FText::AsNumber(Items ? Items->GetItemCount(EItemType::EmployeeEquipmentCore) : 0))
		: FText::Format(LOCTEXT("DuplicateCost", "동일 장비 +0 {0}개 / 선택 {1}개 · 성공률 100%"), FText::AsNumber(DuplicateCount), FText::AsNumber(PendingRequest.InstanceIds.Num()));
	View.Primary = Item->EnhanceLevel >= Economy->MaxEnhanceLevel ? LOCTEXT("MaxEnhance", "최대 강화") : LOCTEXT("EnhanceCommit", "강화하기");
	const bool bHaveMaterials = Mode == EEquipmentMaterialMode::Core
		? Items && Items->GetItemCount(EItemType::EmployeeEquipmentCore) >= CoreCost
		: PendingRequest.InstanceIds.Num() == DuplicateCount;
	View.bPrimaryEnabled = bQuoted && bHaveMaterials && Item->EnhanceLevel < Economy->MaxEnhanceLevel;
	View.Secondary = Mode == EEquipmentMaterialMode::Core ? LOCTEXT("UseDuplicate", "동일 장비 사용") : LOCTEXT("UseCore", "코어 사용");
	View.Tertiary = LOCTEXT("Cancel", "취소");
	ShowDialog(View);
	if (!Dialog) return;
	Dialog->OnPrimary.BindUObject(this, &ThisClass::ExecutePending);
	Dialog->OnSecondary.BindUObject(this, &ThisClass::ShowEnhance, Mode == EEquipmentMaterialMode::Core ? EEquipmentMaterialMode::Duplicate : EEquipmentMaterialMode::Core);
	if (IsInspectorActive() && !bReturnToResults) Dialog->OnTertiary.BindUObject(this, &ThisClass::HandleDialogBack);
	else Dialog->OnTertiary.BindUObject(this, &ThisClass::ShowDetail, SelectedInstanceId);
}

void UEmployeeEquipmentWorkspaceWidget::ShowDismantle()
{
	const auto* Item = EquipmentManager->FindInstance(SelectedInstanceId);
	if (!Item) return;
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::Dismantle);
	PendingRequest.InstanceIds = {SelectedInstanceId}; PendingRequest.bConfirmEnhancedDismantle = Item->EnhanceLevel > 0;
	int32 Cores = 0;
	const bool bQuoted = EquipmentManager->QuoteDismantle(PendingRequest.InstanceIds, Cores);
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("DismantleConfirm", "장비 분해"); View.bShowCurrent = false;
	if (const auto* Def = TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId)) FillDefinitionView(View, *Def, Item->EnhanceLevel);
	View.Description = LOCTEXT("DismantlePermanent", "장비를 영구히 소모합니다. 해금한 외형은 남습니다. 강화에 사용한 재료는 돌려받지 않습니다.");
	View.Notice = FText::Format(LOCTEXT("DismantleAmount", "장비 1개 소모 · 코어 {0}개 획득"), FText::AsNumber(Cores));
	View.Primary = LOCTEXT("Dismantle", "분해하기"); View.bPrimaryEnabled = bQuoted; View.Secondary = LOCTEXT("Cancel", "취소");
	ShowDialog(View);
	if (Dialog) { Dialog->OnPrimary.BindUObject(this, &ThisClass::ExecutePending); Dialog->OnSecondary.BindUObject(this, &ThisClass::HandleDialogBack); }
}

void UEmployeeEquipmentWorkspaceWidget::ShowBulk()
{
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("BulkDismantle", "일괄 분해");
	View.Notice = LOCTEXT("BulkEmpty", "선택 0개 · 미장착·잠금 해제된 +0 장비만 선택할 수 있습니다.");
	View.Primary = LOCTEXT("BulkReview", "분해 내용 확인"); View.bPrimaryEnabled = false; View.Secondary = LOCTEXT("Cancel", "취소");
	ShowDialog(View);
	if (!Dialog) return;
	DialogEntries.Empty(); TArray<UObject*> List;
	for (const auto& Pair : EquipmentManager->GetState().Instances)
	{
		if (Pair.Value.bLocked || Pair.Value.EnhanceLevel > 0 || EquipmentManager->GetInstanceEmployee(Pair.Key) >= 0) continue;
		if (const auto* Def = TableManager->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId))
		{
			auto* Entry = MakeEntry(*Def, &Pair.Value); DialogEntries.Add(Entry); List.Add(Entry);
		}
	}
	Dialog->ShowItems(List, true);
	TArray<UObject*> Restore;
	for (UObject* Object : List) if (const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Object)) if (BulkSelectionIds.Contains(Entry->InstanceID)) Restore.Add(Object);
	Dialog->RestoreSelectedItems(Restore);
	Dialog->OnSelectionChanged.BindUObject(this, &ThisClass::UpdateBulkSelection);
	Dialog->OnPrimary.BindUObject(this, &ThisClass::ShowBulkConfirm);
	Dialog->OnSecondary.BindUObject(this, &ThisClass::DismissDialog);
	bBulkDialog = true;
	UpdateBulkSelection();
}

void UEmployeeEquipmentWorkspaceWidget::UpdateBulkSelection()
{
	if (!Dialog || !bBulkDialog) return;
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::Dismantle);
	for (UObject* Object : Dialog->GetSelectedItems()) if (const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Object)) PendingRequest.InstanceIds.Add(Entry->InstanceID);
	BulkSelectionIds = PendingRequest.InstanceIds;
	int32 Cores = 0;
	const bool bQuoted = EquipmentManager->QuoteDismantle(PendingRequest.InstanceIds, Cores);
	Dialog->SetNotice(FText::Format(LOCTEXT("BulkSelection", "선택 {0}개 · 코어 {1}개 획득 · 분해한 장비는 되돌릴 수 없습니다."), FText::AsNumber(PendingRequest.InstanceIds.Num()), FText::AsNumber(Cores)));
	Dialog->SetPrimaryEnabled(bQuoted && !PendingRequest.InstanceIds.IsEmpty());
}

void UEmployeeEquipmentWorkspaceWidget::ShowBulkConfirm()
{
	UpdateBulkSelection();
	int32 Cores = 0;
	if (!EquipmentManager->QuoteDismantle(PendingRequest.InstanceIds, Cores) || PendingRequest.InstanceIds.IsEmpty()) return;
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("BulkConfirmTitle", "일괄 분해 최종 확인"); View.bShowCurrent = false;
	View.Description = LOCTEXT("BulkConfirmPermanent", "선택한 장비를 영구히 소모합니다. 해금한 외형은 유지되며 분해한 장비는 되돌릴 수 없습니다.");
	View.Notice = FText::Format(LOCTEXT("BulkFinalAmount", "장비 {0}개 소모 · 코어 {1}개 획득"), FText::AsNumber(PendingRequest.InstanceIds.Num()), FText::AsNumber(Cores));
	View.Primary = LOCTEXT("BulkCommit", "분해 확정"); View.Secondary = LOCTEXT("BulkBack", "다시 선택");
	ShowDialog(View);
	if (Dialog) { Dialog->OnPrimary.BindUObject(this, &ThisClass::ExecutePending); Dialog->OnSecondary.BindUObject(this, &ThisClass::ShowBulk); }
}

void UEmployeeEquipmentWorkspaceWidget::ShowOdds()
{
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("EquipmentOdds", "장비 획득 확률"); View.bShowCurrent = false;
	const auto Weights = EquipmentManager->GetNextGradeWeights(CurrentBannerId);
	int32 Total = 0; for (int32 Weight : Weights) Total += Weight;
	const auto* Banner = TableManager->FindEmployeeEquipmentBanner(CurrentBannerId);
	int32 BaseTotal = 0; if (Banner) for (int32 Weight : Banner->GradeWeights) BaseTotal += Weight;
	FString Lines;
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		const auto* Grade = TableManager->FindEmployeeEquipmentGrade(static_cast<EEmployeeEquipmentGrade>(Index));
		if (Grade) Lines += FString::Printf(TEXT("%s · 기본 %.4f%% / 다음 1회 %.4f%%\n"), *Grade->DisplayName.ToString(), BaseTotal > 0 ? 100.0 * Banner->GradeWeights[Index] / BaseTotal : 0.0, Total > 0 ? 100.0 * Weights[Index] / Total : 0.0);
	}
	Lines += TEXT("\n품목별 다음 1회 실효 확률\n");
	const auto Probabilities = EquipmentManager->GetNextDefinitionProbabilities(CurrentBannerId);
	TArray<FName> Keys; Probabilities.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
	for (FName Key : Keys)
	{
		if (const auto* Def = TableManager->FindEmployeeEquipmentDefinition(Key)) Lines += FString::Printf(TEXT("%s %.6f%%\n"), *Def->DisplayName.ToString(), Probabilities[Key] * 100.0);
	}
	View.Description = FText::FromString(Lines);
	View.Notice = LOCTEXT("OddsScope", "현재 보장 횟수가 반영된 다음 1회의 확률입니다.");
	View.Primary = LOCTEXT("Confirm", "확인");
	ShowDialog(View);
	if (Dialog) Dialog->OnPrimary.BindUObject(this, &ThisClass::DismissDialog);
}

void UEmployeeEquipmentWorkspaceWidget::ShowEffects()
{
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("CurrentEffects", "현재 직원의 장비 효과"); View.bShowCurrent = false;
	FString Lines;
	if (const auto* Economy = TableManager->GetEmployeeEquipmentEconomy())
	{
		const auto Snapshot = FEmployeeEquipmentEffectEvaluator::BuildLoadout(EquipmentManager->GetState(), CurrentEmployeeId, TableManager->GetEmployeeEquipmentDefinitions(), TableManager->GetEmployeeEquipmentEffects(), *Economy);
		for (const auto& Pair : Snapshot.BasicValues) if (const auto* Effect = TableManager->FindEmployeeEquipmentEffect(Pair.Key)) Lines += FString::Printf(TEXT("%s %s\n"), *Effect->DisplayName.ToString(), *CGEquipmentDetail::FormatEffectValue(*Effect, Pair.Value));
		for (const auto& Pair : Snapshot.UniqueEffects) Lines += TEXT("\n") + Pair.Value.DisplayName.ToString() + TEXT("\n") + Pair.Value.Description.ToString() + TEXT("\n");
	}
	View.Description = Lines.IsEmpty() ? LOCTEXT("NoActiveEffects", "현재 적용된 장비 효과가 없습니다.") : FText::FromString(Lines);
	View.Notice = LOCTEXT("EquipmentOnly", "장비 합산만 표시합니다. 직원 기본 능력치와 잠재는 육성 화면에서 확인합니다.");
	View.Primary = LOCTEXT("Confirm", "확인"); ShowDialog(View);
	if (Dialog) Dialog->OnPrimary.BindUObject(this, &ThisClass::DismissDialog);
}

void UEmployeeEquipmentWorkspaceWidget::ExecutePending()
{
	if (Dialog) Dialog->SetPrimaryEnabled(false);
	if (IsInspectorActive() && InspectorEquipButton) InspectorEquipButton->SetIsEnabled(false);
	InspectorFailureNotice = FText();
	const EEquipmentCommand Command = PendingRequest.Command;
	FGachaRevealAwardContext Award;
	TSet<FName> OwnedBefore;
	if (Command == EEquipmentCommand::Pull)
	{
		const FEmployeeEquipmentState& Before = EquipmentManager->GetState();
		OwnedBefore = Before.UnlockedDefinitionIds;
		if (const FEmployeeEquipmentBannerRow* Banner = TableManager->FindEmployeeEquipmentBanner(PendingRequest.BannerId))
		{
			Award.GradeWeights = Banner->GradeWeights;
			Award.PityThreshold = Banner->AscendantPity;
			Award.PityBefore = Before.BannerStates.FindRef(Banner->PityGroupId).SinceAscendant;
		}
	}
	const auto Result = EquipmentManager->Execute(PendingRequest);
	if (!Result.bCommitted)
	{
		if (Command == EEquipmentCommand::Pull && !Dialog)
		{
			FEmployeeEquipmentDialogView View;
			View.Title = LOCTEXT("PullFailed", "장비를 뽑지 못했습니다");
			View.bShowCurrent = false;
			View.Notice = LOCTEXT("PullFailedNoSpend", "상자는 사용되지 않았습니다. 잠시 후 다시 시도해 주세요.");
			View.Primary = LOCTEXT("Confirm", "확인");
			ShowDialog(View);
			if (Dialog) Dialog->OnPrimary.BindUObject(this, &ThisClass::DismissDialog);
			Refresh();
			return;
		}
		if (Dialog) { Dialog->SetNotice(LOCTEXT("OperationFailed", "장비·재화 상태 또는 이용 조건이 변경되어 실행하지 못했습니다. 창을 닫고 다시 확인하십시오.")); Dialog->SetPrimaryEnabled(false); }
		if (IsInspectorActive())
		{
			InspectorFailureNotice = LOCTEXT("InspectorOperationFailed", "장비 상태 또는 이용 조건이 변경되어 실행하지 못했습니다. 장비를 다시 선택해 확인해 주세요.");
			RefreshInspector();
		}
		return;
	}
	if (!Result.GrantedInstances.IsEmpty())
	{
		if (Command == EEquipmentCommand::Pull)
		{
			for (const FEmployeeEquipmentInstance& Item : Result.GrantedInstances)
			{
				Award.bOwnedBefore.Add(OwnedBefore.Contains(Item.DefinitionId));
				OwnedBefore.Add(Item.DefinitionId);
			}
			if (const FEmployeeEquipmentBannerRow* Banner = TableManager->FindEmployeeEquipmentBanner(Result.BannerId))
			{
				Award.PityAfter = EquipmentManager->GetState().BannerStates.FindRef(Banner->PityGroupId).SinceAscendant;
			}
		}
		LastResults.Empty(); for (const auto& Item : Result.GrantedInstances) LastResults.Add(Item.InstanceId);
		SelectedInstanceId = LastResults[0]; bReturnToResults = false; bResultsRevealPending = true;
		if (Command == EEquipmentCommand::Pull) ShowResultReveal(Award, Result.TicketType, CurrentEmployeeId); else ShowResults();
	}
	else if (Command == EEquipmentCommand::SetLock || Command == EEquipmentCommand::Enhance)
	{
		if (IsInspectorActive() && !bReturnToResults) DismissDialog();
		else ShowDetail(SelectedInstanceId);
	}

	else if (bReturnToResults) { bReturnToResults = false; ShowResults(); }
	else DismissDialog();
	Refresh();
	if (Command == EEquipmentCommand::Enhance)
	{
		CGEquipmentIcon::TriggerPulse(InspectorIconMaterial);
		if (DetailWindow) DetailWindow->TriggerSelectedIconPulse();
		if (Dialog)
		{
			UImage* SelectedIcon = Cast<UImage>(Dialog->GetWidgetFromName(TEXT("SelectedIconImage")));
			if (SelectedIcon)
				CGEquipmentIcon::TriggerPulse(Cast<UMaterialInstanceDynamic>(SelectedIcon->GetBrush().GetResourceObject()));
		}
	}
}

void UEmployeeEquipmentWorkspaceWidget::ShowResults()
{
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("AcquiredEquipment", "장비 획득"); View.Notice = LOCTEXT("ResultCommitted", "획득한 장비를 선택해 확인하고 장착할 수 있습니다.");
	View.Primary = LOCTEXT("Confirm", "확인"); View.Secondary = LOCTEXT("Inventory", "보관함"); ShowDialog(View);
	if (!Dialog) return;
	DialogEntries.Empty(); TArray<UObject*> List; UObject* Selected = nullptr;
	for (const FGuid& Id : LastResults)
	{
		const auto* Item = EquipmentManager->FindInstance(Id);
		const auto* Def = Item ? TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
		if (!Def) continue;
		auto* Entry = MakeEntry(*Def, Item);
		Entry->IconContext = bResultsRevealPending ? EEmployeeEquipmentIconContext::Reveal : EEmployeeEquipmentIconContext::Idle;
		DialogEntries.Add(Entry); List.Add(Entry);
		if (Id == SelectedInstanceId) Selected = Entry;
	}
	Dialog->ShowItems(List); Dialog->RestoreSelectedItem(Selected);
	bResultsRevealPending = false;
	Dialog->OnItemClicked.BindUObject(this, &ThisClass::HandleResultClicked);
	Dialog->OnPrimary.BindUObject(this, &ThisClass::DismissDialog);
	Dialog->OnSecondary.BindUObject(this, &ThisClass::HandleStorage);
}

void UEmployeeEquipmentWorkspaceWidget::ShowResultReveal(const FGachaRevealAwardContext& Award, EItemType SourceTicket, int32 EmployeeId)
{
	if (Dialog) { Dialog->RemoveFromParent(); Dialog = nullptr; }
	const auto Class = TableManager->GetWidgetClass(EWidgetType::GachaRevealPresentation);
	RevealPresentation = Class ? CreateWidget<UGachaRevealPresentationWidget>(GetOwningPlayer(), Class) : nullptr;
	if (!RevealPresentation) { ShowResults(); return; }
	TArray<FEmployeeEquipmentInstance> Results;
	for (const FGuid& Id : LastResults) if (const auto* Item = EquipmentManager->FindInstance(Id)) Results.Add(*Item);
	RevealPresentation->AddToViewport(130);
	RevealPresentation->OnPresentationClosed.BindUObject(this, &ThisClass::HandleRevealClosed);
	if (GachaImage && TableManager)
	{
		const EGachaBoxType BoxType = SourceTicket == EItemType::EmployeeEquipmentTicketPremium ? EGachaBoxType::EquipmentPremium
			: SourceTicket == EItemType::EmployeeEquipmentTicketAdvanced ? EGachaBoxType::EquipmentAdvanced : EGachaBoxType::EquipmentNormal;
		const FGachaBoxVisualRow* BoxVisual = TableManager->GetGachaBoxVisual(BoxType);
		const FGeometry HeroGeometry = GachaImage->GetCachedGeometry();
		const FGeometry ViewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
		if (BoxVisual && BoxVisual->LayerSize.X > 0.f && HeroGeometry.GetLocalSize().X > 0.f && ViewportGeometry.GetLocalSize().X > 0.f)
		{
			const FVector2D HeroCenter = HeroGeometry.LocalToAbsolute(HeroGeometry.GetLocalSize() * .5f);
			const FVector2D HeroRight = HeroGeometry.LocalToAbsolute(FVector2D(HeroGeometry.GetLocalSize().X, HeroGeometry.GetLocalSize().Y * .5f));
			const FVector2D LaunchPos = ViewportGeometry.AbsoluteToLocal(HeroCenter);
			const float LaunchWidth = (ViewportGeometry.AbsoluteToLocal(HeroRight) - LaunchPos).Size() * 2.f;
			if (LaunchWidth > 0.f) { RevealPresentation->SetLaunchFrom(LaunchPos, LaunchWidth / BoxVisual->LayerSize.X); }
		}
	}
	RevealPresentation->SetupPullEquipment(Results, SourceTicket, EmployeeId, Award);
	bResultsRevealPending = false;
}

void UEmployeeEquipmentWorkspaceWidget::HandleRevealClosed()
{
	RevealPresentation = nullptr;
}

void UEmployeeEquipmentWorkspaceWidget::HandleResultClicked(UObject* Item)
{
	if (const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Item))
	{
		const bool bNewSelection = SelectedInstanceId != Entry->InstanceID;
		bReturnToResults = true;
		ShowDetail(Entry->InstanceID);
		if (bNewSelection && Dialog)
		{
			if (UImage* SelectedIcon = Cast<UImage>(Dialog->GetWidgetFromName(TEXT("SelectedIconImage"))))
				CGEquipmentIcon::TriggerPulse(Cast<UMaterialInstanceDynamic>(SelectedIcon->GetBrush().GetResourceObject()));
		}
	}
}

#undef LOCTEXT_NAMESPACE

UGachaRevealPresentationWidget* UEmployeeEquipmentWorkspaceWidget::GetRevealPresentation() const
{
	return RevealPresentation;
}
