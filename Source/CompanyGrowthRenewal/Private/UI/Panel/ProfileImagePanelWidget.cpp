#include "UI/Panel/ProfileImagePanelWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Profile/ProfileAvatarTileWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Table/ProfileImageData.h"
#include "Enum/WidgetType.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/WrapBox.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Data/GameSaveData.h"
#include "Engine/Texture2D.h"
#include "UI/Panel/InGameLayerWidget.h"

void UProfileImagePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();

	TileGroup = NewObject<UCommonButtonGroupBase>(this);
	TileGroup->SetSelectionRequired(true);

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UProfileImagePanelWidget::OnCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UProfileImagePanelWidget::OnBackgroundClicked);
	}
}

void UProfileImagePanelWidget::NativeDestruct()
{
	if (TileGroup)
	{
		TileGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UProfileImagePanelWidget::OnCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UProfileImagePanelWidget::OnBackgroundClicked);
	}

	Super::NativeDestruct();
}

void UProfileImagePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	BuildTileGrid();
}

void UProfileImagePanelWidget::NativeOnDeactivated()
{
	if (TileGroup)
	{
		TileGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		TileGroup->RemoveAll();
	}
	if (ImageCardContainer)
	{
		ImageCardContainer->ClearChildren();
	}

	Super::NativeOnDeactivated();
}

void UProfileImagePanelWidget::BuildTileGrid()
{
	if (!ImageCardContainer || !TableMgr || !TileGroup) return;

	// 선택 복원이 저장/업로드를 튀기지 않도록, 델리게이트는 그리드를 다 세운 뒤에 붙인다.
	TileGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	TileGroup->RemoveAll();
	ImageCardContainer->ClearChildren();

	TSubclassOf<UUserWidget> TileClass = TableMgr->GetWidgetClass(EWidgetType::ProfileAvatarTile);
	if (!TileClass) return;

	int32 CurrentProfileImageID = 0;
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			CurrentProfileImageID = SaveData->GameData.ProfileImageID;
		}
	}

	const TArray<FProfileImageData> AllImages = TableMgr->GetAllProfileImages();

	int32 SelectedIndex = INDEX_NONE;
	for (int32 i = 0; i < AllImages.Num(); ++i)
	{
		UProfileAvatarTileWidget* Tile = CreateWidget<UProfileAvatarTileWidget>(this, TileClass);
		if (!Tile) continue;

		Tile->SetProfileImage(AllImages[i]);
		ImageCardContainer->AddChild(Tile);
		TileGroup->AddWidget(Tile);

		if (AllImages[i].ImageID == CurrentProfileImageID)
		{
			SelectedIndex = TileGroup->GetButtonCount() - 1;
		}
	}

	if (SelectedIndex != INDEX_NONE)
	{
		TileGroup->SelectButtonAtIndex(SelectedIndex, /*bAllowSound*/ false);
	}
	TileGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UProfileImagePanelWidget::OnTileSelectionChanged);

	if (CountText)
	{
		CountText->SetText(FText::AsNumber(AllImages.Num()));
	}
	RefreshPreview(CurrentProfileImageID);

	UE_LOG(LogTemp, Log, TEXT("[ProfileImagePanel] 아바타 타일 %d개 로드"), AllImages.Num());
}

void UProfileImagePanelWidget::OnTileSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	UProfileAvatarTileWidget* Tile = Cast<UProfileAvatarTileWidget>(SelectedButton);
	if (!Tile) return;

	ApplyProfileImage(Tile->GetImageID());
	RefreshPreview(Tile->GetImageID());
}

void UProfileImagePanelWidget::ApplyProfileImage(int32 ImageID)
{
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			SaveData->GameData.ProfileImageID = ImageID;
			SaveMgr->SaveGameData();
		}
	}

	// 다른 플레이어에게도 보이도록 랭킹 프로필 갱신
	if (URankingManagerSubsystem* RankingMgr = GetGameInstance()->GetSubsystem<URankingManagerSubsystem>())
	{
		RankingMgr->UploadRankingProfile();
	}

	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UInGameLayerWidget* InGameLayer = UIMgr->GetInGameLayer())
		{
			InGameLayer->UpdateProfileImage(ImageID);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ProfileImagePanel] 프로필 이미지 변경: ID=%d"), ImageID);
}

void UProfileImagePanelWidget::RefreshPreview(int32 ImageID)
{
	bool bFound = false;
	const FProfileImageData ImgData = TableMgr ? TableMgr->GetProfileImageData(ImageID, bFound) : FProfileImageData();

	if (PreviewImage && bFound && !ImgData.Icon.IsNull())
	{
		if (UTexture2D* Tex = ImgData.Icon.LoadSynchronous())
		{
			PreviewImage->SetBrushFromTexture(Tex);
		}
	}

	if (PreviewSubText)
	{
		PreviewSubText->SetText(bFound ? ImgData.DisplayName : FText::GetEmpty());
	}

	if (PreviewNameText)
	{
		FString DisplayName;
		if (UPlayFabManagerSubsystem* PlayFabMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			if (PlayFabMgr->IsLoggedIn())
			{
				DisplayName = PlayFabMgr->GetUserInfo().DisplayName;
			}
		}
		PreviewNameText->SetText(FText::FromString(DisplayName.IsEmpty() ? TEXT("Player") : DisplayName));
	}

	if (PreviewLevelText)
	{
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			PreviewLevelText->SetText(FText::AsNumber(SaveMgr->GetHQLevel()));
		}
	}
}

void UProfileImagePanelWidget::OnCloseButtonClicked()
{
	DeactivateWidget();
}

void UProfileImagePanelWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}
