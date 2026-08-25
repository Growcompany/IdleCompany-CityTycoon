// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

namespace InputPriority
{
    constexpr int32 UI_BLOCKING = 100;
    constexpr int32 EMERGENCY = 50;
    constexpr int32 INTERACTION = 20;
    constexpr int32 PLACEMENT = 10;
    constexpr int32 MOVEMENT = 5;
    constexpr int32 DEFAULT = 0;
    constexpr int32 BACKGROUND = -10;
}