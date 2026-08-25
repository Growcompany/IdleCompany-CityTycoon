// 건물 상단 말풍선 버블 컨테이너 위젯
// 위치 추적/애니메이션은 Container가 담당, 비주얼은 BubbleElementWidget(WBP)이 담당
// 건물별 상태 조건에 따라 말풍선 아이콘을 카메라 추적하며 표시

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/BubbleType.h"
#include "BubbleContainerWidget.generated.h"

DECLARE_DELEGATE_TwoParams(FOnBubbleAction, int32 /*BuildingIndex*/, EBubbleType /*Type*/);
DECLARE_MULTICAST_DELEGATE(FOnActiveBubbleMembershipChanged);
class UCanvasPanel;
class UCanvasPanelSlot;
class UBubbleElementWidget;
class UTexture2D;
class UMaterialInterface;
class AActor;

// 버블 애니메이션 상태
UENUM()
enum class EBubbleAnimState : uint8
{
	Appearing,
	Idle,
	Disappearing,
};

struct FBubbleDisappearingTransition
{
	EBubbleAnimState NextAnimState = EBubbleAnimState::Disappearing;
	EBubbleType PendingType = EBubbleType::None;
	bool bResetAnimElapsed = false;
};

inline bool ShouldAdvanceBubbleAnimation(bool bIsOnScreen, EBubbleAnimState AnimState)
{
	return bIsOnScreen || AnimState == EBubbleAnimState::Disappearing;
}

inline FBubbleDisappearingTransition ResolveDisappearingBubbleTransition(
	EBubbleType CurrentType,
	EBubbleType DesiredType)
{
	FBubbleDisappearingTransition Transition;
	if (DesiredType != EBubbleType::None && DesiredType == CurrentType)
	{
		Transition.NextAnimState = EBubbleAnimState::Appearing;
		Transition.bResetAnimElapsed = true;
	}
	else
	{
		Transition.PendingType = DesiredType;
	}

	return Transition;
}

// 개별 버블 애니메이션 데이터
USTRUCT()
struct FBubbleAnimData
{
	GENERATED_BODY()

	// 버블의 식별 키. 빌딩 경로에서는 BuildingIndex, 앵커 경로에서는 임의의 안정 키(액터 UniqueID 등)
	int32 BuildingIndex = INDEX_NONE;
	EBubbleType CurrentType = EBubbleType::None;

	// 앵커 경로 전용 — 위치를 IBubbleAnchorProvider로 추적할 액터. 빌딩 경로에서는 null(EntityManager로 해석)
	UPROPERTY()
	TWeakObjectPtr<AActor> AnchorActor = nullptr;

	// true면 앵커 액터로 위치 해석(액터 파괴 시 숨김), false면 BuildingIndex로 EntityManager 해석
	bool bUsesAnchor = false;

	UPROPERTY()
	UBubbleElementWidget* BubbleWidget = nullptr;

	// CreateBubble 시 캐싱한 Canvas Slot (NativeTick 매 프레임 Cast 회피)
	UPROPERTY()
	UCanvasPanelSlot* CachedSlot = nullptr;

	EBubbleAnimState AnimState = EBubbleAnimState::Appearing;
	float AnimElapsed = 0.0f;
	float IdleTime = 0.0f;

	// 전환 시 대기 타입 (Disappearing 완료 후 이 타입으로 재생성)
	EBubbleType PendingType = EBubbleType::None;
	bool bIsOffscreen = false;

	// 직전 프레임에 SetVisibility로 적용한 값 (Slate Invalidate 누적 방지용)
	ESlateVisibility LastAppliedVisibility = ESlateVisibility::Hidden;
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UBubbleContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 건물의 버블 상태 갱신 (외부에서 호출)
	// NewType이 None이면 버블 제거, 기존과 같으면 무시, 다르면 전환
	void UpdateBubbleForBuilding(int32 BuildingIndex, EBubbleType NewType);

	// 임의 액터(책상 등)를 앵커로 한 버블 상태 갱신.
	// Key는 호출자가 관리하는 안정 식별자(예: 액터 UniqueID). Anchor는 IBubbleAnchorProvider 구현 액터.
	void UpdateBubbleForAnchor(int32 Key, AActor* Anchor, EBubbleType NewType);

	// Key에 연결된 앵커 액터 반환 (버블 클릭 → 대상 액터 역해석용). 없으면 nullptr
	AActor* GetBubbleAnchorActor(int32 Key) const;

	// 모달 팝업 시 전체 숨김/복원
	void SetAllBubblesVisible(bool bVisible);

	// BubbleCanvas의 CachedGeometry 반환 (좌표 변환용)
	FGeometry GetCanvasGeometry() const;

	// 버블 클릭 시 InGameLayerWidget에 전달되는 델리게이트
	FOnBubbleAction OnBubbleAction;

	// ActiveBubbles 멤버십이 실제로 추가되거나 최종 제거된 뒤 알린다.
	FOnActiveBubbleMembershipChanged OnActiveBubbleMembershipChanged;

	// 외부에서 특정 건물의 버블 즉시 제거 (수금 후 등)
	void DismissBubble(int32 BuildingIndex);

	// 튜토리얼 하이라이트용 — 현재 표시 중인 첫 버블 위젯(없으면 null)
	UWidget* GetFirstBubbleWidget() const;

	// 튜토리얼 하이라이트용 — 특정 앵커 키(책상 UniqueID)의 버블 위젯 반환(없으면 null).
	// 스포트라이트가 고른 책상과 링을 정합시키기 위해 사용.
	UWidget* GetBubbleWidgetForAnchor(int32 Key) const;

