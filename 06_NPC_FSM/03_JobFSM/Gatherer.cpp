#include"Gatherer.h"
#include"CharacterBase.h"
#include"JobActionComponent.h"
#include"NavigationSystem.h"
#include"LocomotionSystem.h"

namespace Game::Character::Job
{
	Gatherer::Gatherer() = default;

	void Gatherer::Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext)
	{

	}

	void Gatherer::Resume(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& jobPhase = character.GetJobState().phase;

		//移動系のフェーズでなければ再開処理は行わない
		if (jobPhase != JobWorkPhase::MovingToTarget && jobPhase != JobWorkPhase::PathFinding && jobPhase != JobWorkPhase::Gathering) {
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


	JobUpdateResult Gatherer::Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
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
			//採取クラスが探すことになるオブジェクトの候補をリストで管理しておいて、順番に探していく
			std::vector<std::wstring> targetObjects = { L"grass", L"bush", L"flower" };
			for (const auto& targetObject : targetObjects) {
				bool result = action.SelectNearestTargetObject(character, updateContext, targetObject, 100.0f);
				if (result) {
					//ターゲットの選択に成功した場合
					character.GetJobState().phase = JobWorkPhase::PathFinding;
					suspendedTarget_ = action.GetTargetInfo();
					return JobUpdateResult::Working;
				}
			}
			//ターゲットの選択に失敗した場合
			character.GetJobState().phase = JobWorkPhase::Idle;
			return JobUpdateResult::NoWork;
			break;
		}
		case JobWorkPhase::PathFinding:
		{
			//経路探索の処理
			PathGoal goalInfo;
			goalInfo.type = PathGoalType::InteractObject;
			goalInfo.goalPosition = action.GetTargetInfo().targetPos;
			bool result = action.BuildPathFromJobTarget(character, updateContext, goalInfo, PathQuery());
			if (result) {
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				action.ClearTarget();

				//ターゲットの選択に失敗した場合
				character.GetJobState().phase = JobWorkPhase::Idle;
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
					character.GetJobState().phase = JobWorkPhase::Gathering;
					return JobUpdateResult::Working;
				}
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				action.ClearTarget();

				//ターゲットの選択に失敗した場合
				character.GetJobState().phase = JobWorkPhase::Idle;

				return JobUpdateResult::Working;
			}

			break;
		}

		case JobWorkPhase::Gathering:
		{
			bool result = action.Attack(character, updateContext);
			if (result) {
				auto& comState = character.GetCharacterCombatState();
				if (comState.combatFlags.isInCombat) {
					//攻撃に成功した場合
					character.GetJobState().phase = JobWorkPhase::Gathering;
				}
				else {
					//戦闘が終了した場合
					character.GetJobState().phase = JobWorkPhase::Idle;
				}
			}
		}

		default:
			break;
		}
		return JobUpdateResult::Working;

	}
}