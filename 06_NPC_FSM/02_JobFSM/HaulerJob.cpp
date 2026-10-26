#include"HaulerJob.h"
#include"CharacterBase.h"
#include"JobActionComponent.h"
#include"NavigationSystem.h"
#include"LocomotionSystem.h"
#include"WorldObjectSystem.h"
#include"WorldItemDropManager.h"
#include"Engine/IModelProvider.h"
#include"FbxLoader.h"
#include"ColliderComponent.h"	
#include"ItemInventory.h"
#include"BuildPlacementController.h"
#include"Engine/Model/FbxInstanceRegistry.h"

#include<limits>
#include<DirectXMath.h>
#include<unordered_set>
#include"StorageUtility.h"
#include"StorageObject.h"
#include"ItemSystem.h"
#include"ItemDropManager.h"
namespace Game::Character::Job
{
	HaulerJob::HaulerJob() = default;

	void HaulerJob::Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		CancelCurrentReservation(character, updateContext);
		DropCarriedItems(character, updateContext);
	}

	void HaulerJob::Resume(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto& jobPhase = character.GetJobState().phase;

		//移動系のフェーズでなければ再開処理は行わない
		if (jobPhase != JobWorkPhase::MovingToTarget && jobPhase != JobWorkPhase::PathFinding){
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


	int HaulerJob::CalculatePriority(const HaulingRequest& request) const
	{
		switch (request.type)
		{
		case HaulingRequestType::BuildingSupply:
			return 100;

		case HaulingRequestType::ProductionOutput:
			return 60;

		case HaulingRequestType::DroppedItemPickup:
			return 50;

		}

		return 0;
	}

	JobUpdateResult HaulerJob::Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		//運搬職の行動パターン設計
		auto& action = character.GetJobActionComponent();
		auto jobPhase = character.GetJobState().phase;
		auto& worldItemDropManager = updateContext.gameServices.worldItemDropManager;
		auto playerTid = character.GetFbxInstanceHandle().tid;

		switch (jobPhase)
		{
		case JobWorkPhase::Idle:
		{
			character.GetJobState().phase = JobWorkPhase::SelectingTarget;
			//ターゲット倉庫初期化
			targetSourceStorageId_ = std::nullopt;
			targetDroppedItem_ = {};
			suspendedTarget_.reset();
			return JobUpdateResult::NoWork;
		}
		case JobWorkPhase::SelectingTarget:
		{
			std::vector<HaulingRequest> requests;

			//建築資材運搬の候補
			const auto selectBuild = SelectingBuildingObject(character, updateContext);

			const auto& transform = updateContext.engineServices.transform;

			//キャラクターの座標取得
			const auto characterTransform = updateContext.engineServices.transform->GetLocal(playerTid);
			if (!characterTransform) {
				ThrrowError("HaulerJob::Update : キャラクターのトランスフォームIDが割り当てられていません");
			}

			//建築物の候補
			if (selectBuild)
			{
				for (const auto& progress : *selectBuild)
				{
					
					HaulingRequest request;
					request.type = HaulingRequestType::BuildingSupply;

					request.priority = CalculatePriority(request);


					//指定したアイテムを取り出すことが出来る倉庫を探す
					const auto sourceStorage= updateContext.gameServices.storageSystem.FindItemSources(progress.materialKey, progress.shortageAmount,World::Storage::StorageRole::General);

					const World::Storage::StorageItemSource* bestSource = nullptr;
					float bestDistance = FLT_MAX;

					//見つかった倉庫の中で最も近いものを選ぶ
					for (const auto& source : sourceStorage) {
						
						auto posStorage = updateContext.engineServices.transform->GetLocal(source.storageTid);
						if (!posStorage) {
							ThrrowError("HaulerJob::Update : 倉庫が見つかりませんでした");
						}

						float distance = action.Distance(characterTransform->pos.xyz, posStorage->pos.xyz);

						if (distance < bestDistance) {
							bestDistance = distance;

							bestSource = &source;
						}
					}

					if (!bestSource) {
						continue;
					}

					const int32_t haulAmount = std::min<int32_t>(progress.shortageAmount, bestSource->availableAmount);

					//必要な建築資材のセット
					request.buildMaterialRequest = Build::BuildMaterialRequest(progress.buildId,progress.position,progress.materialKey,haulAmount);


					request.storageId = bestSource->storageId;
					request.sourceItemInstanceId = bestSource->itemInstanceId;

					const auto sourceTransform = updateContext.engineServices.transform->GetLocal(bestSource->storageTid);

					if (!sourceTransform) {
						continue;
					}
					request.position = sourceTransform->pos.xyz;


					requests.push_back(request);
				}
			}

			//ドロップアイテム回収の候補
			const auto drops = worldItemDropManager.GetAll();

			for (const auto& drop : drops)
			{
				HaulingRequest request;
				request.type = HaulingRequestType::DroppedItemPickup;
				request.position = drop.value.pos;

				request.priority = CalculatePriority(request);
				//アイテムのIDをここで渡す
				request.droppedItem = drop.key;
				requests.push_back(request);
			}

			//生産したアイテムの候補
			auto outputProducts = updateContext.gameServices.storageSystem.GetStorages(World::Storage::StorageRole::ProductionOutput);
			
			for (const auto& outputProduct : outputProducts) {

				if (!outputProduct.storage) {
					continue;
				}

				auto& inventory = outputProduct.storage->GetInventory();

				//搬出可能なアイテムを探す
				for (const auto& entry : inventory.GetAllData()) {
					const auto& item = entry.value;

					if (item.stackCount <= 0) {
						continue;
					}

					//アイテムのスタック数が0以外のものを運ぶ
					HaulingRequest request;
					request.type = HaulingRequestType::ProductionOutput;

					auto optTid = outputProduct.storage->GetTransformId();
					request.position = transform->GetLocal(optTid)->pos.xyz;
					request.priority = CalculatePriority(request);
					request.storageId = outputProduct.storageId;
					request.sourceItemInstanceId = item.instanceId;


					const int32_t availableAmount = updateContext.gameServices.storageSystem.GetAvailableAmount(outputProduct.storageId, item.instanceId);

					if (availableAmount <= 0) {
						continue;
					}

					ProductionOutputRequest outputReq;
					outputReq.amount = availableAmount;
					outputReq.itemKey = item.masterKey;

					request.productionOutputRequest = outputReq;

					requests.push_back(request);

				}


			}


			if (requests.empty())
			{
				character.GetJobState().phase = JobWorkPhase::Idle;

				return JobUpdateResult::NoWork;
			}


			const HaulingRequest* bestRequest = nullptr;
			int bestPriority = INT_MIN;
			float bestDistance = FLT_MAX;

			for (const auto& request : requests) {
				float distance = action.Distance(characterTransform->pos.xyz, request.position);

				if ( request.priority > bestPriority || request.priority == bestPriority && distance < bestDistance) {
					bestPriority = request.priority;
					bestDistance = distance;

					bestRequest = &request;
				}
			}

			if (!bestRequest)
			{
				character.GetJobState().phase = JobWorkPhase::Idle;
				return JobUpdateResult::NoWork;
			}

			switch (bestRequest->type)
			{
				case HaulingRequestType::BuildingSupply:
				{
					//どれか一つでも初期化されていなければターゲット選択からやり直し
					if(!bestRequest->storageId || !bestRequest->sourceItemInstanceId || !bestRequest->buildMaterialRequest) {
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						break;
					}

					const auto& buildRequest = *bestRequest->buildMaterialRequest;

					//予約するアイテム
					World::Storage::ReservedItem reservedItem(*bestRequest->storageId, *bestRequest->sourceItemInstanceId, buildRequest.shortageAmount);

					//予約元は建築物
					auto reservationOwner = World::Storage::ReservationOwner::Build(buildRequest.buildId);

					const auto reservationResult = updateContext.gameServices.storageSystem.TryCreateReservation(buildRequest.materialKey, reservedItem, reservationOwner);

					if (reservationResult.status == World::Storage::ReservationCreateStatus::Created) {
						//建築側に資材の予約状況を伝える
						const bool reserveResult = updateContext.gameServices.buildProgressManager.AddMaterialReservation(buildRequest.buildId, buildRequest.materialKey, buildRequest.shortageAmount);

						if (!reserveResult) {
							if (reservationResult.reservationId) {
								updateContext.gameServices.storageSystem.CancelReservation(*reservationResult.reservationId);
							}

							character.GetJobState().phase = JobWorkPhase::SelectingTarget;
							break;
						}

						//予約IDを保存
						currentReservedId_ = *reservationResult.reservationId;
						hasBuildReservation_ = true;
						//後で建築物へ届けるため保存
						targetBuildMaterialRequest = buildRequest;

						//次に向かう倉庫を保存
						targetSourceStorageId_ = *bestRequest->storageId;

						haulTargetPurpose_ = HaulTargetPurpose::WithdrawReservedItem;

						character.GetJobState().phase = JobWorkPhase::SelectingSourceStorage;
					}
					else if (reservationResult.status == World::Storage::ReservationCreateStatus::AlreadyExists) {
						printf("建築資材の予約が既に存在しています buildId=%u\n", buildRequest.buildId);

						//Idleに戻して無限ループを直す
						character.GetJobState().phase = JobWorkPhase::Idle;

					}
					else {
						//既に予約済みであればもう一回候補探しから
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;

					}
					break;
				}
				case HaulingRequestType::DroppedItemPickup:
				{
					if (!bestRequest->droppedItem) {
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						break;
					}

					const auto droppedItem = worldItemDropManager.GetData(*bestRequest->droppedItem);
					if (!droppedItem) {
						//他のNPCに拾われた等
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						break;
					}
					//拾うアイテムをターゲットとしてセットする
					targetDroppedItem_ = *droppedItem;

					//移動先をドロップ位置に設定
					action.SelectTargetPosition(droppedItem->pos);

					//ここで一時的にターゲットを保存しておくことで、経路探索中にターゲットが消えた場合に再度ターゲット選択からやり直すことができる
					suspendedTarget_ = action.GetTargetInfo();


					haulTargetPurpose_ = HaulTargetPurpose::PickupDroppedItem;

					character.GetJobState().phase = JobWorkPhase::PathFinding;

					break;
				}
				case HaulingRequestType::ProductionOutput:
				{
					if (!bestRequest->storageId || !bestRequest->sourceItemInstanceId)
					{
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
						break;
					}

					//予約するアイテム
					World::Storage::ReservedItem reservedItem(*bestRequest->storageId, *bestRequest->sourceItemInstanceId, bestRequest->productionOutputRequest->amount);

					//予約元はキャラクターのトランスフォームID
					auto reservationOwner = World::Storage::ReservationOwner::Hauler(playerTid);

					const auto reservationResult = updateContext.gameServices.storageSystem.TryCreateReservation(bestRequest->productionOutputRequest->itemKey, reservedItem, reservationOwner);

					if (reservationResult.status == World::Storage::ReservationCreateStatus::Created && reservationResult.reservationId) {
						currentReservedId_ = *reservationResult.reservationId;

						//取りに行くOutput倉庫
						targetSourceStorageId_ = *bestRequest->storageId;

						haulTargetPurpose_ = HaulTargetPurpose::WithdrawProductionOutput;

						character.GetJobState().phase = JobWorkPhase::SelectingSourceStorage;
					}
					else
					{
						character.GetJobState().phase = JobWorkPhase::SelectingTarget;
					}

					break;
				}
			}
			break;
		}
		case JobWorkPhase::SelectingSourceStorage:
		{
			
			//指定された倉庫を探す
			bool result = action.SelectTargetStorageById(character, updateContext, *targetSourceStorageId_);
			if (result) {
				character.GetJobState().phase = JobWorkPhase::PathFinding;
				//ここで一時的にターゲットを保存しておくことで、経路探索中にターゲットが消えた場合に再度ターゲット選択からやり直すことができる
				suspendedTarget_ = action.GetTargetInfo();

			}
			else {
				CancelCurrentReservation(character, updateContext);
				character.GetJobState().phase = JobWorkPhase::Idle;
			}
			break;

		}

		case JobWorkPhase::PathFinding:
		{
			//経路探索の処理
			PathGoal goalInfo;
			goalInfo.type = PathGoalType::InteractObject;
			goalInfo.goalPosition = action.GetTargetInfo().targetPos;

			//経路探索の目的地が倉庫または、建築予定地への運搬中であれば、目的地のコライダーを経路探索のゴール情報に追加する
			if(haulTargetPurpose_ == HaulTargetPurpose::DepositStorage || haulTargetPurpose_ == HaulTargetPurpose::DeliverToConstruction
				|| haulTargetPurpose_ == HaulTargetPurpose::WithdrawReservedItem || haulTargetPurpose_ == HaulTargetPurpose::WithdrawProductionOutput)
			{
				const auto& modelPath = action.GetTargetInfo().modelPath;

				if (modelPath.empty()) {
					//モデルパスが空の場合は、経路探索に失敗とみなしてIdleに戻る
					CancelCurrentReservation(character, updateContext);
					return JobUpdateResult::Working;
				}
				std::wstring keyDrawName = action.GetTargetInfo().drawKey;
				const auto& model = updateContext.engineServices.modelProvider->GetFromFbxName(modelPath);
				int idx = model->GetfbxKeysIndex(keyDrawName);
				auto fbxPos = updateContext.engineServices.fbxInstanceRegistry->TryGetInstance(modelPath,idx);
				if (!fbxPos) {
					//fbxPosが空の場合は、経路探索に失敗とみなしてIdleに戻る
					CancelCurrentReservation(character, updateContext);
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
			}

			if (haulTargetPurpose_ == HaulTargetPurpose::PickupDroppedItem)
			{
				goalInfo.interactRange = 0.0f;
			}

			bool result = action.BuildPathFromJobTarget(character, updateContext, goalInfo, PathQuery());
			if (result) {
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				action.ClearTarget();
				CancelCurrentReservation(character, updateContext);


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
					if (haulTargetPurpose_ == HaulTargetPurpose::PickupDroppedItem) {
						character.GetJobState().phase = JobWorkPhase::PickingUp;
						haulTargetPurpose_ = HaulTargetPurpose::None; //目的をリセット

						return JobUpdateResult::Working;
					}
					else if (haulTargetPurpose_ == HaulTargetPurpose::DepositStorage) {
						character.GetJobState().phase = JobWorkPhase::Depositing;
						haulTargetPurpose_ = HaulTargetPurpose::None; //目的をリセット
						return JobUpdateResult::Working;
					}
					else if (haulTargetPurpose_ == HaulTargetPurpose::WithdrawReservedItem || haulTargetPurpose_ == HaulTargetPurpose::WithdrawProductionOutput) {
						character.GetJobState().phase = JobWorkPhase::WithdrawingReservedItem;
						return JobUpdateResult::Working;
					}
					else if (haulTargetPurpose_ == HaulTargetPurpose::DeliverToConstruction) {
						character.GetJobState().phase = JobWorkPhase::SupplyingBuilding;
						return JobUpdateResult::Working;
					}


				}
				//ターゲットの選択に成功した場合
				character.GetJobState().phase = JobWorkPhase::MovingToTarget;
			}
			else {
				//ターゲットの選択に失敗した場合
				int h = 0;
			}

			break;
		}
		case JobWorkPhase::PickingUp:
		{
			//Pickupの処理
			//Pickupに成功したらIdleに戻る
			bool result = action.Pickup(character,updateContext,targetDroppedItem_);
			if (result) {
				character.GetJobState().phase = JobWorkPhase::SelectingDepositTarget;
			}
			else {
				//Pickupに失敗した場合
				character.GetJobState().phase = JobWorkPhase::Idle;
			}
			break;
		}
		case JobWorkPhase::SelectingDepositTarget:
		{
			//汎用倉庫を全て挙げる
			auto storageEntries = updateContext.gameServices.storageSystem.GetStorages(World::Storage::StorageRole::General);
			if (storageEntries.empty()) {
				CancelCurrentReservation(character, updateContext);
				break;
			}
			
			std::vector<Engine::Components::TransformId> targetList;
			targetList.reserve(storageEntries.size());

			//ここでトランスフォームIDのみを抽出する
			for (const auto& entry : storageEntries)
			{
				if (!entry.storage) {
					continue;
				}

				targetList.push_back(entry.storage->GetTransformId());
			}

			//挙げた倉庫のうち最も近いものを選ぶ
			const auto targetTid = action.SelectNearestTarget(character, updateContext, targetList, 100.0f,
				[](Engine::Components::TransformId)
				{
					return true;
				});

			if (!targetTid) {
				CancelCurrentReservation(character, updateContext);
				break;
			}
			//トランスフォームIDから倉庫を取得
			auto storageId = updateContext.gameServices.storageSystem.GetStorageIdFromTid(*targetTid);
			if (!storageId) {
				CancelCurrentReservation(character, updateContext);
				break;
			}

			//Depositの処理
			//Depositに成功したらIdleに戻る
			bool result = action.SelectTargetStorageById(character, updateContext, *storageId);

			if (result) {
				haulTargetPurpose_ = HaulTargetPurpose::DepositStorage; //目的を倉庫へのデポジットに設定
				character.GetJobState().phase = JobWorkPhase::PathFinding;
				//ここで一時的にターゲットを保存しておくことで、経路探索中にターゲットが消えた場合に再度ターゲット選択からやり直すことができる
				suspendedTarget_ = action.GetTargetInfo();

			}
			else {
				//Depositに失敗した場合
				CancelCurrentReservation(character, updateContext);
			}
			break;
		}
		case JobWorkPhase::Depositing:
		{
			bool result = action.Deposit(character, updateContext);
			if (result) {
				if (currentReservedId_) {
					updateContext.gameServices.storageSystem.CompleteReservation(*currentReservedId_);
					currentReservedId_.reset();

				}
				itemsWithdrawn_ = false;
				character.GetJobState().phase = JobWorkPhase::Idle;
				haulTargetPurpose_ = HaulTargetPurpose::None; //目的をリセット
			}
			else {
				//Depositに失敗した場合
				CancelCurrentReservation(character, updateContext);
			}
			break;
		}
		//アイテムを倉庫から取り出す
		case JobWorkPhase::WithdrawingReservedItem:
		{
			//ここでアイテムを倉庫から取り出す
			if (updateContext.gameServices.storageSystem.WithdrawReservedItems(*currentReservedId_, character.GetInventory())) {
				itemsWithdrawn_ = true;

				//生産物だった場合
				if (haulTargetPurpose_ == HaulTargetPurpose::WithdrawProductionOutput)
				{
					character.GetJobState().phase = JobWorkPhase::SelectingDepositTarget;
					break;
				}

				if (haulTargetPurpose_ == HaulTargetPurpose::WithdrawReservedItem)
				{
					//アイテムを取り出すことが出来たら次は、建築予定地へ資材運搬する
					character.GetJobState().phase = JobWorkPhase::SelectingBuildingTarget;

					haulTargetPurpose_ = HaulTargetPurpose::DeliverToConstruction; //建築現場に供給に切り替え
					break;
				}


			}
			else {
				character.GetJobState().phase = JobWorkPhase::Idle;
				CancelCurrentReservation(character, updateContext);

			}
			break;
		}
		case JobWorkPhase::SelectingBuildingTarget:
		{
			//建築IDから建築データ取得
			const auto buildData = updateContext.gameServices.buildProgressManager.FindBuilding(targetBuildMaterialRequest.buildId);
			if (!buildData) {
				CancelCurrentReservation(character, updateContext);
				return JobUpdateResult::Working;
			}
			
			bool result = action.SelectTargetObject(buildData->placementData.buildKey, buildData->placementData.modelName, buildData->placementData.drawKey, buildData->placementData.position, buildData->placementData.instanceHandle.tid);
			//ここで一時的にターゲットを保存しておくことで、経路探索中にターゲットが消えた場合に再度ターゲット選択からやり直すことができる
			suspendedTarget_ = action.GetTargetInfo();

			if (!result) {
				CancelCurrentReservation(character, updateContext);
			}
			else {
				character.GetJobState().phase = JobWorkPhase::PathFinding;
			}



			break;
		}
		case JobWorkPhase::SupplyingBuilding:
		{

			auto& chInv = character.GetInventory();
			const auto& material = chInv.GetDataByKey(targetBuildMaterialRequest.materialKey);
			if (!material) {
				CancelCurrentReservation(character, updateContext);
				return JobUpdateResult::Working;
			}

			const int32_t deliveryAmount = targetBuildMaterialRequest.shortageAmount;

			Build::BuildMaterialAmount buildMaterialAmount(targetBuildMaterialRequest.materialKey, deliveryAmount);

			//キャラクターのインベントリ内のアイテムは減らす
			if (!chInv.ConsumeItem(material->instanceId, deliveryAmount)) {
				printf("アイテムをキャラクターのインベントリから減らす処理失敗しました。\n");
			}
			//ここでアイテムを建築に消費する
			const bool deliveryResult = updateContext.gameServices.buildProgressManager.CompleteMaterialDelivery(targetBuildMaterialRequest.buildId, targetBuildMaterialRequest.materialKey, deliveryAmount);
			
			if (!deliveryResult) {
				printf("建築資材の納品進捗更新に失敗しました。\n");
				CancelCurrentReservation(character, updateContext);
				return JobUpdateResult::Working;
			}

			//ここで対象の予約IDを完了とする
			updateContext.gameServices.storageSystem.CompleteReservation(*currentReservedId_);
			//ここで変数類を初期化
			targetBuildMaterialRequest = {};
			currentReservedId_.reset();
			hasBuildReservation_ = false;
			itemsWithdrawn_ = false;
			character.GetJobState().phase = JobWorkPhase::Idle;
			haulTargetPurpose_ = HaulTargetPurpose::None; //目的をリセット
			PrintWstring(L"建築資材" + material->masterKey + L"を" + std::to_wstring(deliveryAmount) + L"個消費しました。\n");

			break;
		}

		default:
			break;
		}

		return JobUpdateResult::Working;

	}

	std::optional <std::vector< Game::Build::BuildMaterialRequest >> HaulerJob::SelectingBuildingObject(Game::Character::CharacterBase & character, const Game::Character::CharacterUpdateContext & updateContext)
	{
		auto materialRequestData = updateContext.gameServices.buildProgressManager.CollectMaterialRequests();
		if (materialRequestData.empty()) {
			return std::nullopt;
		}
		auto& action = character.GetJobActionComponent();

		const auto chId = character.GetFbxInstanceHandle();
		auto chPos = updateContext.engineServices.transform->GetLocal(chId.tid);

		//距離が近い順番で並び替える
		std::sort(materialRequestData.begin(), materialRequestData.end(),
			[&](const auto& a, const auto& b) {
				return action.Distance(chPos->pos.xyz, a.position) < action.Distance(chPos->pos.xyz, b.position);
		});

		std::vector <Game::Build::BuildMaterialRequest> requestList;
		std::unordered_set<Game::Build::BuildingId> checkBuildIds;


		for (const auto& reqData : materialRequestData) {
			const auto buildId = reqData.buildId;

			//secondに追加できたかどうかが入る。初回はtrue/その後追加するとfalse
			if (!checkBuildIds.insert(buildId).second) {
				continue;
			}

			for (const auto& materialReqData : materialRequestData) {
				if (materialReqData.buildId != buildId) {
					continue;
				}

				const auto sources = updateContext.gameServices.storageSystem.FindItemSources(materialReqData.materialKey, materialReqData.shortageAmount, World::Storage::StorageRole::General);

				if (!sources.empty()) {
					requestList.push_back(materialReqData);
				}
			}

			if (!requestList.empty()) {
				return requestList;
			}

		}

		return std::nullopt;
	}

	void HaulerJob::CancelCurrentReservation(CharacterBase& character, const CharacterUpdateContext& updateContext)
	{
		auto& storage = updateContext.gameServices.storageSystem;
		auto& build = updateContext.gameServices.buildProgressManager;

		if (hasBuildReservation_) {
			build.CancelMaterialReservation(targetBuildMaterialRequest.buildId, targetBuildMaterialRequest.materialKey, targetBuildMaterialRequest.shortageAmount);
			hasBuildReservation_ = false;
		}

		if (currentReservedId_) {
			if (itemsWithdrawn_) {
				//取り出し済みなら、予約は完了扱いにして手持ちをその場にドロップする
				//DroppedItemPickup として誰かが回収し直す
				storage.CompleteReservation(*currentReservedId_);
				DropCarriedItems(character, updateContext);
			}
			else {
				storage.CancelReservation(*currentReservedId_);
			}
			currentReservedId_.reset();
		}

		itemsWithdrawn_ = false;
		targetSourceStorageId_.reset();
		targetBuildMaterialRequest = {};
		haulTargetPurpose_ = HaulTargetPurpose::None;
		suspendedTarget_.reset();
		character.GetJobState().phase = JobWorkPhase::Idle;
	}

	void HaulerJob::DropCarriedItems(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		auto& inventory = character.GetInventory();
		auto itemData = inventory.GetAllData();
		auto chFI = character.GetFbxInstanceHandle();
		auto chPos = updateContext.engineServices.transform->GetLocal(chFI.tid);
		if (chPos == nullptr) {
			ThrrowError("キャラクターの座標が見つかりませんでした。");
		}
		for (const auto& item : itemData) {
			for (int i = 0; i < item.value.stackCount; i++) {
				Item::Drop::Manager::DropRequest req(item.value.masterKey, chPos->pos.xyz,Item::Drop::Manager::DropRequestType::DirectItem);
				updateContext.gameServices.itemSystem.RequstItemDrop(req);
			}
			inventory.ConsumeItem(item.value.instanceId, item.value.stackCount);

		}

	}
}