	// 버블이 아직 없는 건물의 앵커가 화면 안인지 미리 조회 — 동시 표시 상한 경합에서
	// 화면 밖 건물이 슬롯을 먹지 않게 InGameLayer 가 쓴다.
	// 컨테이너가 매 틱 가시성을 정할 때 쓰는 것과 같은 투영·같은 여유값이므로 선택과 표시가 갈라질 수 없다.
	bool IsBuildingAnchorOnScreen(int32 BuildingIndex) const;

	// Appearing/Idle/Disappearing을 포함해 실제 ActiveBubbles 멤버십만 조회한다.
	bool HasActiveBubbleForBuilding(int32 BuildingIndex) const;

	// 튜토리얼 설명 스포트라이트용 — 건물 버블 위젯. 사라지는 중이면 nullptr
	// (구멍은 남고 버블만 사라져 빈 구멍을 설명하게 된다).
	UWidget* GetBubbleWidgetForBuilding(int32 BuildingIndex) const;

	// 금고 게이지가 뜬 건물의 버블만 그만큼 위로 올린다 — 둘은 앵커·정렬·ZOrder 가 같아 안 올리면 정확히 겹친다.
	// 소유자는 게이지 풀과 이 컨테이너를 둘 다 쥔 InGameLayerWidget.
	void SetExtraLiftForBuilding(int32 BuildingIndex, float Lift);
	void ClearAllExtraLifts();

private:
	// 버블 클릭 이벤트 핸들러 (Element → Container 릴레이)
	void HandleBubbleClicked(int32 BuildingIndex, EBubbleType Type);
	// 빌딩/앵커 공통 상태 갱신 내부 구현 (Anchor==null → 빌딩 경로)
	void UpdateBubbleInternal(int32 Key, AActor* Anchor, EBubbleType NewType);
	// 버블 생성/제거
	void CreateBubble(int32 Key, AActor* Anchor, EBubbleType Type);
	void RemoveBubble(int32 Key);

	// 애니메이션 상태 머신 업데이트
	void UpdateBubbleAnimation(FBubbleAnimData& Data, float DeltaTime);

	// 3D 앵커 위치 → Canvas 로컬 좌표 변환 (AbsoluteToLocal 패턴).
	// AnchorActor가 유효하면 IBubbleAnchorProvider로, 아니면 BuildingIndex로 EntityManager 해석
	FVector2D GetBubbleScreenPosition(const FBubbleAnimData& Data) const;

	// 좌표가 Canvas 범위 내인지 체크
	bool IsBuildingOnScreen(const FVector2D& CanvasLocal) const;

	UPROPERTY()
	UCanvasPanel* BubbleCanvas = nullptr;

	// BuildingIndex → 애니메이션 데이터
	UPROPERTY()
	TMap<int32, FBubbleAnimData> ActiveBubbles;

	// 캐시된 텍스처
	UPROPERTY()
	UTexture2D* CachedBubbleBG = nullptr;

	UPROPERTY()
	UTexture2D* CachedDefaultIcon = nullptr;

	UPROPERTY()
	TMap<EBubbleType, UTexture2D*> CachedBubbleIcons;

	// 타입별 셸 머티리얼 캐시 (MI_UI_BubbleShell_*). 없는 타입은 CachedBubbleBG 텍스처로 폴백
	UPROPERTY()
	TMap<EBubbleType, UMaterialInterface*> CachedShells;

	// 전체 표시 여부 (모달 시 숨김)
	bool bGlobalVisible = true;

	// 건물 인덱스 -> 추가 리프트(px). 게이지가 뜬 건물만 들어 있다(게이지 캡과 같은 크기).
	TMap<int32, float> ExtraLiftByBuilding;

	// TableManager에서 가져온 Element WBP 클래스 (lazy-init 캐시)
	UPROPERTY()
	TSubclassOf<UUserWidget> CachedBubbleElementClass;

	// 상수
	static constexpr float OffscreenMargin = 48.0f;   // 화면 밖 판정 여유 (기존 BubbleSize 대체)
	static constexpr float WorldZOffset = 0.0f;           // 건물 꼭대기 기준 (GetBubbleAnchorPosition이 이미 꼭대기 반환)
	static constexpr float ScreenYOffset_Close = 0.0f;  // ZoomValue=0 (줌인)
	static constexpr float ScreenYOffset_Far   = 0.0f;  // ZoomValue=1 (줌아웃)
	static constexpr float AppearDuration = 0.25f;     // 등장 애니메이션 시간
	static constexpr float DisappearDuration = 0.20f;  // 사라짐 애니메이션 시간
	static constexpr float BounceScale = 1.2f;         // 등장 시 오버슈트 스케일
	static constexpr float BounceCycle = 2.6f;         // Idle 바운스 주기(초)
	static constexpr float BounceAmplitude = 7.0f;     // Idle 바운스 진폭(px)
	static constexpr float BouncePhaseStep = 0.26f;    // 키별 위상 어긋냄 — 여러 버블이 한 박자로 뛰면 기계적으로 보인다

	// DataAsset 에셋 경로 (에디터에서 DA_BubbleIconConfig 생성 후 경로 지정)
	TSoftObjectPtr<class UBubbleIconConfig> BubbleIconConfigPath;
};

inline bool ApplyMissingBubbleSourcePolicy(
	bool bSourceMissing,
	EBubbleAnimState& AnimState,
	EBubbleType& PendingType)
{
	if (!bSourceMissing) return false;

	PendingType = EBubbleType::None;
	if (AnimState == EBubbleAnimState::Disappearing) return false;

	AnimState = EBubbleAnimState::Disappearing;
	return true;
}
