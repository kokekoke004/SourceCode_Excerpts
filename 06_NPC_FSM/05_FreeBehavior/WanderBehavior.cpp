#include"WanderBehavior.h"
#include"FreeTimeBehaviorUtility.h"
#include"CharacterBase.h"
#include"CharacterUpdateContext.h"
#include"JobActionComponent.h"
#include"LocomotionSystem.h"

namespace Game::Character::FreeTime
{
	bool WanderBehavior::CanInterruptByJob() const
	{
		return true;
	}

	int WanderBehavior::GetPriority(CharacterBase& character, const CharacterUpdateContext& updateContext) const
	{
		return BehaviorPriority::Wander;
	}

	bool WanderBehavior::CanStart(CharacterBase& character, const CharacterUpdateContext& updateContext) const
	{
		return true;
	}

	void WanderBehavior::Enter(CharacterBase& character)
	{
		auto& freeTimePhase = character.GetFreeTimeState().phase;
		freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
	}

	FreeTimeBehaviorUpdateResult WanderBehavior::Update(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& freeTimePhase = character.GetFreeTimeState().phase;

		switch (freeTimePhase)
		{
		case FreeTimeBehaviorPhase::SelectingTarget:
			{
				//目標座標セット
				bool result = action.SelectRandomTargetPosition(character, updateContext, 10.0f, 10);
				if (result) {
					freeTimePhase = FreeTimeBehaviorPhase::PathFinding;
					return FreeTimeBehaviorUpdateResult::Running;
				}
				else {
					freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
					return FreeTimeBehaviorUpdateResult::Failed;

				}

			}
			case FreeTimeBehaviorPhase::PathFinding:
			{
				PathGoal goalInfo;
				goalInfo.type = PathGoalType::InteractObject;
				goalInfo.goalPosition = action.GetTargetInfo().targetPos;
				goalInfo.interactRange = 1.5f;

				PathQuery pathQuery;

				auto result = action.BuildPathFromJobTarget(character, updateContext, goalInfo, pathQuery);
				if (result) {
					//ターゲットの選択に成功した場合
					freeTimePhase = FreeTimeBehaviorPhase::MovingToTarget;
					return FreeTimeBehaviorUpdateResult::Running;

				}
				else {
					//ターゲットの選択に失敗した場合
					action.ClearTarget();

					//別ターゲットを探し直す
					freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
					return FreeTimeBehaviorUpdateResult::Failed;


				}

				return FreeTimeBehaviorUpdateResult::Running;
			}
			case FreeTimeBehaviorPhase::MovingToTarget:
			{
				bool result = action.MoveTo(character, updateContext);
				if (result) {
					auto state = updateContext.engineServices.locomotion->GetPathFollowState(character.GetFbxInstanceHandle().tid);
					if (!state.followFlag) {
						waitRemainingTime_ = action.RandomFloat(3.0f, 10.0f);
						//目的地に到達した場合
						freeTimePhase = FreeTimeBehaviorPhase::Waiting;
						return FreeTimeBehaviorUpdateResult::Running;
					}
					//移動を続ける
					freeTimePhase = FreeTimeBehaviorPhase::MovingToTarget;
					return FreeTimeBehaviorUpdateResult::Running;

				}
				else {
					//ターゲットの選択に失敗した場合
					freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
					return FreeTimeBehaviorUpdateResult::Failed;
				}

				return FreeTimeBehaviorUpdateResult::Running;
			}
			case FreeTimeBehaviorPhase::Waiting:
			{
				waitElapsedTime_ += updateContext.simulationDeltaTime;

				//経過時間と設定した待機時間の比較
				if (waitElapsedTime_ >= waitRemainingTime_) {
					return FreeTimeBehaviorUpdateResult::Completed;

				}

				return FreeTimeBehaviorUpdateResult::Running;
			}
			default:
			{
				return FreeTimeBehaviorUpdateResult::Failed;
			}
		}
	}

	void WanderBehavior::Exit(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		auto freeTimePhase = character.GetFreeTimeState().phase;
		freeTimePhase = FreeTimeBehaviorPhase::Idle;
		waitElapsedTime_ = 0.0f;
		waitRemainingTime_ = 0.0f;
	}


}