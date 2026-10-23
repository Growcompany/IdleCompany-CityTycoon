#include "UI/Element/Employee/EmployeeBadgeCardWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Data/EntityCardData.h"
#include "Engine/Texture2D.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Enum/LootBoxRarity.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"

namespace EmployeeBadgePortraitCache
{
	struct FEntry
	{
		TWeakObjectPtr<UTexture2D> Texture;
		FDateTime ModificationTime;
		int64 FileSize = 0;
		FVector2D TargetSize = FVector2D::ZeroVector;
	};

	TMap<int32, FEntry>& Entries()
	{
		static TMap<int32, FEntry> Map;
		return Map;
	}
}

UTexture2D* UEmployeeBadgeCardWidget::GetCachedPortrait(int32 EmployeeId, const FVector2D& TargetSize)
{
	using namespace EmployeeBadgePortraitCache;
	const FFileStatData Stat = IFileManager::Get().GetStatData(*AOfficeworker::GetPortraitFilePathForEmployee(EmployeeId));
	if (!Stat.bIsValid)
	{
		Entries().Remove(EmployeeId);
		return nullptr;
	}
	if (const FEntry* Found = Entries().Find(EmployeeId))
	{
		UTexture2D* Cached = Found->Texture.Get();
		if (Cached && Found->ModificationTime == Stat.ModificationTime && Found->FileSize == Stat.FileSize
			&& Found->TargetSize.Equals(TargetSize))
		{
			return Cached;
		}
	}

	UTexture2D* Loaded = LoadPortraitTexture(EmployeeId, TargetSize);
	if (Loaded)
	{
		FEntry& Entry = Entries().FindOrAdd(EmployeeId);
		Entry.Texture = Loaded;
		Entry.ModificationTime = Stat.ModificationTime;
		Entry.FileSize = Stat.FileSize;
		Entry.TargetSize = TargetSize;
	}
	else
	{
		Entries().Remove(EmployeeId);
	}
	return Loaded;
}

void UEmployeeBadgeCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (NameText)
	{
		AuthoredNameColor = NameText->GetColorAndOpacity();
	}
}

void UEmployeeBadgeCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 베이스 클릭 핸들러는 SetEmployeeInfo 가 채운 데이터로 쏘는데 사원증은 그 경로(매번 초상 디코드)를 타지 않는다.
	// 같은 델리게이트를 자기 ID 로 쏜다 — 해제는 베이스 NativeDestruct 의 RemoveAll(this) 가 함께 한다
	OnClicked().RemoveAll(this);
	OnClicked().AddUObject(this, &ThisClass::HandleBadgeClicked);
}

void UEmployeeBadgeCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// 베이스 경로는 「Lv%d」·★ 칩을 쓰고 초상을 매번 디코드한다 — 목록에 넣더라도 사원증 표기를 지킨다
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	if (const UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		SetBadge(CardData->EmployeeInfo, false, false);
	}
	SetBadgeSelected(IsListItemSelected());
}

void UEmployeeBadgeCardWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	// 베이스는 선택 펀치(1.06배)를 건다 — 원 크기 초과 스케일은 이웃 카드·클립에 걸려 쓰지 않는다
	SetBadgeSelected(bIsSelected);
}

