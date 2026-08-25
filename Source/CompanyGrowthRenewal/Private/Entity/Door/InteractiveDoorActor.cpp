// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Door/InteractiveDoorActor.h"
#include "Kismet/GameplayStatics.h"

AInteractiveDoorActor::AInteractiveDoorActor()
{
	// Tick은 DoorTimeline 진행 시에만 켜짐 (Open/Close에서 enable, OnTimelineFinished에서 disable)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AInteractiveDoorActor::BeginPlay()
{
	Super::BeginPlay();

	// HingeComponentName으로 BP 컴포넌트 자동 탐색
	if (!HingeComponent && !HingeComponentName.IsNone())
	{
		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (USceneComponent* Comp : Components)
		{
			if (Comp->GetFName() == HingeComponentName)
			{
				HingeComponent = Comp;
				break;
			}
		}

		if (!HingeComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] '%s' 이름의 힌지 컴포넌트를 찾지 못했습니다."),
				*GetName(), *HingeComponentName.ToString());
		}
	}

	// 런타임 커브 생성 (0→1, EaseInOut 느낌을 위해 중간 키 추가)
	DoorCurve = NewObject<UCurveFloat>(this);
	DoorCurve->FloatCurve.AddKey(0.f, 0.f);
	DoorCurve->FloatCurve.AddKey(OpenDuration, 1.f);

	// 키 보간을 Cubic으로 설정하여 부드러운 이징
	for (auto It = DoorCurve->FloatCurve.GetKeyHandleIterator(); It; ++It)
	{
		DoorCurve->FloatCurve.SetKeyInterpMode(*It, ERichCurveInterpMode::RCIM_Cubic);
	}

	// 타임라인 콜백 바인딩
	FOnTimelineFloat UpdateCallback;
	UpdateCallback.BindUFunction(this, FName("OnTimelineUpdate"));

	FOnTimelineEvent FinishedCallback;
	FinishedCallback.BindUFunction(this, FName("OnTimelineFinished"));

	DoorTimeline.AddInterpFloat(DoorCurve, UpdateCallback);
	DoorTimeline.SetTimelineFinishedFunc(FinishedCallback);
	DoorTimeline.SetTimelineLength(OpenDuration);
}

void AInteractiveDoorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DoorTimeline.TickTimeline(DeltaTime);
}

void AInteractiveDoorActor::OpenDoor()
{
	if (bIsOpen) return;

	if (DoorSound)
	{
		UGameplayStatics::SpawnSound2D(this, DoorSound);
	}

	bIsOpen = true;
	DoorTimeline.Play();
	SetActorTickEnabled(true);
}

void AInteractiveDoorActor::CloseDoor()
{
	if (!bIsOpen) return;

	if (DoorSound)
	{
		UGameplayStatics::SpawnSound2D(this, DoorSound);
	}

	bIsOpen = false;
	DoorTimeline.Reverse();
	SetActorTickEnabled(true);
}

void AInteractiveDoorActor::OnTimelineUpdate(float Alpha)
{
	if (!HingeComponent) return;

	const FRotator NewRotation = FMath::Lerp(FRotator::ZeroRotator, DoorOpenRotation, Alpha);
	HingeComponent->SetRelativeRotation(NewRotation);
}

void AInteractiveDoorActor::OnTimelineFinished()
{
	SetActorTickEnabled(false);

	if (bIsOpen)
	{
		OnDoorOpened.Broadcast();
	}
	else
	{
		OnDoorClosed.Broadcast();
	}
}
