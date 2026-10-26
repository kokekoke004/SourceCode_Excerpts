#include"FarmerJob.h"
#include"CharacterBase.h"
#include"JobActionComponent.h"
#include"NavigationSystem.h"
#include"LocomotionSystem.h"
#include"Engine/IModelProvider.h"
#include"FbxLoader.h"
#include"WorldObjectSystem.h"
#include"AgricultureSystem.h"

namespace Game::Character::Job
{

	FarmerJob::FarmerJob()
	{

	}

	FarmerJob::~FarmerJob() = default;

	void FarmerJob::Resume(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& jobPhase = character.GetJobState().phase;

		//移動系のフェーズでなければ再開処理は行わない
		if (jobPhase != JobWorkPhase::MovingToTarget && jobPhase != JobWorkPhase::PathFinding && jobPhase != JobWorkPhase::Farming && jobPhase != JobWorkPhase::Seeding &&
			jobPhase != JobWorkPhase::Harvesting) {
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

	void FarmerJob::Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext)
	{

	}

	int FarmerJob::CalculatePriority(Agriculture::CropGrowthStage stage) const
	{
		switch (stage)
		{
			case Agriculture::CropGrowthStage::Mature:
			{
				return 100; //収穫
			}

			case Agriculture::CropGrowthStage::Seedling:
			{
				return 80;  //種まき
			}

			case Agriculture::CropGrowthStage::Growing:
			{
				return 40;  //成長促進
			}

			default:
			{
				return 0;
			}
		}
	}

	//次は、建築家を実装していく。以下のコードはウッドカッターのもの。
	JobUpdateResult FarmerJob::Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		auto& action = character.GetJobActionComponent();
		auto jobPhase = character.GetJobState().phase;
		auto& agricultureSystem = updateContext.gameServices.agricultureSystem;



		switch (jobPhase)
		{
		case JobWorkPhase::Idle:
		{
			character.GetJobState().phase = JobWorkPhase::SelectingTarget;
			action.ClearTarget();
			farmerWorkState_.Reset();
			suspendedTarget_.reset();
			return JobUpdateResult::NoWork;
		}
		case JobWorkPhase::SelectingTarget:
		{

			const auto characterTid = character.GetFbxInstanceHandle().tid;

			const auto& components = agricultureSystem.GetAllFarmComponentData();

			std::vector<Engine::Components::TransformId> highestPriorityFields;
			int highestPriority = 0;

			for (const auto& component : components) {
				//成長過程を取得
				const auto growthStage = component.value.get()->GetCurrentGrowthStage();

				bool canWork = false;

				//各作業段階で作業が可能かどうかを確認する
				switch (growthStage)
				{
					case Agriculture::CropGrowthStage::Seedling:
					{
						canWork = agricultureSystem.CanAssignSower(component.key, characterTid);
						break;
					}
					case Agriculture::CropGrowthStage::Growing:
					{
						canWork = agricultureSystem.CanApplyGrowthBuff(component.key, characterTid);
						break;
					}
					case Agriculture::CropGrowthStage::Mature:
					{
						canWork = agricultureSystem.CanAssignHarvester(component.key, characterTid);
						break;
					}

				}

				if (!canWork) {
					continue;
				}

				//優先度の計算
				int score = CalculatePriority(growthStage);

				if (score > highestPriority) {
					highestPriority = score;

					//今までの候補は全て削除
					highestPriorityFields.clear();
					highestPriorityFields.push_back(component.key);
				}
				else if (score == highestPriority) {
					highestPriorityFields.push_back(component.key);
				}
			}

			const auto selectTarget = action.SelectNearestTarget(character, updateContext, highestPriorityFields,1000.0f,
				[&](Engine::Components::TransformId fieldTid)
				{
					return true;
				});
			if (selectTarget == std::nullopt) {
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;

				return JobUpdateResult::NoWork;

			}

			//取得したトランスフォームIDからワールド上のオブジェクトを検索
			const auto selectObj = updateContext.gameServices.worldObjectSystem.Find(*selectTarget);
			if (!selectObj) {
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;
				return JobUpdateResult::Working;

			}

			//トランスフォームIDからローカル座標を取得
			auto pos = updateContext.engineServices.transform->GetLocal(selectObj->tid);
			if (!pos) {
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;
				return JobUpdateResult::Working;

			}
			//ターゲットをここで指定する
			auto select = action.SelectTargetObject(selectObj->objectKey, selectObj->modelPath, selectObj->keyDrawName, pos->pos.xyz, selectObj->tid);

			if (!select) {
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;
				return JobUpdateResult::Working;
			}


			const auto targetField = action.GetTargetInfo();

			const auto growthStage = agricultureSystem.GetCurrentGrowthStage(targetField.targetTid);

			if (!growthStage) {
				character.GetJobState().phase = JobWorkPhase::SelectingTarget;
				return JobUpdateResult::Working;
			}

			switch (*growthStage)
			{
				case Agriculture::CropGrowthStage::Seedling:
				{
					if (!agricultureSystem.TryAssignSower(targetField.targetTid, character.GetFbxInstanceHandle().tid)) {
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						return JobUpdateResult::Working;

					}
					//作業フェーズは農作業
					farmerWorkState_.farmerPurpose = FarmerPurpose::Seeding;
					break;
				}
				case Agriculture::CropGrowthStage::Growing:
				{
					if (!agricultureSystem.TryAssignFarmer(targetField.targetTid, character.GetFbxInstanceHandle().tid)) {
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						return JobUpdateResult::Working;

					}

					//作業フェーズは農作業
					farmerWorkState_.farmerPurpose = FarmerPurpose::Farming;
					break;
				}
				case Agriculture::CropGrowthStage::Mature:
				{
					if (!agricultureSystem.TryAssignHarvester(targetField.targetTid, character.GetFbxInstanceHandle().tid)) {
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						return JobUpdateResult::Working;

					}
					//作業フェーズを収穫に
					farmerWorkState_.farmerPurpose = FarmerPurpose::Harvesting;
					break;
				}
				default:
				{
					character.GetJobState().phase = JobWorkPhase::SelectingTarget;
					return JobUpdateResult::Working;
				}
			}

			//ターゲットの選択に成功した場合
			character.GetJobState().phase = JobWorkPhase::PathFinding;
			//ここで一時的にターゲットを保存しておくことで経路探索中にターゲットが消えた場合に再度ターゲット選択からやり直すことができる
			suspendedTarget_ = action.GetTargetInfo();

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
				auto collider = model->Collider(colliderIdx);
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
				ReleaseCurrentWork(character, agricultureSystem);

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
				//目的地に到達した場合
				if (!state.followFlag && farmerWorkState_.farmerPurpose == FarmerPurpose::Seeding) {
					character.GetJobState().phase = JobWorkPhase::Seeding;
					return JobUpdateResult::Working;

				}
				else if (!state.followFlag && farmerWorkState_.farmerPurpose == FarmerPurpose::Farming) {
					character.GetJobState().phase = JobWorkPhase::Farming;
					return JobUpdateResult::Working;
				}
				else if (!state.followFlag && farmerWorkState_.farmerPurpose == FarmerPurpose::Harvesting) {
					character.GetJobState().phase = JobWorkPhase::Harvesting;
					return JobUpdateResult::Working;

				}


				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				ReleaseCurrentWork(character, agricultureSystem);

				character.GetJobState().phase = JobWorkPhase::SelectingTarget;
				return JobUpdateResult::Working;
			}

			break;
		}
		case JobWorkPhase::Seeding:
		{
			const auto targetField = action.GetTargetInfo();

			//作物の種をまく
			bool resultWork = agricultureSystem.ApplySowingWork(targetField.targetTid, character.GetFbxInstanceHandle().tid, updateContext.simulationDeltaTime);
			if (resultWork) {
				character.GetJobState().phase = JobWorkPhase::Seeding;

			}
			else {
				character.GetJobState().phase = JobWorkPhase::Idle;
				agricultureSystem.ReleaseSower(targetField.targetTid, character.GetFbxInstanceHandle().tid);
			}


			break;
		}

		case JobWorkPhase::Farming:
		{
			//成長バフをかける
			farmerWorkState_.workElapsedTime += updateContext.frame.time.deltaTime;
			if (farmerWorkState_.IsWorkFinished()) {
				const auto targetField = action.GetTargetInfo();
				//バフ適用
				updateContext.gameServices.agricultureSystem.ApplyGrowthBuff(targetField.targetTid, character.GetFbxInstanceHandle().tid, 1.5f, 30.0f);
				//登録解除
				updateContext.gameServices.agricultureSystem.ReleaseFarmer(targetField.targetTid, character.GetFbxInstanceHandle().tid);
				character.GetJobState().phase = JobWorkPhase::Idle;

			}
			break;
		}
		case JobWorkPhase::Harvesting:
		{
			const auto targetField = action.GetTargetInfo();

			//作物を収穫する
			bool resultWork = agricultureSystem.ApplyHarvestWork(targetField.targetTid, character.GetFbxInstanceHandle().tid, updateContext.simulationDeltaTime);
			if (resultWork) {
				character.GetJobState().phase = JobWorkPhase::Harvesting;

			}
			else {
				character.GetJobState().phase = JobWorkPhase::Idle;
				agricultureSystem.ReleaseHarvester(targetField.targetTid, character.GetFbxInstanceHandle().tid);
			}

			break;
		}

		default:
			break;
		}

		return JobUpdateResult::Working;

	}

	void FarmerJob::ReleaseCurrentWork(CharacterBase& character, Agriculture::AgricultureSystem& agricultureSystem)
	{
		auto& action = character.GetJobActionComponent();
		const auto targetField = action.GetTargetInfo();


		const auto characterTid = character.GetFbxInstanceHandle().tid;

		//仕事のフローに応じて初期化
		switch (farmerWorkState_.farmerPurpose)
		{
			case FarmerPurpose::Seeding:
			{
				agricultureSystem.ReleaseSower(targetField.targetTid, characterTid);
				break;
			}
			case FarmerPurpose::Farming:
			{
				agricultureSystem.ReleaseFarmer(targetField.targetTid, characterTid);
				break;
			}
			case FarmerPurpose::Harvesting:
			{
				agricultureSystem.ReleaseHarvester(targetField.targetTid, characterTid);
				break;
			}
			default:
			{
				break;
			}
		}

		farmerWorkState_.Reset();
		action.ClearTarget();
	}

}