// MyHUD.cpp
#include "UI/HUD/MyHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Player/InputTypeManager.h"  // EInputType ���� ��ġ
#include "Player/MainMapPlayerController.h"

void AMyHUD::DrawHUD()
{
    Super::DrawHUD();

    // ��Ʈ�ѷ��� �����ͼ� ���� ��� �б�
    if (APlayerController* PC = GetOwningPlayerController())
    {
        if (AMainMapPlayerController* MyPC = Cast<AMainMapPlayerController>(PC))
        {
            FString ModeString;
            switch (MyPC->GetCurrentInputType())
            {
            case EInputType::KeyMouse: ModeString = TEXT("Keyboard/Mouse Mode"); break;
            case EInputType::GamePad:  ModeString = TEXT("GamePad Mode");       break;
            case EInputType::Touch:    ModeString = TEXT("Touch Mode");         break;
            default:                   ModeString = TEXT("Unknown Mode");       break;
            }

            // ȭ�� ���� ���(50,50)�� �� �۾��� ���
            const float X = 50.f, Y = 250.f;
            const FColor Color = FColor::White;
            UFont* Font = GEngine->GetMediumFont();
            const float Scale = 1.2f;
            DrawText(ModeString, Color, X, Y, Font, Scale);
        }
    }
}
