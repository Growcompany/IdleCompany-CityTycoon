#pragma once

#include "CoreMinimal.h"
#include "Enum/MissionTypes.h"
#include "Manager/MissionGuidePhases.h"
#include "UI/Element/Common/GestureHintTypes.h"

enum class EGestureAnchorKind : uint8 { None, BrickFactory, PlacementPreview, SpotlightActor };

// 페이즈 → 제스처/앵커/딤. 매니저 상태(미션 없음·클레임)는 호출자가 먼저 거른다 — 여기는 표만 안다.
namespace GuideGestureRules
{
	inline bool IsPlacementDragPhase(EMissionConditionType Cond, int32 Phase)
	{
		return (Cond == EMissionConditionType::BuildFirstBuilding && Phase == BuildGuide::PlaceBuilding)
			|| (Cond == EMissionConditionType::PlaceDesks && (Phase == DeskGuide::Confirm || Phase == DeskGuide::PlaceMore));
	}
	inline bool IsFactoryHoldPhase(EMissionConditionType Cond, int32 Phase)
	{
		return Cond == EMissionConditionType::CollectBricks && Phase == BrickGuide::BoostLaborers;
	}
	inline bool IsWorldTapPhase(EMissionConditionType Cond, int32 Phase)
	{
		return Cond == EMissionConditionType::EnterOffice && Phase == EnterOfficeGuide::ClickBuilding;
	}
	inline EGestureHintKind ResolveGesture(EMissionConditionType Cond, int32 Phase)
	{
		if (IsFactoryHoldPhase(Cond, Phase)) return EGestureHintKind::Hold;
		if (IsPlacementDragPhase(Cond, Phase)) return EGestureHintKind::Drag;
		if (IsWorldTapPhase(Cond, Phase)) return EGestureHintKind::Tap;
		return EGestureHintKind::None;
	}
	inline EGestureAnchorKind ResolveAnchor(EMissionConditionType Cond, int32 Phase)
	{
		if (IsFactoryHoldPhase(Cond, Phase)) return EGestureAnchorKind::BrickFactory;
		if (IsPlacementDragPhase(Cond, Phase)) return EGestureAnchorKind::PlacementPreview;
		if (IsWorldTapPhase(Cond, Phase)) return EGestureAnchorKind::SpotlightActor;
		return EGestureAnchorKind::None;
	}
	// 딤 0 = 배치·강화 조작 및 M6 기획안 선택/개발 관찰/출시에서 주변 화면 보존.
	inline float ResolveDimScale(EMissionConditionType Cond, int32 Phase)
	{
		if (IsPlacementDragPhase(Cond, Phase)) return 0.f;
		if (Cond == EMissionConditionType::CollectBricks && Phase == BrickGuide::CollectBricks) return 0.f;
		if (Cond == EMissionConditionType::LaunchFirstInHouseProject
			&& (Phase == InHouseGuide::PickInHouse || Phase == InHouseGuide::WatchStrip || Phase == InHouseGuide::PressLaunch)) return 0.f;
		return 1.f;
	}
}
