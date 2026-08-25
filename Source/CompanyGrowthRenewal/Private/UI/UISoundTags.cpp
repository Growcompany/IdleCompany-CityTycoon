// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/UISoundTags.h"

namespace CGUISoundTags
{
	UE_DEFINE_GAMEPLAY_TAG(ButtonClick,           "UI.Sound.Button.Click");
	UE_DEFINE_GAMEPLAY_TAG(ButtonMainAction,      "UI.Sound.Button.MainAction");
	UE_DEFINE_GAMEPLAY_TAG(ButtonHover,           "UI.Sound.Button.Hover");
	UE_DEFINE_GAMEPLAY_TAG(ButtonPositive,        "UI.Sound.Button.Positive");
	UE_DEFINE_GAMEPLAY_TAG(ButtonNegative,        "UI.Sound.Button.Negative");
	UE_DEFINE_GAMEPLAY_TAG(ButtonDisabled,        "UI.Sound.Button.Disabled");

	UE_DEFINE_GAMEPLAY_TAG(ModalOpen,             "UI.Sound.Modal.Open");
	UE_DEFINE_GAMEPLAY_TAG(ModalClose,            "UI.Sound.Modal.Close");
	UE_DEFINE_GAMEPLAY_TAG(BottomSheetOpen,       "UI.Sound.BottomSheet.Open");
	UE_DEFINE_GAMEPLAY_TAG(BottomSheetClose,      "UI.Sound.BottomSheet.Close");

	UE_DEFINE_GAMEPLAY_TAG(NotificationInfo,      "UI.Sound.Notification.Info");
	UE_DEFINE_GAMEPLAY_TAG(NotificationSuccess,   "UI.Sound.Notification.Success");
	UE_DEFINE_GAMEPLAY_TAG(NotificationError,     "UI.Sound.Notification.Error");
	UE_DEFINE_GAMEPLAY_TAG(NotificationWarning,   "UI.Sound.Notification.Warning");

	UE_DEFINE_GAMEPLAY_TAG(TabSwitch,             "UI.Sound.Tab.Switch");
	UE_DEFINE_GAMEPLAY_TAG(ToggleOn,              "UI.Sound.Toggle.On");
	UE_DEFINE_GAMEPLAY_TAG(ToggleOff,             "UI.Sound.Toggle.Off");

	UE_DEFINE_GAMEPLAY_TAG(RewardCoin,            "UI.Sound.Reward.Coin");
	UE_DEFINE_GAMEPLAY_TAG(RewardDiamond,         "UI.Sound.Reward.Diamond");
	UE_DEFINE_GAMEPLAY_TAG(RewardBrick,           "UI.Sound.Reward.Brick");
	UE_DEFINE_GAMEPLAY_TAG(BrickPop,              "UI.Sound.Reward.BrickPop");
	UE_DEFINE_GAMEPLAY_TAG(RewardCompanyLevelUp,  "UI.Sound.Reward.CompanyLevelUp");
	UE_DEFINE_GAMEPLAY_TAG(RewardGeneric,         "UI.Sound.Reward.Generic");

	UE_DEFINE_GAMEPLAY_TAG(ResourceDigitMilestone, "UI.Sound.Resource.DigitMilestone");

	UE_DEFINE_GAMEPLAY_TAG(UpgradeSuccess,        "UI.Sound.Upgrade.Success");

	UE_DEFINE_GAMEPLAY_TAG(PurchaseSuccess,       "UI.Sound.Purchase.Success");
	UE_DEFINE_GAMEPLAY_TAG(PurchaseFail,          "UI.Sound.Purchase.Fail");

	UE_DEFINE_GAMEPLAY_TAG(GachaPull,             "UI.Sound.Gacha.Pull");
	UE_DEFINE_GAMEPLAY_TAG(GachaResultRare,       "UI.Sound.Gacha.Result.Rare");
	UE_DEFINE_GAMEPLAY_TAG(GachaResultEpic,       "UI.Sound.Gacha.Result.Epic");
	UE_DEFINE_GAMEPLAY_TAG(GachaResultLegendary,  "UI.Sound.Gacha.Result.Legendary");
	UE_DEFINE_GAMEPLAY_TAG(GachaBuildup,          "UI.Sound.Gacha.Buildup");
	UE_DEFINE_GAMEPLAY_TAG(GachaFlash,            "UI.Sound.Gacha.Flash");
	UE_DEFINE_GAMEPLAY_TAG(GachaCardPop,          "UI.Sound.Gacha.CardPop");
	UE_DEFINE_GAMEPLAY_TAG(GachaShine,            "UI.Sound.Gacha.Shine");
	UE_DEFINE_GAMEPLAY_TAG(GachaStamp,            "UI.Sound.Gacha.Stamp");

	UE_DEFINE_GAMEPLAY_TAG(PotentialReroll,       "UI.Sound.Potential.Reroll");

	UE_DEFINE_GAMEPLAY_TAG(Catch,                 "UI.Sound.Catch");

	UE_DEFINE_GAMEPLAY_TAG(GroupAll,              "UI.Sound");
	UE_DEFINE_GAMEPLAY_TAG(GroupNotification,     "UI.Sound.Notification");
	UE_DEFINE_GAMEPLAY_TAG(GroupGacha,            "UI.Sound.Gacha");
}
