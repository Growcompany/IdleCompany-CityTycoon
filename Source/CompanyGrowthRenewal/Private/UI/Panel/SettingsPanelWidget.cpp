// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/SettingsPanelWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

#include "Data/GameAudioSettings.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Settings/SettingSegmentRowWidget.h"
#include "UI/Element/Settings/SettingSliderRowWidget.h"
#include "UI/Element/Settings/SettingToggleRowWidget.h"

void USettingsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &USettingsPanelWidget::OnBackgroundClicked);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &USettingsPanelWidget::OnCloseClicked);
	}

	InitTabs();

	// 사운드 행 → 매니저 배선 (부품은 매니저를 모르므로 여기서 카테고리를 캡처)
	if (MasterRow)
	{
		MasterRow->OnValueChangedDelegate.AddUObject(this, &USettingsPanelWidget::HandleVolumeRowChanged, ESoundCategory::Master);
		MasterRow->OnMuteToggledDelegate.AddUObject(this, &USettingsPanelWidget::HandleMuteRowToggled, ESoundCategory::Master);
		MasterRow->OnSliderReleased.AddUObject(this, &USettingsPanelWidget::HandleSliderReleased);
	}
	if (MusicRow)
	{
		MusicRow->OnValueChangedDelegate.AddUObject(this, &USettingsPanelWidget::HandleVolumeRowChanged, ESoundCategory::Music);
		MusicRow->OnMuteToggledDelegate.AddUObject(this, &USettingsPanelWidget::HandleMuteRowToggled, ESoundCategory::Music);
		MusicRow->OnSliderReleased.AddUObject(this, &USettingsPanelWidget::HandleSliderReleased);
	}
	if (SFXRow)
	{
		SFXRow->OnValueChangedDelegate.AddUObject(this, &USettingsPanelWidget::HandleVolumeRowChanged, ESoundCategory::SFX);
		SFXRow->OnMuteToggledDelegate.AddUObject(this, &USettingsPanelWidget::HandleMuteRowToggled, ESoundCategory::SFX);
		SFXRow->OnSliderReleased.AddUObject(this, &USettingsPanelWidget::HandleSliderReleased);
	}
	if (UIRow)
	{
		UIRow->OnValueChangedDelegate.AddUObject(this, &USettingsPanelWidget::HandleVolumeRowChanged, ESoundCategory::UI);
		UIRow->OnMuteToggledDelegate.AddUObject(this, &USettingsPanelWidget::HandleMuteRowToggled, ESoundCategory::UI);
		UIRow->OnSliderReleased.AddUObject(this, &USettingsPanelWidget::HandleSliderReleased);
	}

	// 그래픽 행 → 매니저 배선 (AddLambda는 RemoveAll(this)로 안 지워져 AddUObject 사용)
	if (QualityRow) QualityRow->OnSegmentChanged.AddUObject(this, &USettingsPanelWidget::HandleQualityChanged);
	if (FpsRow) FpsRow->OnSegmentChanged.AddUObject(this, &USettingsPanelWidget::HandleFpsChanged);
	if (ReduceMotionRow) ReduceMotionRow->OnToggleChanged.AddUObject(this, &USettingsPanelWidget::HandleReduceMotionToggled);

	// 계정 버튼 (CommonButtonBase 계열 = 네이티브 OnClicked())
	if (NickChangeBtn) NickChangeBtn->OnClicked().AddUObject(this, &USettingsPanelWidget::HandleNickChangeClicked);
	if (GoogleLinkBtn) GoogleLinkBtn->OnClicked().AddUObject(this, &USettingsPanelWidget::HandleGoogleLinkClicked);
	if (LogoutBtn) LogoutBtn->OnClicked().AddUObject(this, &USettingsPanelWidget::HandleLogoutClicked);
	if (ResetBtn) ResetBtn->OnClicked().AddUObject(this, &USettingsPanelWidget::HandleResetClicked);

	// 버전 표시 (DefaultGame.ini ProjectVersion)
	if (VersionText)
	{
		FString Version;
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
		VersionText->SetText(FText::FromString(FString::Printf(TEXT("v%s"), *Version)));
	}
}

