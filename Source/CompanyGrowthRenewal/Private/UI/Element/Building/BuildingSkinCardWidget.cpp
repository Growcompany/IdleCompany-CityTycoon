// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Building/BuildingSkinCardWidget.h"
#include "UI/Element/Building/SkinInfoWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"

void UBuildingSkinCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터에서만 미리보기
	if (IsDesignTime())
	{
		if (EntityImage && !PreviewImage.IsNull())
		{
			UTexture2D* Texture = PreviewImage.LoadSynchronous();
			if (Texture)
			{
				EntityImage->SetBrushFromTexture(Texture);
				EntityImage->SetDesiredSizeOverride(PreviewImageSize);
			}
		}

		if (UIE_SkinInfo)
		{
			UIE_SkinInfo->SetSkinName(PreviewSkinName);
			UIE_SkinInfo->SetBackgroundColor(PreviewBackgroundColor);
		}

		// 프리뷰/쇼케이스 카드는 잠금 표시 안 함 (잠금은 SetSkinData/SetLocked 로 명시할 때만)
		SetLocked(false);
	}
}

void UBuildingSkinCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// SetSkinData가 호출되지 않은 경우에만 Preview 값 적용
	if (!bSkinDataSet)
	{
		if (EntityImage && !PreviewImage.IsNull())
		{
			UTexture2D* Texture = PreviewImage.LoadSynchronous();
			if (Texture)
			{
				EntityImage->SetBrushFromTexture(Texture);
				EntityImage->SetDesiredSizeOverride(PreviewImageSize);
			}
		}

		if (UIE_SkinInfo)
		{
			UIE_SkinInfo->SetSkinName(PreviewSkinName);
			UIE_SkinInfo->SetBackgroundColor(PreviewBackgroundColor);
		}

		// 데이터 미설정(프리뷰/쇼케이스) 카드는 기본 잠금 해제 — 잠금은 명시적 SetSkinData/SetLocked 로만.
		SetLocked(false);

		// 여기서 HitTestInvisible 로 입력을 끊으면 안 된다: MenuPanel 의 순위/스킨/가챠 버튼은
		// SetSkinData 없이 Preview 값만으로 그리는 실제 버튼이라 같이 클릭이 죽는다(c22dcc88 회귀).
		// 보여주기 전용 카드는 배치한 WBP/패널 쪽에서 입력을 끄는 게 맞다.
	}

	// 선택 가능하도록 설정 (SetIsSelected가 작동하려면 필요)
	SetIsSelectable(true);

	// 토글 비활성화 (클릭 시 자동 토글 방지)
	SetIsToggleable(false);

	// 클릭 이벤트는 카드를 생성하는 패널(BuildingManagePanel/ProfileImagePanel)에서 바인딩함
}

void UBuildingSkinCardWidget::NativeDestruct()
{
	Super::NativeDestruct();

	// 생성 패널에서 WeakLambda로 바인딩하므로 자동 정리됨
}

void UBuildingSkinCardWidget::SetSkinData(const FBuildingSkinData& InSkinData, bool bIsUnlocked)
{
	SkinData = InSkinData;
	bUnlocked = bIsUnlocked;
	bSkinDataSet = true;

	// 스킨 아이콘 이미지 설정
	if (EntityImage)
	{
		// Soft Object Pointer에서 아이콘 로드
		UTexture2D* IconTexture = SkinData.Icon.LoadSynchronous();
		if (IconTexture)
		{
			EntityImage->SetBrushFromTexture(IconTexture);
		}
	}

	// 스킨 이름과 희귀도 색상 설정
	if (UIE_SkinInfo)
	{
		UIE_SkinInfo->SetSkinInfo(SkinData.DisplayName, SkinData.Rarity);
	}

	// LockBorder 표시/숨김 (LockImage/LockText 는 LockBorder 자식 → 함께 숨겨짐)
	if (LockBorder)
	{
		LockBorder->SetVisibility(bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UBuildingSkinCardWidget::SetLocked(bool bIsLocked)
{
	bUnlocked = !bIsLocked;

	if (LockBorder)
	{
		LockBorder->SetVisibility(bIsLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
