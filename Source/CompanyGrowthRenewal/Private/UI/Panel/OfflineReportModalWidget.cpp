// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfflineReportModalWidget.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Enum/WidgetType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Table/ResourceInfo.h"
#include "UI/Element/Building/OfflineGainRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Panel/InGameLayerWidget.h"

namespace
{
	// 카운트업 — 축하가 착지한 뒤에 손실을 말하기 위한 기준 타이밍(설계 SOT §6)
	constexpr float HeroCountUpDelay = 0.25f;
	constexpr float HeroCountUpDuration = 0.6f;

	const FLinearColor OfflineLossInk(1.0f, 0.35f, 0.35f, 1.0f);
}

void UOfflineReportModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 데이터는 델리게이트로 수신 — 푸시(구독) 직후 Consume 이 1회 발화 → SetReportData
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SL = GI->GetSubsystem<USaveLoadManager>())
		{
			SL->OnOfflineGainsDetailed.AddUObject(this, &UOfflineReportModalWidget::SetReportData);
		}
	}

	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().AddUObject(this, &UOfflineReportModalWidget::HandleCollectAllClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UOfflineReportModalWidget::HandleCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UOfflineReportModalWidget::HandleBackgroundClicked);
	}
}

void UOfflineReportModalWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SL = GI->GetSubsystem<USaveLoadManager>())
		{
			SL->OnOfflineGainsDetailed.RemoveAll(this);
		}
	}

	if (CollectAllButton)
	{
		CollectAllButton->OnClicked().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UOfflineReportModalWidget::HandleCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UOfflineReportModalWidget::HandleBackgroundClicked);
	}

	Super::NativeDestruct();
}

