#include"MinerJob.h"
#include"CharacterBase.h"
#include"JobActionComponent.h"
#include"NavigationSystem.h"
#include"LocomotionSystem.h"

namespace Game::Character::Job
{
	MinerJob::MinerJob() = default;

	void MinerJob::Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext)
	{

	}

	void MinerJob::Resume(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& jobPhase = character.GetJobState().phase;

		//移動系のフェーズでなければ再開処理は行わない
		if (jobPhase != JobWorkPhase::MovingToTarget && jobPhase != JobWorkPhase::PathFinding && jobPhase != JobWorkPhase::Mining) {
			return;
		}

		//保存されたターゲットがなければ何もしない
		if (!suspendedTarget_) {
			return;
		}

		//前にターゲットだったものが消えていた時の処理
		if (updateContext.engineServices.transform->GetLocal(suspendedTarget_->targetTid) == nullptr) {
			jobPhase = JobWorkPhase::Idle;
			suspendedTarget_.reset();
			action.ClearTarget();

			return;
		}

		//ターゲット情報を復元
		action.SetTargetInfo(*suspendedTarget_);

		//経路探索から再開
		jobPhase = JobWorkPhase::PathFinding;

	}


	JobUpdateResult MinerJob::Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		auto& action = character.GetJobActionComponent();
		auto jobPhase = character.GetJobState().phase;

		switch (jobPhase)
		{
		case JobWorkPhase::Idle:
		{
			character.GetJobState().phase = JobWorkPhase::SelectingTarget;
			suspendedTarget_.reset();
			return JobUpdateResult::NoWork;
		}
		case JobWorkPhase::SelectingTarget:
		{
			bool result = action.SelectNearestTargetObject(character, updateContext, L"rock", 100.0f);
			if (result) {
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::PathFinding;
				suspendedTarget_ = action.GetTargetInfo();
			}
			else {
				//ターゲットの選択に失敗した場合
				character.GetJobState().phase = JobWorkPhase::Idle;
				return JobUpdateResult::NoWork;
			}
			break;
		}
		case JobWorkPhase::PathFinding:
		{
			PathGoal goalInfo;
			goalInfo.type = PathGoalType::InteractObject;
			goalInfo.goalPosition = action.GetTargetInfo().targetPos;
			goalInfo.interactRange = 1.5f;

			PathQuery pathQuery;

			//経路探索の処理
			bool result = action.BuildPathFromJobTarget(character, updateContext, goalInfo, pathQuery);
			if (result) {
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				action.ClearTarget();

				//別ターゲットを探し直す
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;

				return JobUpdateResult::Working;


			}
			break;
		}
		case JobWorkPhase::MovingToTarget:
		{
			bool result = action.MoveTo(character, updateContext);
			if (result) {
				auto state = updateContext.engineServices.locomotion->GetPathFollowState(character.GetFbxInstanceHandle().tid);
				if (!state.followFlag) {
					//目的地に到達した場合
					character.GetJobState().phase = JobWorkPhase::Mining;
					return JobUpdateResult::Working;
				}
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				action.ClearTarget();

				character.GetJobState().phase = JobWorkPhase::SelectingTarget;

				return JobUpdateResult::Working;
			}

			break;
		}

		case JobWorkPhase::Mining:
		{
			bool result = action.Attack(character, updateContext);
			if (result) {
				auto& comState = character.GetCharacterCombatState();
				if (comState.combatFlags.isInCombat) {
					//攻撃に成功した場合
					character.GetJobState().phase = JobWorkPhase::Mining;
				}
				else {
					//戦闘が終了した場合
					character.GetJobState().phase = JobWorkPhase::Idle;
					return JobUpdateResult::Working;

				}
			}
		}

		default:
		{
			break;
		}
		}
		return JobUpdateResult::Working;

	}
}