void UEmployeeBadgeCardWidget::SetBadge(const FEmployeeInstance& Employee, bool bCanLevelUp, bool bOnBench)
{
	BadgeEmployeeId = Employee.EmployeeID;

	if (NameText)
	{
		NameText->SetText(FText::FromString(Employee.EmployeeName));
		NameText->SetColorAndOpacity(bOnBench ? FSlateColor(BenchNameColor) : AuthoredNameColor);
	}
	if (LevelText)
	{
		LevelText->SetText(FText::AsNumber(Employee.Level));
	}
	if (StarsText)
	{
		StarsText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LevelUpMark)
	{
		LevelUpMark->SetVisibility(bCanLevelUp && !bOnBench ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (BenchLabel)
	{
		BenchLabel->SetVisibility(bOnBench ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyPortrait(bOnBench);
	ApplyTeam(Employee, bOnBench);
	ApplyPotential(Employee);
}

void UEmployeeBadgeCardWidget::SetBadgeSelected(bool bNowSelected)
{
	bBadgeSelected = bNowSelected;
	if (SelectionRing)
	{
		SelectionRing->SetVisibility(bNowSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEmployeeBadgeCardWidget::ApplyPortrait(bool bOnBench)
{
	if (!PortraitImage)
	{
		return;
	}
	UTexture2D* Portrait = GetCachedPortrait(BadgeEmployeeId, PortraitDisplaySize);
	if (!Portrait)
	{
		// 초상은 런타임 캡처라 아직 없을 수 있다 — 빈 사진창으로 둔다
		PortraitImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	UObject* Resource = Portrait;
	if (bOnBench)
	{
		if (!BenchPortraitMID)
		{
			if (UMaterialInterface* BenchMaterial = BenchPortraitMaterial.LoadSynchronous())
			{
				BenchPortraitMID = UMaterialInstanceDynamic::Create(BenchMaterial, this);
			}
			else if (!BenchPortraitMaterial.IsNull())
			{
				UE_LOG(LogTemp, Warning, TEXT("[EmployeeBadgeCard] BenchPortraitMaterial 로드 실패: %s"), *BenchPortraitMaterial.ToString());
			}
		}
		if (BenchPortraitMID)
		{
			BenchPortraitMID->SetTextureParameterValue(TEXT("Icon"), Portrait);
			Resource = BenchPortraitMID;
		}
	}

	FSlateBrush Brush = PortraitImage->GetBrush();
	Brush.SetResourceObject(Resource);
	Brush.ImageSize = FVector2D(Portrait->GetSizeX(), Portrait->GetSizeY());
	PortraitImage->SetBrush(Brush);
	PortraitImage->SetColorAndOpacity(bOnBench ? BenchPortraitTint : FLinearColor::White);
	PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UEmployeeBadgeCardWidget::ApplyTeam(const FEmployeeInstance& Employee, bool bOnBench)
{
	const int32 DisciplineSlot = GetPrimaryDisciplineSlot(Employee);
	const int32 IconSlot = GetDisciplineIconSlot(DisciplineSlot, Employee.DisciplinePoints.Num());
	UTexture2D* Glyph = TeamGlyphIcons.IsValidIndex(IconSlot) ? TeamGlyphIcons[IconSlot].LoadSynchronous() : nullptr;
	if (DisciplineSlot != INDEX_NONE && !Glyph)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeBadgeCard] 직능 %d 글리프 없음 — UIE_EmployeeBadgeCard CDO TeamGlyphIcons 확인"), DisciplineSlot);
	}
	if (TeamGlyph && Glyph)
	{
		TeamGlyph->SetBrushResourceObject(Glyph);
	}
	if (TeamDisc)
	{
		TeamDisc->SetVisibility(Glyph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		TeamDisc->SetRenderOpacity(bOnBench ? BenchTeamDiscOpacity : 1.f);
	}
}

void UEmployeeBadgeCardWidget::ApplyPotential(const FEmployeeInstance& Employee)
{
	const ELootBoxRarity Rarity = Employee.PotentialAbility.CurrentRarity;
	const bool bShow = static_cast<uint8>(Rarity) > static_cast<uint8>(ELootBoxRarity::Common);
	const FLinearColor Color = EmployeeRarityDisplay::VividColor(Rarity);
	const ESlateVisibility GlowVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (PotentialGlow)
	{
		PotentialGlow->SetColorAndOpacity(FLinearColor(Color.R, Color.G, Color.B, PotentialLineAlpha));
		PotentialGlow->SetVisibility(GlowVisibility);
	}
	if (PotentialHaze)
	{
		PotentialHaze->SetColorAndOpacity(FLinearColor(Color.R, Color.G, Color.B, PotentialHazeAlpha));
		PotentialHaze->SetVisibility(GlowVisibility);
	}
}

void UEmployeeBadgeCardWidget::HandleBadgeClicked()
{
	if (BadgeEmployeeId != INDEX_NONE)
	{
		OnRosterCardClicked.Broadcast(BadgeEmployeeId);
	}
}