void USettingsPanelWidget::NativeDestruct()
{
	if (BackgroundBtn) BackgroundBtn->OnClicked.RemoveDynamic(this, &USettingsPanelWidget::OnBackgroundClicked);
	if (UIE_CloseButton) UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &USettingsPanelWidget::OnCloseClicked);
	if (TabGroup) TabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);

	if (MasterRow) { MasterRow->OnValueChangedDelegate.RemoveAll(this); MasterRow->OnMuteToggledDelegate.RemoveAll(this); MasterRow->OnSliderReleased.RemoveAll(this); }
	if (MusicRow) { MusicRow->OnValueChangedDelegate.RemoveAll(this); MusicRow->OnMuteToggledDelegate.RemoveAll(this); MusicRow->OnSliderReleased.RemoveAll(this); }
	if (SFXRow) { SFXRow->OnValueChangedDelegate.RemoveAll(this); SFXRow->OnMuteToggledDelegate.RemoveAll(this); SFXRow->OnSliderReleased.RemoveAll(this); }
	if (UIRow) { UIRow->OnValueChangedDelegate.RemoveAll(this); UIRow->OnMuteToggledDelegate.RemoveAll(this); UIRow->OnSliderReleased.RemoveAll(this); }
	if (QualityRow) QualityRow->OnSegmentChanged.RemoveAll(this);
	if (FpsRow) FpsRow->OnSegmentChanged.RemoveAll(this);
	if (ReduceMotionRow) ReduceMotionRow->OnToggleChanged.RemoveAll(this);

	if (NickChangeBtn) NickChangeBtn->OnClicked().RemoveAll(this);
	if (GoogleLinkBtn) GoogleLinkBtn->OnClicked().RemoveAll(this);
	if (LogoutBtn) LogoutBtn->OnClicked().RemoveAll(this);
	if (ResetBtn) ResetBtn->OnClicked().RemoveAll(this);

	Super::NativeDestruct();
}

void USettingsPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bRefreshing = true;
	RefreshSoundTab();
	RefreshGraphicsTab();
	RefreshAccountTab();
	bRefreshing = false;
}

void USettingsPanelWidget::NativeOnDeactivated()
{
	// 변경분 디스크 저장은 닫힐 때 1회 (드래그마다 쓰기 금지)
	if (bAudioDirty)
	{
		if (USoundManagerSubsystem* SoundMgr = GetSoundMgr()) SoundMgr->SaveAudioSettings();
		bAudioDirty = false;
	}
	if (bSettingsDirty)
	{
		if (USettingsManagerSubsystem* SettingsMgr = GetSettingsMgr()) SettingsMgr->SaveSettings();
		bSettingsDirty = false;
	}

	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
	{
		PC->GoToNormalMode();
	}

	Super::NativeOnDeactivated();
}

void USettingsPanelWidget::InitTabs()
{
	TabGroup = NewObject<UCommonButtonGroupBase>(this);
	TabGroup->SetSelectionRequired(true);

	UIconWithButtonWidget* Tabs[3] = { SoundTab, GraphicsTab, AccountTab };
	for (UIconWithButtonWidget* Tab : Tabs)
	{
		if (Tab)
		{
			TabGroup->AddWidget(Tab);
			Tab->SetIsSelectable(true);
		}
	}
	TabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &USettingsPanelWidget::HandleTabChanged);
	TabGroup->SelectButtonAtIndex(0);
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
	}
}

void USettingsPanelWidget::HandleTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}
	if (ButtonIndex == 2)
	{
		RefreshAccountTab();
	}
}

void USettingsPanelWidget::RefreshSoundTab()
{
	USoundManagerSubsystem* SoundMgr = GetSoundMgr();
	if (!SoundMgr)
	{
		return;
	}
	const FGameAudioSettings Audio = SoundMgr->GetAudioSettings();
	if (MasterRow) MasterRow->ConfigureSlider(FText::FromString(TEXT("마스터")), Audio.MasterVolume, Audio.bMuted);
	if (MusicRow)  MusicRow->ConfigureSlider(FText::FromString(TEXT("배경음악")), Audio.MusicVolume, Audio.bMusicMuted);
	if (SFXRow)    SFXRow->ConfigureSlider(FText::FromString(TEXT("효과음")), Audio.SFXVolume, Audio.bSFXMuted);
	if (UIRow)     UIRow->ConfigureSlider(FText::FromString(TEXT("UI음")), Audio.UIVolume, Audio.bUIMuted);
	ApplyMasterDim();
}

void USettingsPanelWidget::HandleVolumeRowChanged(float NewValue01, ESoundCategory Category)
{
	if (bRefreshing)
	{
		return;
	}
	if (USoundManagerSubsystem* SoundMgr = GetSoundMgr())
	{
		if (Category == ESoundCategory::Master)
		{
			SoundMgr->SetMasterVolume(NewValue01);
		}
		else
		{
			SoundMgr->SetCategoryVolume(Category, NewValue01);
		}
		bAudioDirty = true;
	}
}

void USettingsPanelWidget::HandleMuteRowToggled(bool bNewMuted, ESoundCategory Category)
{
	if (bRefreshing)
	{
		return;
	}
	if (USoundManagerSubsystem* SoundMgr = GetSoundMgr())
	{
		if (Category == ESoundCategory::Master)
		{
			SoundMgr->SetMuted(bNewMuted);
			ApplyMasterDim();
		}
		else
		{
			SoundMgr->SetCategoryMuted(Category, bNewMuted);
		}
		bAudioDirty = true;
	}
}

