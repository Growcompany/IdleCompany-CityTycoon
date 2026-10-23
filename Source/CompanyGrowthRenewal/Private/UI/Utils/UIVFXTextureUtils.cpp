// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Utils/UIVFXTextureUtils.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Manager/TableManagerSubsystem.h"

bool FUIVFXTextureUtils::SetTextureKeepStyle(const UTableManagerSubsystem* TableMgr, FName Key, UImage* Image)
{
	if (!TableMgr || !Image)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UIVFXTexture] %s: %s 가 없습니다"),
			*Key.ToString(), !TableMgr ? TEXT("TableManager") : TEXT("Image"));
		return false;
	}

	FUIVFXTexRow Row;
	if (!TableMgr->GetUIVFXTexRow(Key, Row))
	{
		UE_LOG(LogTemp, Warning, TEXT("[UIVFXTexture] DT_UIVFXTexture 에 %s 행이 없습니다"), *Key.ToString());
		return false;
	}
	if (Row.Texture.IsNull())
	{
		// 행 누락과 빈 참조는 고치는 곳이 다르다 — 앞은 행 추가, 뒤는 CSV 빈칸/리임포트다
		UE_LOG(LogTemp, Warning, TEXT("[UIVFXTexture] %s 행의 Texture 참조가 비었습니다"), *Key.ToString());
		return false;
	}

	UTexture2D* Tex = Row.Texture.LoadSynchronous();
	if (!Tex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UIVFXTexture] %s 텍스처 로드 실패"), *Key.ToString());
		return false;
	}

	// SetBrushResourceObject 는 ResourceObject 만 바꾼다 — ImageSize/TintColor/DrawAs 는 저작값 그대로 남는다
	Image->SetBrushResourceObject(Tex);
	return true;
}
