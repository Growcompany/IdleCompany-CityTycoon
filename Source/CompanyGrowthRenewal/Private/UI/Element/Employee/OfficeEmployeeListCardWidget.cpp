// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Employee/OfficeEmployeeListCardWidget.h"
#include "Data/EntityCardData.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/UIIconData.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonTextBlock.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Animation/WidgetAnimation.h"

void UOfficeEmployeeListCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 이벤트 바인딩
	OnClicked().AddUObject(this, &UOfficeEmployeeListCardWidget::OnButtonClicked);

	// 선택 테두리 초기 숨김
	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 레벨업 이미지 초기 숨김
	if (LevelUpImage)
	{
		LevelUpImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOfficeEmployeeListCardWidget::NativeDestruct()
{
	Super::NativeDestruct();

	OnClicked().RemoveAll(this);
}

void UOfficeEmployeeListCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// ListView에서 데이터가 설정될 때 호출됨 (스크롤 시 재활용될 때마다)
	if (UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		SetEmployeeInfo(CardData->EmployeeInfo);
	}
}

void UOfficeEmployeeListCardWidget::SetEmployeeInfo(const FEmployeeInstance& InEmployeeData)
{
	EmployeeData = InEmployeeData;

	// 직원 초상화 로드
	if (EmployeeImage)
	{
		LoadPortraitImage(EmployeeData.EmployeeID);
	}

	// 부서 아이콘 로드
	LoadDepartmentIcon(EmployeeData.Department);

	// 레벨 표시
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), EmployeeData.Level)));
	}

	// 직원 이름 + 직급 표시
	if (EmployeeText)
	{
		// 직급 가져오기
		EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(EmployeeData.EnhancementLevel);
		FString RankString;
		switch (Rank)
		{
		case EEmployeeRank::Intern:           RankString = TEXT("인턴"); break;
		case EEmployeeRank::Assistant:        RankString = TEXT("사원"); break;
		case EEmployeeRank::Associate:        RankString = TEXT("주임"); break;
		case EEmployeeRank::SeniorAssociate:  RankString = TEXT("대리"); break;
		case EEmployeeRank::Manager:          RankString = TEXT("과장"); break;
		case EEmployeeRank::SeniorManager:    RankString = TEXT("차장"); break;
		case EEmployeeRank::Director:         RankString = TEXT("부장"); break;
		case EEmployeeRank::ManagingDirector: RankString = TEXT("이사"); break;
		case EEmployeeRank::ExecutiveDirector:RankString = TEXT("상무"); break;
		case EEmployeeRank::VP:               RankString = TEXT("전무"); break;
		case EEmployeeRank::VicePresident:    RankString = TEXT("부사장"); break;
		case EEmployeeRank::President:        RankString = TEXT("사장"); break;
		case EEmployeeRank::Chairman:         RankString = TEXT("회장"); break;
		default:                              RankString = TEXT(""); break;
		}

		FString DisplayText = FString::Printf(TEXT("%s %s"), *EmployeeData.EmployeeName, *RankString);
		EmployeeText->SetText(FText::FromString(DisplayText));
	}

	// 레벨업 가능 표시 업데이트
	UpdateLevelUpIndicator();

	UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeeListCardWidget] SetEmployeeInfo - ID: %d, Name: %s, Level: %d"),
		EmployeeData.EmployeeID, *EmployeeData.EmployeeName, EmployeeData.Level);
}

void UOfficeEmployeeListCardWidget::LoadDepartmentIcon(EEmployeeDepartment Department)
{
	if (!DepartmentImage)
	{
		return;
	}

	// 부서별 RowName
	FName RowName;
	switch (Department)
	{
	case EEmployeeDepartment::Development:
		RowName = TEXT("Department_Development");
		break;
	case EEmployeeDepartment::Design:
		RowName = TEXT("Department_Design");
		break;
	case EEmployeeDepartment::Sales:
		RowName = TEXT("Department_Sales");
		break;
	case EEmployeeDepartment::HR:
		RowName = TEXT("Department_HR");
		break;
	case EEmployeeDepartment::Management:
		RowName = TEXT("Department_Management");
		break;
	default:
		DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// TableManager에서 아이콘 가져오기
	UTableManagerSubsystem* TableManager = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager)
	{
		DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bool bSuccess = false;
	FUIIconData IconData = TableManager->GetUIIconData(RowName, bSuccess);

	if (bSuccess && !IconData.Icon.IsNull())
	{
		UTexture2D* IconTexture = IconData.Icon.LoadSynchronous();
		if (IconTexture)
		{
			DepartmentImage->SetBrushFromTexture(IconTexture);
			DepartmentImage->SetVisibility(ESlateVisibility::Visible);
			return;
		}
	}

	DepartmentImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UOfficeEmployeeListCardWidget::OnButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeeListCardWidget] Card clicked - EmployeeID: %d"), EmployeeData.EmployeeID);

	// 델리게이트 브로드캐스트
	OnOfficeEmployeeListCardClicked.Broadcast(EmployeeData.EmployeeID, this);
}

void UOfficeEmployeeListCardWidget::SetSelected(bool bInSelected)
{
	bIsSelected = bInSelected;

	// CommonButtonBase의 선택 스타일 적용
	SetSelectedInternal(bInSelected, false);

	// 선택 테두리 표시/숨김
	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(bInSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeeListCardWidget] SetSelected(%s) - EmployeeID: %d"),
		bInSelected ? TEXT("true") : TEXT("false"), EmployeeData.EmployeeID);
}

