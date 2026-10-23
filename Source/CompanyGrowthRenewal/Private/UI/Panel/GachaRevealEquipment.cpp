#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Data/EmployeeEquipmentData.h"

void UGachaRevealPresentationWidget::SetupPullEquipment(const TArray<FEmployeeEquipmentInstance>& Results, EItemType SourceTicket, int32 EmployeeId,
	const FGachaRevealAwardContext& Award)
{
	if (RejectIfOpen(TEXT("장비")))
	{
		return;
	}
	// 장비 결과 연출은 작업 공간이 따로 추적한다 ― 튜토리얼 공용 등록에서 빠진다
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			Missions->UnregisterGachaReveal(this);
		}
	}
	AwardContext = Award;
	RevealEmployeeId = EmployeeId;
	EquipmentInstanceIds.Reset();
	bSkinDuplicates.Reset();
	UGameInstance* GI = GetGameInstance();
	const UEmployeeEquipmentManagerSubsystem* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const UTableManagerSubsystem* Tables = ResolveTables();
	const FEmployeeEquipmentEconomyRow* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
	TArray<FGachaRevealEntry> NewEntries;
	TArray<FGachaRevealCardData> NewCards;
	TArray<FText> NewMeta;
	int32 SourceIndex = 0;
	for (const FEmployeeEquipmentInstance& Item : Results)
	{
		const int32 AwardIndex = SourceIndex++;
		const FEmployeeEquipmentDefinitionRow* Definition = Tables ? Tables->FindEmployeeEquipmentDefinition(Item.DefinitionId) : nullptr;
		if (!Definition)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 장비 정의 없음: %s"), *Item.DefinitionId.ToString());
			continue;
		}
		FGachaRevealEntry& Entry = NewEntries.AddDefaulted_GetRef();
		Entry.PullIndex = NewEntries.Num() - 1;
		Entry.Grade = FMath::Clamp(static_cast<int32>(Definition->Grade), 0, GachaRevealModel::MaxGrade);
		Entry.SlotKey = static_cast<int32>(Definition->EquipmentSlot);
		Entry.bNew = Award.bOwnedBefore.IsValidIndex(AwardIndex) && !Award.bOwnedBefore[AwardIndex];
		Entry.bEmployeeKnown = EmployeeId != INDEX_NONE && Equipment != nullptr;
		if (Entry.bEmployeeKnown)
		{
			if (const FEmployeeEquipmentInstance* Worn = Equipment->GetEquippedInstance(EmployeeId, Definition->EquipmentSlot))
			{
				if (const FEmployeeEquipmentDefinitionRow* WornDef = Tables->FindEmployeeEquipmentDefinition(Worn->DefinitionId))
				{
					Entry.EquippedGrade = static_cast<int32>(WornDef->Grade);
				}
			}
		}
		EquipmentInstanceIds.Add(Item.InstanceId);

		FGachaRevealCardData& Card = NewCards.AddDefaulted_GetRef();
		Card.Kind = EGachaRevealKind::Equipment;
		Card.Grade = Entry.Grade;
		Card.Name = Definition->DisplayName;
		Card.Icon = Definition->Icon;
		Card.bNew = Entry.bNew;
		if (Economy)
		{
			const auto Effects = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(*Definition, Item.EnhanceLevel, Tables->GetEmployeeEquipmentEffects(), *Economy);
			if (Effects.bValid)
			{
				// 카드 한 줄 = 첫 기본 효과(이름순, 표시 이름 있는 것), 고유 효과 = 정의 순서 첫 것 ― 시안 item.main·item.unique
				TArray<FName> EffectIds;
				Effects.BasicValues.GetKeys(EffectIds);
				EffectIds.Sort(FNameLexicalLess());
				for (FName EffectId : EffectIds)
				{
					const FEmployeeEquipmentEffectRow* Effect = Tables->FindEmployeeEquipmentEffect(EffectId);
					if (Effect && !Effect->DisplayName.IsEmpty())
					{
						Card.EffectLine = FText::FromString(Effect->DisplayName.ToString() + TEXT(" ") + CGEquipmentDetail::FormatEffectValue(*Effect, Effects.BasicValues[EffectId]));
						break;
					}
				}
				for (FName EffectId : Definition->UniqueEffectIds)
				{
					const auto* Unique = Effects.UniqueEffects.Find(EffectId);
					if (Unique && !Unique->DisplayName.IsEmpty())
					{
						Card.UniqueLine = Unique->DisplayName;
						break;
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 장비 효과 계산 실패: %s"), *Item.DefinitionId.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaReveal] 장비 경제 표(Default) 없음 ― 효과 줄 비움"));
		}
		const FEmployeeEquipmentSlotRow* SlotRow = Tables->FindEmployeeEquipmentSlot(Definition->EquipmentSlot);
		const FText Part = SlotRow ? SlotRow->DisplayName : FText::GetEmpty();
		const FText Noun = NSLOCTEXT("GachaReveal", "EquipmentNoun", "장비");
		NewMeta.Add(Part.IsEmpty() ? Noun : FText::Format(NSLOCTEXT("GachaReveal", "MetaLine", "{0} · {1}"), Part, Noun));
	}

	EGachaBoxType Box = EGachaBoxType::EquipmentNormal;
	FText Tier = NSLOCTEXT("GachaReveal", "TierNormal", "일반");
	if (SourceTicket == EItemType::EmployeeEquipmentTicketAdvanced)
	{
		Box = EGachaBoxType::EquipmentAdvanced;
		Tier = NSLOCTEXT("GachaReveal", "TierAdvanced", "고급");
	}
	else if (SourceTicket == EItemType::EmployeeEquipmentTicketPremium)
	{
		Box = EGachaBoxType::EquipmentPremium;
		Tier = NSLOCTEXT("GachaReveal", "TierPremium", "최고급");
	}
	const FText Title = FText::Format(NSLOCTEXT("GachaReveal", "Title", "{0} {1} 뽑기 · {2}회"), Tier,
		NSLOCTEXT("GachaReveal", "EquipmentNoun", "장비"), FText::AsNumber(NewEntries.Num()));
	BeginReveal(EGachaRevealKind::Equipment, Box, Title, NewEntries, NewCards, NewMeta);
}
