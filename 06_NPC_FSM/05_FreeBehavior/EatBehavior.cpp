#include"EatBehavior.h"
#include"FreeTimeBehaviorUtility.h"
#include"CharacterBase.h"
#include"CharacterUpdateContext.h"
#include"JobActionComponent.h"
#include"LocomotionSystem.h"
#include"StorageUtility.h"
#include"ItemInventory.h"
#include"WorldObjectSystem.h"
#include"StorageObject.h"
#include"Engine/IModelProvider.h"
#include"FbxLoader.h"


namespace Game::Character::FreeTime
{
	bool EatBehavior::CanInterruptByJob() const
	{
		return false;
	}

	int EatBehavior::GetPriority(CharacterBase& character, const CharacterUpdateContext& updateContext) const
	{
		return BehaviorPriority::Eat;
	}

	bool EatBehavior::CanStart(CharacterBase& character, const CharacterUpdateContext& updateContext) const
	{

		//食べ物を持つ倉庫があるかをここで確認する
		int foodNum = 1;
		std::wstring materialKey = L"food";
		auto storageLists = updateContext.gameServices.storageSystem.FindItemSources(materialKey, foodNum, World::Storage::StorageRole::General);


		return character.GetCharacterNeeds().hungerNeed.GetValue() <= 30.0f && !storageLists.empty();
	}

	void EatBehavior::Enter(CharacterBase& character)
	{
		auto& freeTimePhase = character.GetFreeTimeState().phase;
		freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
	}