void USettingsPanelWidget::HandleSliderReleased()
{
	// 효과음 볼륨을 조절하면서 바로 들어보는 체감 피드백
	if (USoundManagerSubsystem* SoundMgr = GetSoundMgr())
	{
		SoundMgr->PlayUISound(FGameplayTag::RequestGameplayTag(FName(TEXT("UI.Sound.Button.Click")), false));
	}
}

void USettingsPanelWidget::ApplyMasterDim()
{
	USoundManagerSubsystem* SoundMgr = GetSoundMgr();
	const bool bAllMuted = SoundMgr && SoundMgr->GetAudioSettings().bMuted;
	const float DimAlpha = bAllMuted ? 0.45f : 1.0f;
	if (MusicRow) MusicRow->SetRenderOpacity(DimAlpha);
	if (SFXRow) SFXRow->SetRenderOpacity(DimAlpha);
	if (UIRow) UIRow->SetRenderOpacity(DimAlpha);
}

void USettingsPanelWidget::RefreshGraphicsTab()
{
	USettingsManagerSubsystem* SettingsMgr = GetSettingsMgr();
	if (!SettingsMgr)
	{
		return;
	}

	if (QualityRow)
	{
		QualityRow->ConfigureSegments(FText::FromString(TEXT("품질")), FText::GetEmpty(),
			{ FText::FromString(TEXT("낮음")), FText::FromString(TEXT("중간")), FText::FromString(TEXT("높음")) },
			static_cast<int32>(SettingsMgr->GetQualityPreset()));
	}
	if (FpsRow)
	{
		const int32 Fps = SettingsMgr->GetFrameRateLimit();
		const int32 FpsIndex = (Fps >= 120) ? 2 : (Fps >= 60 ? 1 : 0);
		FpsRow->ConfigureSegments(FText::FromString(TEXT("최대 FPS")), FText::FromString(TEXT("120은 고주사율 기기에서만")),
			{ FText::FromString(TEXT("30")), FText::FromString(TEXT("60")), FText::FromString(TEXT("120")) }, FpsIndex);
	}
	if (ReduceMotionRow)
	{
		ReduceMotionRow->ConfigureToggle(FText::FromString(TEXT("연출 감소")), FText::FromString(TEXT("화면 흔들림을 줄입니다")),
			SettingsMgr->IsReduceMotionEnabled());
	}
}

void USettingsPanelWidget::HandleQualityChanged(int32 Index)
{
	if (bRefreshing)
	{
		return;
	}
	if (USettingsManagerSubsystem* Mgr = GetSettingsMgr())
	{
		Mgr->SetQualityPreset(static_cast<EGraphicsQualityPreset>(Index));
		bSettingsDirty = true;
	}
}

void USettingsPanelWidget::HandleFpsChanged(int32 Index)
{
	if (bRefreshing)
	{
		return;
	}
	static const int32 FpsOptions[3] = { 30, 60, 120 };
	if (USettingsManagerSubsystem* Mgr = GetSettingsMgr())
	{
		Mgr->SetFrameRateLimit(FpsOptions[FMath::Clamp(Index, 0, 2)]);
		bSettingsDirty = true;
	}
}

void USettingsPanelWidget::HandleReduceMotionToggled(bool bOn)
{
	if (bRefreshing)
	{
		return;
	}
	if (USettingsManagerSubsystem* Mgr = GetSettingsMgr())
	{
		Mgr->SetReduceMotion(bOn);
		bSettingsDirty = true;
	}
}

