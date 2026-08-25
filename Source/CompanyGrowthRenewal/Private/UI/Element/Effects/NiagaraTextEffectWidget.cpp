// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Effects/NiagaraTextEffectWidget.h"

#include "CommonTextBlock.h"
#include "NiagaraSystemWidget.h"
#include "NiagaraUIComponent.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Engine/GameInstance.h"

void UNiagaraTextEffectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Niagara 시스템 비활성화 (Visibility 대신 Deactivate 사용)
	for (UNiagaraSystemWidget* Widget : GetAllNiagaraWidgets())
	{
		if (Widget)
		{
			Widget->DeactivateSystem();
		}
	}

	// 텍스트 초기 상태 설정 (보이지 않게)
	if (EffectText)
	{
		EffectText->SetRenderOpacity(0.0f);
		EffectText->SetRenderScale(FVector2D(StartScale, StartScale));
		EffectText->SetRenderTranslation(FVector2D(0.0f, StartOffsetY));
	}
}

void UNiagaraTextEffectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsAnimating) return;

	ElapsedTime += InDeltaTime;

	// 텍스트 딜레이 단계 (0 ~ TextDelayTime): 텍스트 안 보임
	if (ElapsedTime < TextDelayTime)
	{
		// 텍스트 숨김 상태 유지
		if (EffectText)
		{
			EffectText->SetRenderOpacity(0.0f);
		}
	}
	// 팝업 애니메이션 단계 (TextDelayTime ~ TextDelayTime + PopAnimDuration)
	else if (ElapsedTime <= TextDelayTime + PopAnimDuration)
	{
		float Alpha = FMath::Clamp((ElapsedTime - TextDelayTime) / PopAnimDuration, 0.0f, 1.0f);
		UpdatePopAnimation(Alpha);
	}
	// 페이드 아웃 단계 (FadeOutStartTime ~ FadeOutEndTime)
	else if (ElapsedTime >= FadeOutStartTime && ElapsedTime <= FadeOutEndTime)
	{
		float FadeOutDuration = FadeOutEndTime - FadeOutStartTime;
		float FadeAlpha = FMath::Clamp((ElapsedTime - FadeOutStartTime) / FadeOutDuration, 0.0f, 1.0f);
		float Opacity = FMath::Lerp(1.0f, 0.0f, EaseOutCubic(FadeAlpha));

		if (EffectText)
		{
			EffectText->SetRenderOpacity(Opacity);
		}
	}
	// 페이드 아웃 완료 후: 투명 유지
	else if (ElapsedTime > FadeOutEndTime)
	{
		if (EffectText)
		{
			EffectText->SetRenderOpacity(0.0f);
		}
	}

	// 전체 효과 종료 체크
	if (ElapsedTime >= EffectDuration)
	{
		bIsAnimating = false;

		// 제거 전에 완료 델리게이트 브로드캐스트
		OnEffectFinished.Broadcast();

		RemoveFromParent();
	}
}

void UNiagaraTextEffectWidget::SetStyle(ETextEffectStyle Style)
{
	// 디버그: 모든 스타일 바인딩 상태 확인
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] SetStyle called. Style: %d"), static_cast<int32>(Style));
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Style1 bound: %s"), NiagaraEffect_Style1 ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Style2 bound: %s"), NiagaraEffect_Style2 ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Style3 bound: %s"), NiagaraEffect_Style3 ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Style4 bound: %s"), NiagaraEffect_Style4 ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Style5 bound: %s"), NiagaraEffect_Style5 ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Style6 bound: %s"), NiagaraEffect_Style6 ? TEXT("YES") : TEXT("NO"));

	// 선택된 스타일 저장 (Visibility 조작 없이)
	TArray<UNiagaraSystemWidget*> AllWidgets = GetAllNiagaraWidgets();
	int32 StyleIndex = static_cast<int32>(Style);

	if (AllWidgets.IsValidIndex(StyleIndex))
	{
		ActiveNiagaraWidget = AllWidgets[StyleIndex];
		UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] ActiveNiagaraWidget set: %s"),
			ActiveNiagaraWidget ? TEXT("YES") : TEXT("NO"));
	}
}

void UNiagaraTextEffectWidget::PlayEffectAndRemove()
{
	UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] PlayEffectAndRemove called"));

	// 활성화된 Niagara 효과 재생
	if (ActiveNiagaraWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NiagaraTextEffect] Activating Niagara System..."));
		ActiveNiagaraWidget->DeactivateSystem();
		ActiveNiagaraWidget->ActivateSystem(true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[NiagaraTextEffect] ActiveNiagaraWidget is NULL in PlayEffectAndRemove!"));
	}

	// 텍스트 애니메이션 시작
	ElapsedTime = 0.0f;
	bIsAnimating = true;

	// 사운드 — BP에서 SoundID 색깔별 override 가능 (NAME_None 이면 무음)
	if (SoundID != NAME_None)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SoundMgr->PlaySound(SoundID);
			}
		}
	}
}

void UNiagaraTextEffectWidget::SetEffectText(const FText& InText)
{
	if (EffectText)
	{
		EffectText->SetText(InText);
	}
}

TArray<UNiagaraSystemWidget*> UNiagaraTextEffectWidget::GetAllNiagaraWidgets() const
{
	return {
		NiagaraEffect_Style1,
		NiagaraEffect_Style2,
		NiagaraEffect_Style3,
		NiagaraEffect_Style4,
		NiagaraEffect_Style5,
		NiagaraEffect_Style6
	};
}

void UNiagaraTextEffectWidget::UpdatePopAnimation(float Alpha)
{
	if (!EffectText) return;

	// 스케일: 오버슈트 효과 (0.5 → 1.2 → 1.0)
	float CurrentScale;
	if (Alpha < 0.6f)
	{
		// 0 ~ 0.6: StartScale → OvershootScale
		float Phase1Alpha = Alpha / 0.6f;
		CurrentScale = FMath::Lerp(StartScale, OvershootScale, EaseOutCubic(Phase1Alpha));
	}
	else
	{
		// 0.6 ~ 1.0: OvershootScale → EndScale
		float Phase2Alpha = (Alpha - 0.6f) / 0.4f;
		CurrentScale = FMath::Lerp(OvershootScale, EndScale, EaseOutCubic(Phase2Alpha));
	}

	// Y 위치: 아래에서 위로 떠오름 (30 → 0)
	float EasedAlpha = EaseOutCubic(Alpha);
	float CurrentY = FMath::Lerp(StartOffsetY, 0.0f, EasedAlpha);

	// 투명도: 페이드 인 (0 → 1, 빠르게)
	float OpacityAlpha = FMath::Clamp(Alpha * 2.5f, 0.0f, 1.0f);
	float CurrentOpacity = FMath::Lerp(0.0f, 1.0f, OpacityAlpha);

	// 적용
	EffectText->SetRenderScale(FVector2D(CurrentScale, CurrentScale));
	EffectText->SetRenderTranslation(FVector2D(0.0f, CurrentY));
	EffectText->SetRenderOpacity(CurrentOpacity);
}

float UNiagaraTextEffectWidget::EaseOutCubic(float Alpha) const
{
	return 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
}

float UNiagaraTextEffectWidget::EaseOutBack(float Alpha) const
{
	const float C1 = 1.70158f;
	const float C3 = C1 + 1.0f;
	return 1.0f + C3 * FMath::Pow(Alpha - 1.0f, 3.0f) + C1 * FMath::Pow(Alpha - 1.0f, 2.0f);
}