void UOfficeEmployeeListCardWidget::LoadPortraitImage(int32 EmployeeID)
{
	if (!EmployeeImage)
	{
		return;
	}

	// 파일 경로 생성 (플랫폼별)
	FString BasePath;

#if PLATFORM_ANDROID
	extern FString GExternalFilePath;
	BasePath = GExternalFilePath;
#elif PLATFORM_IOS
	BasePath = FPaths::ProjectSavedDir();
#else
	BasePath = FPaths::ProjectSavedDir();
#endif

	FString FilePath = BasePath / TEXT("Portraits") / (FString::FromInt(EmployeeID) + TEXT(".png"));

	// 파일 존재 확인
	if (!FPaths::FileExists(FilePath))
	{
		EmployeeImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 파일 읽기
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		EmployeeImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// ImageWrapper 모듈 가져오기
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		EmployeeImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Raw 데이터 추출
	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		EmployeeImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	int32 SrcWidth = ImageWrapper->GetWidth();
	int32 SrcHeight = ImageWrapper->GetHeight();
	int32 DstWidth = FMath::RoundToInt32(PortraitDisplaySize.X);
	int32 DstHeight = FMath::RoundToInt32(PortraitDisplaySize.Y);

	int32 FinalWidth = SrcWidth;
	int32 FinalHeight = SrcHeight;
	TArray<uint8> FinalData;

	// 원본이 표시 크기보다 크면 소프트웨어 리사이즈 (Officeworker.cpp와 동일 방식)
	if (DstWidth > 0 && DstHeight > 0 && (SrcWidth > DstWidth || SrcHeight > DstHeight))
	{
		TArray<FColor> SrcColors;
		SrcColors.SetNum(SrcWidth * SrcHeight);
		FMemory::Memcpy(SrcColors.GetData(), RawData.GetData(), RawData.Num());

		// RGB와 알파를 분리하여 리사이즈 (알파에 감마 보정이 적용되지 않도록)
		TArray<FColor> RGBOnly;
		TArray<FColor> AlphaAsColor;
		RGBOnly.SetNum(SrcWidth * SrcHeight);
		AlphaAsColor.SetNum(SrcWidth * SrcHeight);

		for (int32 i = 0; i < SrcColors.Num(); i++)
		{
			const FColor& Src = SrcColors[i];
			RGBOnly[i] = FColor(Src.R, Src.G, Src.B, 255);
			AlphaAsColor[i] = FColor(Src.A, Src.A, Src.A, 255);
		}

		TArray<FColor> ResizedRGB;
		FImageUtils::ImageResize(SrcWidth, SrcHeight, RGBOnly, DstWidth, DstHeight, ResizedRGB, false);

		TArray<FColor> ResizedAlpha;
		FImageUtils::ImageResize(SrcWidth, SrcHeight, AlphaAsColor, DstWidth, DstHeight, ResizedAlpha, false);

		// RGB + Alpha 재결합
		TArray<FColor> ResizedPixels;
		ResizedPixels.SetNum(DstWidth * DstHeight);
		for (int32 i = 0; i < ResizedPixels.Num(); i++)
		{
			ResizedPixels[i] = FColor(ResizedRGB[i].R, ResizedRGB[i].G, ResizedRGB[i].B, ResizedAlpha[i].R);
		}

		FinalWidth = DstWidth;
		FinalHeight = DstHeight;
		FinalData.SetNum(FinalWidth * FinalHeight * 4);
		FMemory::Memcpy(FinalData.GetData(), ResizedPixels.GetData(), FinalData.Num());
	}
	else
	{
		FinalData = MoveTemp(RawData);
	}

	// Texture2D 생성
	UTexture2D* Texture = UTexture2D::CreateTransient(FinalWidth, FinalHeight, PF_B8G8R8A8);
	if (!Texture)
	{
		EmployeeImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 텍스처에 데이터 복사
	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, FinalData.GetData(), FinalData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	// Image 위젯에 설정
	EmployeeImage->SetBrushFromTexture(Texture);
	EmployeeImage->SetVisibility(ESlateVisibility::Visible);
}

void UOfficeEmployeeListCardWidget::UpdateLevelUpIndicator()
{
	if (!LevelUpImage)
	{
		return;
	}

	// EmployeeManager에서 필요 경험치 확인
	UGameInstance* GI = GetWorld()->GetGameInstance();
	if (!GI)
	{
		LevelUpImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UEmployeeManager* EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	if (!EmployeeManager)
	{
		LevelUpImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 현재 레벨의 최대 경험치 확인
	int32 MaxExp = EmployeeManager->GetMaxExperienceForLevel(EmployeeData.Level);
	bool bCanLevelUp = (EmployeeData.Experience >= static_cast<float>(MaxExp));

	if (bCanLevelUp)
	{
		LevelUpImage->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 플로팅 애니메이션 재생 (무한 반복)
		if (LevelUpFloatAnim)
		{
			PlayAnimation(LevelUpFloatAnim, 0.f, 0);
		}
	}
	else
	{
		LevelUpImage->SetVisibility(ESlateVisibility::Collapsed);

		// 애니메이션 중지
		if (LevelUpFloatAnim)
		{
			StopAnimation(LevelUpFloatAnim);
		}
	}
}