	FreeTimeBehaviorUpdateResult EatBehavior::Update(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& freeTimePhase = character.GetFreeTimeState().phase;
		

		switch (freeTimePhase)
		{
			case FreeTimeBehaviorPhase::SelectingTarget:
			{
				int foodNum = 1;
				std::wstring materialKey = L"food";

				//一般倉庫リスト
				auto storageLists = updateContext.gameServices.storageSystem.FindItemSources(materialKey, foodNum, World::Storage::StorageRole::General);

				//キャラクターのトランスフォームID取得
				auto characterTid = character.GetFbxInstanceHandle().tid;
				//キャラクターID取得
				auto characterId = character.GetCharacterId();

				auto characterPos = updateContext.engineServices.transform->GetLocal(characterTid);
				if (!characterPos) {
					return FreeTimeBehaviorUpdateResult::Failed;
				}

				float bestDistance = FLT_MAX;
				const World::Storage::StorageItemSource* bestSource = nullptr;

				for (const auto& storageData : storageLists) {
					auto inventory = updateContext.gameServices.storageSystem.GetInventory(storageData.storageId);
					if (!inventory) {
						continue;
					}
					//倉庫IDから対応するトランスフォームIDを取得する
					auto storageTid = updateContext.gameServices.storageSystem.GetTransformId(storageData.storageId);


					auto posStorage = updateContext.engineServices.transform->GetLocal(storageTid);
					if (!posStorage) {
						ThrrowError("HaulerJob::Update : 倉庫が見つかりませんでした");
					}

					//最も近いものを探す
					float distance = action.Distance(characterPos->pos.xyz, posStorage->pos.xyz);

					if (distance < bestDistance) {
						bestDistance = distance;
						//ベストとして登録する
						bestSource = &storageData;

						auto itemData = inventory->Find(bestSource->itemInstanceId);
						if (!itemData) {
							ThrrowError("アイテムデータが見つかりませんでした");
						}
						//値の更新
						eatRequest_.position = posStorage->pos.xyz;
						eatRequest_.storageId = bestSource->storageId;
						eatRequest_.targetTid = bestSource->storageTid;
						eatRequest_.itemMasterKey = itemData->masterKey;
						eatRequest_.sourceStorageItemInstanceId = bestSource->itemInstanceId;
						eatRequest_.requestNum = foodNum;
					}

				}

				//予約するアイテム
				World::Storage::ReservedItem reservedItem(eatRequest_.storageId, eatRequest_.sourceStorageItemInstanceId, foodNum);

				//予約元は建築物
				auto reservationOwner = World::Storage::ReservationOwner::Character(characterId);
				const auto reservationResult = updateContext.gameServices.storageSystem.TryCreateReservation(materialKey, reservedItem, reservationOwner);
				currentReservedId_ = reservationResult.reservationId;


				if (reservationResult.status == World::Storage::ReservationCreateStatus::AlreadyExists || reservationResult.status == World::Storage::ReservationCreateStatus::Failed) {
					//既に予約が存在する場合
					freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;

					CancelCurrentReservation(freeTimePhase,updateContext.gameServices.storageSystem);
					//失敗
					return FreeTimeBehaviorUpdateResult::Failed;
				}

				//指定された倉庫を探す
				bool result = action.SelectTargetStorageById(character, updateContext, eatRequest_.storageId);
				if (result) {
					freeTimePhase = FreeTimeBehaviorPhase::PathFinding;

				}
				else {
					CancelCurrentReservation(freeTimePhase, updateContext.gameServices.storageSystem);
					return FreeTimeBehaviorUpdateResult::Failed;

				}

				return FreeTimeBehaviorUpdateResult::Running;
			}
			case FreeTimeBehaviorPhase::PathFinding:
			{
				//経路探索の処理 
				PathGoal goalInfo;
				goalInfo.type = PathGoalType::InteractObject;
				goalInfo.goalPosition = action.GetTargetInfo().targetPos;

				//ここでOターゲットとなるオブジェクトのOBBをゴールとしてセット
				auto isSucees = action.TrySetTargetCollisionToPathGoal(updateContext, goalInfo);
				if (!isSucees) {
					CancelCurrentReservation(freeTimePhase, updateContext.gameServices.storageSystem);
					return FreeTimeBehaviorUpdateResult::Failed;

				}

				//経路探索の処理
				bool result = action.BuildPathFromJobTarget(character, updateContext, goalInfo, PathQuery());
				if (result) {
					//ターゲットの選択に成功した場合
					freeTimePhase = FreeTimeBehaviorPhase::MovingToTarget;
				}
				else {
					//ターゲットの選択に失敗した場合
					action.ClearTarget();

					CancelCurrentReservation(freeTimePhase, updateContext.gameServices.storageSystem);
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
						//食べ物を引き出す状態へ遷移
						freeTimePhase = FreeTimeBehaviorPhase::WithdrawingFood;

						return FreeTimeBehaviorUpdateResult::Running;
					}
					//移動を続ける
					freeTimePhase = FreeTimeBehaviorPhase::MovingToTarget;
				}
				else {
					//ターゲットの選択に失敗した場合
					freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
				}

				return FreeTimeBehaviorUpdateResult::Running;
			}
			case FreeTimeBehaviorPhase::WithdrawingFood:
			{
				if (!currentReservedId_) {
					freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;
					return FreeTimeBehaviorUpdateResult::Failed;
				}

				//アイテムを引き出す
				if (!updateContext.gameServices.storageSystem.WithdrawReservedItems(*currentReservedId_, character.GetInventory())) {
					CancelCurrentReservation(freeTimePhase, updateContext.gameServices.storageSystem);
					return FreeTimeBehaviorUpdateResult::Failed;
				}

				updateContext.gameServices.storageSystem.CompleteReservation(*currentReservedId_);
				currentReservedId_.reset();

				freeTimePhase = FreeTimeBehaviorPhase::Eating;

				return FreeTimeBehaviorUpdateResult::Running;
			}
			case FreeTimeBehaviorPhase::Eating:
			{
				eatingElapsedTime_ += updateContext.simulationDeltaTime;

				if (eatingElapsedTime_ < EatingDuration) {
					freeTimePhase = FreeTimeBehaviorPhase::Eating;
					return FreeTimeBehaviorUpdateResult::Running;
				}
				auto& chInventory = character.GetInventory();
				auto itemData = chInventory.GetDataByKey(eatRequest_.itemMasterKey);
				if (!itemData) {
					ThrrowError("アイテムデータが見つかりませんでした");
				}
				//ここでアクセルするためのIDは、ソース元のアイテムのインスタンスIDではなく、アイテムを所有する方のインベントリから取得した物
				chInventory.ConsumeItem(itemData->instanceId, eatRequest_.requestNum);

				//食料一つ毎の空腹度回復ポイント
				float satisfyPoint = 50;

				character.GetCharacterNeeds().hungerNeed.Recover(satisfyPoint);

				return FreeTimeBehaviorUpdateResult::Completed;
			}
			default:
			{
				return FreeTimeBehaviorUpdateResult::Failed;
			}
		}

		return FreeTimeBehaviorUpdateResult::Running;
	}

	void EatBehavior::Exit(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		auto& freeTimePhase = character.GetFreeTimeState().phase;
		CancelCurrentReservation(freeTimePhase, updateContext.gameServices.storageSystem);
		freeTimePhase = FreeTimeBehaviorPhase::Idle;
	}

	void EatBehavior::CancelCurrentReservation(FreeTimeBehaviorPhase& freeTimePhase, World::Storage::StorageSystem& storageSystem)
	{
		if (currentReservedId_)
		{

			storageSystem.CancelReservation(*currentReservedId_);

			currentReservedId_.reset();
		}

		eatRequest_ = {};
		freeTimePhase = FreeTimeBehaviorPhase::SelectingTarget;

	}

}