void USettingsPanelWidget::RefreshAccountTab()
{
	UPlayFabManagerSubsystem* PlayFabMgr = GetPlayFabMgr();
	const bool bLoggedIn = PlayFabMgr && PlayFabMgr->IsLoggedIn();

	if (StatusChipText)
	{
		FString Status = TEXT("오프라인");
		if (bLoggedIn)
		{
			Status = (PlayFabMgr->GetUserInfo().LoginType == EPlayFabLoginType::Google) ? TEXT("구글") : TEXT("게스트");
		}
		StatusChipText->SetText(FText::FromString(Status));
	}
	if (NickNameText)
	{
		NickNameText->SetText(FText::FromString(bLoggedIn ? PlayFabMgr->GetUserInfo().DisplayName : TEXT("―")));
	}
	if (NickInput)
	{
		NickInput->SetText(FText::FromString(bLoggedIn ? PlayFabMgr->GetUserInfo().DisplayName : TEXT("")));
		NickInput->SetIsEnabled(bLoggedIn);
	}
	if (NickChangeBtn) NickChangeBtn->SetIsEnabled(bLoggedIn);
	if (LogoutBtn) LogoutBtn->SetIsEnabled(bLoggedIn);
	if (GoogleLinkRow)
	{
		const bool bShowGoogleLink = bLoggedIn && PlayFabMgr->GetUserInfo().LoginType == EPlayFabLoginType::Guest;
		GoogleLinkRow->SetVisibility(bShowGoogleLink ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void USettingsPanelWidget::HandleNickChangeClicked()
{
	UPlayFabManagerSubsystem* PlayFabMgr = GetPlayFabMgr();
	UUIManagerSubsystem* UIMgr = GetUIMgr();
	if (!PlayFabMgr || !NickInput)
	{
		return;
	}

	const FString NewName = NickInput->GetText().ToString().TrimStartAndEnd();
	if (NewName.Len() < 3 || NewName.Len() > 25)
	{
		if (UIMgr)
		{
			UIMgr->ShowColoredNotification(FText::FromString(TEXT("닉네임은 3~25자로 입력해 주세요")), 3.0f, FLinearColor(0.9f, 0.35f, 0.3f));
		}
		return;
	}
	if (NewName == PlayFabMgr->GetUserInfo().DisplayName)
	{
		return;
	}

	PlayFabMgr->UpdateDisplayName(NewName);
	if (UIMgr)
	{
		UIMgr->ShowColoredNotification(FText::FromString(TEXT("닉네임 변경을 요청했습니다")), 3.0f, FLinearColor::White);
	}
}

void USettingsPanelWidget::HandleGoogleLinkClicked()
{
	// 구글 인증 플로우는 기존 LoginPanel이 소유 — 닫고 열기 (스택 전환 = 닫기 먼저)
	if (UUIManagerSubsystem* UIMgr = GetUIMgr())
	{
		UIMgr->ShowAlertDialog(FText::FromString(TEXT("구글 계정 연동")),
			FText::FromString(TEXT("로그인 화면에서 구글 계정으로 다시 로그인하면 연동됩니다")),
			FSimpleDelegate());
	}
}

void USettingsPanelWidget::HandleLogoutClicked()
{
	if (UUIManagerSubsystem* UIMgr = GetUIMgr())
	{
		UIMgr->ShowConfirmDialog(FText::FromString(TEXT("로그아웃")), FText::FromString(TEXT("로그아웃 할까요?")),
			FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				if (UPlayFabManagerSubsystem* PlayFabMgr = GetPlayFabMgr())
				{
					PlayFabMgr->Logout();
				}
				RefreshAccountTab();
			}));
	}
}

void USettingsPanelWidget::HandleResetClicked()
{
	if (UUIManagerSubsystem* UIMgr = GetUIMgr())
	{
		UIMgr->ShowConfirmDialog(FText::FromString(TEXT("데이터 초기화")),
			FText::FromString(TEXT("모든 진행이 사라집니다 ― 되돌릴 수 없어요. 정말 초기화할까요?")),
			FSimpleDelegate::CreateUObject(this, &USettingsPanelWidget::HandleResetConfirmed));
	}
}

void USettingsPanelWidget::HandleResetConfirmed()
{
	// 순서 중요: 저장 억제 → 삭제 → 종료 안내. 억제 없이는 캐시 재저장이 슬롯을 부활시킴.
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SuppressSaving();
		SaveMgr->WipeAllSaveData();
	}

	if (UUIManagerSubsystem* UIMgr = GetUIMgr())
	{
		UIMgr->ShowAlertDialog(FText::FromString(TEXT("초기화 완료")),
			FText::FromString(TEXT("게임을 종료합니다. 다시 실행하면 처음부터 시작됩니다")),
			FSimpleDelegate::CreateUObject(this, &USettingsPanelWidget::HandleQuitConfirmed));
	}
	else
	{
		HandleQuitConfirmed();
	}
}

void USettingsPanelWidget::HandleQuitConfirmed()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void USettingsPanelWidget::ClosePanel()
{
	CloseWithAnimation();
}

void USettingsPanelWidget::OnBackgroundClicked()
{
	ClosePanel();
}

void USettingsPanelWidget::OnCloseClicked()
{
	ClosePanel();
}

USoundManagerSubsystem* USettingsPanelWidget::GetSoundMgr() const
{
	return GetGameInstance()->GetSubsystem<USoundManagerSubsystem>();
}

USettingsManagerSubsystem* USettingsPanelWidget::GetSettingsMgr() const
{
	return GetGameInstance()->GetSubsystem<USettingsManagerSubsystem>();
}

UPlayFabManagerSubsystem* USettingsPanelWidget::GetPlayFabMgr() const
{
	return GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>();
}

UUIManagerSubsystem* USettingsPanelWidget::GetUIMgr() const
{
	return GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
}
