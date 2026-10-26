#pragma once

namespace Game::Character::FreeTime
{
    //各行動の優先度
    namespace BehaviorPriority
    {
        inline constexpr int Wander = 10;
        inline constexpr int Social = 40;
        inline constexpr int Rest = 80;
        inline constexpr int Eat = 100;
    }

    //暇な時間での行動状態の一覧
    enum class FreeTimeBehaviorType
    {
        None,
        Eat,
        Wander,
        Rest
    };

    enum class FreeTimeBehaviorUpdateResult
    {
        Running,
        Completed,
        Failed
    };

    //自由時間行動の現在の処理フェーズ
    enum class FreeTimeBehaviorPhase
    {
        Idle,

        //共通
        SelectingTarget,
        PathFinding,
        MovingToTarget,
        Waiting,

        //食事
        WithdrawingFood,
        Eating,

        //休憩
        Resting,

    };

    struct FreeTimeState
    {
        //現在選択されている暇時間行動
        FreeTimeBehaviorType type = FreeTimeBehaviorType::None;

        //Enter済みの行動
        FreeTimeBehaviorType activeType = FreeTimeBehaviorType::None;
        
        //現在の行動フェーズ
        FreeTimeBehaviorPhase phase = FreeTimeBehaviorPhase::Idle;
    };

}