void UOfflineReportModalWidget::SetReportData(float InTotalGained, float InOfflineSeconds,
	const TArray<FOfflineGainEntry>& InEntries, bool bInCapReached)
{
	TotalGained = InTotalGained;
	OfflineSeconds = InOfflineSeconds;
	bCapReached = bInCapReached;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// ===== 시간 라인 =====
	// OfflineSeconds 는 서버가 12h 로 자른 값이라 "12시간 만에 돌아오셨습니다" 가 거짓일 수 있다 → 캡 도달 시 교체
	if (TimeAwayText)
	{
		FString TimeStr;
		if (bCapReached)
		{
			TimeStr = TEXT("12시간 이상 자리를 비우셨습니다");
		}
		else
		{
			const int32 TotalMinutes = FMath::FloorToInt(OfflineSeconds / 60.0f);
			const int32 Hours = TotalMinutes / 60;
			const int32 Minutes = TotalMinutes % 60;
			if (Hours > 0 && Minutes > 0)
			{
				TimeStr = FString::Printf(TEXT("%d시간 %d분 만에 돌아오셨습니다"), Hours, Minutes);
			}
			else if (Hours > 0)
			{
				TimeStr = FString::Printf(TEXT("%d시간 만에 돌아오셨습니다"), Hours);
			}
			else
			{
				TimeStr = FString::Printf(TEXT("%d분 만에 돌아오셨습니다"), FMath::Max(1, Minutes));
			}
		}
		TimeAwayText->SetText(FText::FromString(TimeStr));
	}

	// ===== 히어로 =====
	const bool bZeroGain = (TotalGained <= 0.0f);

	if (HeroCaptionText)
	{
		// 이 돈은 지갑이 아니라 각 건물 금고로 들어간다 — "획득"이라 쓰면 지갑을 확인한 유저가 배신감을 느낀다
		HeroCaptionText->SetText(FText::FromString(bZeroGain
			? TEXT("금고가 가득 차 이번에는 적립되지 않았습니다")
			: TEXT("각 건물 금고에 적립되었습니다")));
	}

	if (TotalGainText && TableMgr)
	{
		if (bZeroGain)
		{
			// 0원을 축하색(그린)으로 칠하면 거짓 — 뮤트로 둔다
			TotalGainText->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(0xB9, 0xC2, 0xCF))));
		}
		else
		{
			bool bResOk = false;
			const FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bResOk);
			if (bResOk)
			{
				TotalGainText->SetColorAndOpacity(FSlateColor(ResInfo.UIColor));
			}
		}
	}

	// Money 아이콘도 DT 경유 (코드에 경로/색을 굽지 않는다)
	if (MoneyIcon && TableMgr)
	{
		bool bIconOk = false;
		const FResourceInfo ResInfo = TableMgr->GetResourceInfo(EResourceType::Money, bIconOk);
		if (bIconOk && !ResInfo.Icon.IsNull())
		{
			if (UTexture2D* MoneyTex = ResInfo.Icon.LoadSynchronous())
			{
				MoneyIcon->SetBrushFromTexture(MoneyTex);
			}
		}
	}

	// 적립 0 이면 축하 연출 전면 오프 — 카운트업 없이 최종값 즉시 표시
	bCountUpActive = !bZeroGain;
	CountUpElapsed = 0.0f;
	ApplyTotalText(bZeroGain ? 0.0f : 0.0f);

	// ===== 빌딩별 행 =====
	float TotalLoss = 0.0f;

	// 최대 수익원이 첫 줄 = 축하 우선. 손실 행을 하단에 모으지 않는다(문제 섹션처럼 읽혀 잔소리가 됨)
	TArray<FOfflineGainEntry> SortedEntries = InEntries;
	SortedEntries.Sort([](const FOfflineGainEntry& A, const FOfflineGainEntry& B)
	{
		return A.ActualGain > B.ActualGain;
	});

	for (const FOfflineGainEntry& Entry : SortedEntries)
	{
		TotalLoss += Entry.LossByVault;
	}

	if (RowBox)
	{
		RowBox->ClearChildren();

		TSubclassOf<UUserWidget> RowClass = TableMgr
			? TableMgr->GetWidgetClass(EWidgetType::OfflineGainRow)
			: nullptr;

		if (!RowClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[OfflineReportModal] EWidgetType::OfflineGainRow 클래스 없음 — DT_WidgetClass 행 확인"));
		}
		else
		{
			for (const FOfflineGainEntry& Entry : SortedEntries)
			{
				if (UOfflineGainRowWidget* RowWidget = CreateWidget<UOfflineGainRowWidget>(this, RowClass))
				{
					RowWidget->SetRowData(Entry);
					// C++ 로 추가한 슬롯은 패딩 0 → 행이 딱 붙는다. 스펙 §3 간격 10 을 슬롯 하단 패딩으로.
					if (UVerticalBoxSlot* BoxSlot = RowBox->AddChildToVerticalBox(RowWidget))
					{
						BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
					}
				}
			}
		}
	}

	// ===== 손실 밴드 (안내만 — 강화는 각 빌딩에서. 딥링크/CTA 없음) =====
	const bool bHasLoss = TotalLoss > 0.0f;
	if (LossBand)
	{
		LossBand->SetVisibility(bHasLoss ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LossBandAmountText && bHasLoss)
	{
		LossBandAmountText->SetText(UGlobalUtilFunctions::AbbreviateNumber(
			static_cast<int64>(TotalLoss), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
		LossBandAmountText->SetColorAndOpacity(FSlateColor(OfflineLossInk));
	}

	if (CapNote)
	{
		CapNote->SetVisibility(bCapReached ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UOfflineReportModalWidget::ApplyTotalText(float Value)
{
	if (!TotalGainText)
	{
		return;
	}

	const FText Abbrev = UGlobalUtilFunctions::AbbreviateNumber(
		static_cast<int64>(Value), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor);

	// 적립 0 은 "+0" 이 아니라 "0" — 더하기 기호가 붙으면 벌었다는 인상이 남는다
	TotalGainText->SetText(TotalGained <= 0.0f
		? Abbrev
		: FText::FromString(FString::Printf(TEXT("+%s"), *Abbrev.ToString())));
}

void UOfflineReportModalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bCountUpActive)
	{
		return;
	}

	CountUpElapsed += InDeltaTime;
	const float Elapsed = CountUpElapsed - HeroCountUpDelay;
	if (Elapsed <= 0.0f)
	{
		return;
	}

	const float Alpha = FMath::Clamp(Elapsed / HeroCountUpDuration, 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);  // CubicOut
	ApplyTotalText(TotalGained * Eased);

	if (Alpha >= 1.0f)
	{
		bCountUpActive = false;
	}
}

void UOfflineReportModalWidget::HandleCollectAllClicked()
{
	// 닫기 먼저 → 수거 나중. 스택 잔류 방지 + 코인 플라이아웃이 모달에 가리지 않게 (순서 고정)
	DeactivateWidget();

	// 수거+코인 플라이아웃은 InGameLayer(HUD) 소유 — UIManager 가 추적하는 단일 인스턴스를 직접 호출.
	// (델리게이트 브로드캐스트는 InGameLayer 이중구독 시 2회 수거되어 두 번째가 "빈 금고" 토스트를 띄웠음)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
			{
				InGame->ExecuteAllVaultCollection();
			}
		}
	}
}

void UOfflineReportModalWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

void UOfflineReportModalWidget::HandleCloseButtonClicked()
{
	HandleCloseClicked();
}

void UOfflineReportModalWidget::HandleBackgroundClicked()
{
	HandleCloseClicked();
}

void UOfflineReportModalWidget::NativeOnDeactivated()
{
	// UI 모드 복원 (호출자 책임 쌍). 강화 딥링크는 push 후 다시 GoToUIMode 하므로 순서상 안전
	if (UWorld* WorldPtr = GetWorld())
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(WorldPtr->GetFirstPlayerController()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}

	Super::NativeOnDeactivated();
}
