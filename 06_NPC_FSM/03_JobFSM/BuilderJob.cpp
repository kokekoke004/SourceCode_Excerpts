#include"BuilderJob.h"
#include"CharacterBase.h"
#include"JobActionComponent.h"
#include"NavigationSystem.h"
#include"LocomotionSystem.h"
#include"Engine/IModelProvider.h"
#include"FbxLoader.h"


namespace Game::Character::Job
{

	BuilderJob::BuilderJob()
	{

	}

	BuilderJob::~BuilderJob() = default;

	void BuilderJob::Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext)
	{

	}

	void BuilderJob::Resume(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& jobPhase = character.GetJobState().phase;

		//移動系のフェーズでなければ再開処理は行わない
		if (jobPhase != JobWorkPhase::MovingToTarget && jobPhase != JobWorkPhase::PathFinding && jobPhase != JobWorkPhase::Building) {
			return;
		}

		//保存されたターゲットがなければ何もしない
		if (!suspendedTarget_) {
			return;
		}

		//ターゲット情報を復元
		action.SetTargetInfo(*suspendedTarget_);

		//経路探索から再開
		jobPhase = JobWorkPhase::PathFinding;

	}

	//次は、建築家を実装していく。以下のコードはウッドカッターのもの。
	JobUpdateResult BuilderJob::Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
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
			const auto nearestBuild = FindNearestBuildingAwaitingConstruction(character, updateContext);
			if (!nearestBuild) {
				character.GetJobState().phase = JobWorkPhase::Idle;
				return JobUpdateResult::NoWork;
			}
			selectBuildProgressData_ = *nearestBuild;

			const auto& placementData = nearestBuild->placementData;

			//ターゲットオブジェクトを直接指定する
			const auto select = action.SelectTargetObject(placementData.buildKey, placementData.modelName, placementData.drawKey, placementData.position, placementData.instanceHandle.tid);


			if (select) {
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::PathFinding;
				suspendedTarget_ = action.GetTargetInfo();
			}
			else {
				//ターゲットの選択に失敗した場合
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;

			}
			break;
		}
		case JobWorkPhase::PathFinding:
		{
			//経路探索の処理 
			PathGoal goalInfo;
			goalInfo.type = PathGoalType::InteractObject;
			goalInfo.goalPosition = action.GetTargetInfo().targetPos;



			const auto& modelPath = action.GetTargetInfo().modelPath;

			if (modelPath.empty()) {
				//モデルパスが空の場合は、経路探索に失敗とみなしてIdleに戻る
				character.GetJobState().phase = JobWorkPhase::Idle;
				return JobUpdateResult::Working;
			}
			std::wstring keyDrawName = action.GetTargetInfo().drawKey;
			const auto& model = updateContext.engineServices.modelProvider->GetFromFbxName(modelPath);
			int idx = model->GetfbxKeysIndex(keyDrawName);
			auto fbxPos = updateContext.engineServices.fbxInstanceRegistry->TryGetInstance(modelPath, idx);
			if (!fbxPos) {
				//fbxPosが空の場合は、経路探索に失敗とみなしてIdleに戻る
				character.GetJobState().phase = JobWorkPhase::Idle;
				return JobUpdateResult::Working;
			}
			//コリジョンで全体を覆っているものを指定するようにした
			for (const auto& colliderKey : fbxPos->collsionNameKey) {
				const auto& colliderIdx = model->ColliderIdx(colliderKey.nameKey);
				auto& collider = model->Collider(colliderIdx);
				if (model->Collider(colliderIdx).collisionType == CollisionType::WHOLE_OBJECT) {
					goalInfo.collision = &collider;
					break;
				}
			}

			//経路探索の処理
			bool result = action.BuildPathFromJobTarget(character, updateContext, goalInfo, PathQuery());
			if (result) {
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				action.ClearTarget();

				//別ターゲットを探し直す
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;
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
					character.GetJobState().phase = JobWorkPhase::Building;
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

		case JobWorkPhase::Building:
		{
			const auto& chTid = character.GetFbxInstanceHandle().tid;
			const float ConstructionInterval = 1.0f;
			const int ConstructionWorkPerInterval = 1;
			//建築タイマーを進める
			constructionTimer_ += updateContext.simulationDeltaTime;

			//デルタタイムが長くなった時にも、しっかり秒数分の建築進捗加えるため
			while (constructionTimer_ >= ConstructionInterval) {
				const auto& queueResult = updateContext.gameServices.buildProgressManager.QueueConstructionWork(selectBuildProgressData_.placementData.buildingId, 
					chTid, ConstructionWorkPerInterval);

				if (queueResult == Build::QueueConstructionWorkResult::Queued) {
					//建築処理に成功した場合
					character.GetJobState().phase = JobWorkPhase::Building;
				}
				else {
					//建築処理失敗、または完了した場合
					character.GetJobState().phase = JobWorkPhase::Idle;
					action.ClearTarget();
					constructionTimer_ = 0;
					selectBuildProgressData_ = {};
					return JobUpdateResult::Working;
				}

				constructionTimer_ -= ConstructionInterval;
			}
			break;

		}

		default:
			break;
		}
		return JobUpdateResult::Working;

	}

	std::optional <Game::Build::BuildProgressData> BuilderJob::FindNearestBuildingAwaitingConstruction(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		auto targetList = updateContext.gameServices.buildProgressManager.CollectBuildingsAwaitingConstruction();
		auto& action = character.GetJobActionComponent();

		//ここで、到達不可能リストに入っていたらターゲットから削除する
		//remove_ifで削除対象を末尾側へ移動し、返された新しい末尾位置から
		//vector末尾までをeraseで実際に削除する
		targetList.erase(std::remove_if(targetList.begin(), targetList.end(),
			[&](const auto& build) {
				return action.IsUnreachableTarget(build.placementData.instanceHandle.tid);
			}), targetList.end());


		if (targetList.empty()){
			return std::nullopt;
		}


		const auto chId = character.GetFbxInstanceHandle();
		auto chPos = updateContext.engineServices.transform->GetLocal(chId.tid);

		//距離が近い順番で並び替える
		const auto nearestIt = std::min_element(targetList.begin(), targetList.end(),
			[&](const auto& a, const auto& b) {
				return action.Distance(chPos->pos.xyz, a.placementData.position) < action.Distance(chPos->pos.xyz, b.placementData.position);
			});

		if (nearestIt == targetList.end()) {
			return std::nullopt;
		}

		return *nearestIt;
	}


}