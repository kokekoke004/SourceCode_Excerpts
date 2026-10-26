#include"JobActionComponent.h"
#include <random>
#include"CharacterBase.h"
#include"CharacterUpdateContext.h"
#include"Engine/EngineAPI.h"
#include"Ray.h"
#include"PlayerClickAction.h"
#include"CombatSystem.h"
#include"helper.h"
#include"TransformSystem.h"
#include"UiManager.h"
#include"WorldObjectSystem.h"
#include"WorldItemDropData.h"
#include"ItemSystem.h"
#include"WorldItemDropVisualSystem.h"
#include"StorageSystem.h"
#include"NavigationSystemUtility.h"
#include <cstdio>
#include<algorithm>
#include"CollisionSystem.h"
#include"ObjectAttributeManager.h"


using namespace DirectX;

namespace Game::Character::Job
{
	JobActionComponent::JobActionComponent()
	{
		printf("\n[JobActionComponent ctor] this=%p", this);
	}


	JobActionComponent::~JobActionComponent()
	{
		printf("\n[JobActionComponent dtor] this=%p", this);
	}

	//ターゲットを座標指定で選択する
	bool JobActionComponent::SelectTargetPosition(const DirectX::XMFLOAT3& targetPos)
	{
		target_.type = JobTargetType::Position;
		target_.targetPos = targetPos;
		target_.objectKey.clear();
		target_.targetTid = {};
		target_.targetCharacterId = {};
		target_.hasCharacterId = false;
		return true;

	}

	bool JobActionComponent::SelectTargetObject(const std::wstring& objectKey, const std::wstring& modelPath, const std::wstring& drawKey,const DirectX::XMFLOAT3& targetPos, const Engine::Components::TransformId& targetTid)
	{
		target_.type = JobTargetType::Position;
		target_.targetPos = targetPos;
		target_.objectKey = objectKey;
		target_.modelPath = modelPath;
		target_.drawKey = drawKey;
		target_.targetTid = targetTid;
		target_.targetCharacterId = {};
		target_.hasCharacterId = false;
		return true;

	}


	bool JobActionComponent::SelectTargetStorageById(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, uint32_t selectStorageId)
	{
		//characterの現在位置を取得
		DirectX::XMFLOAT3 myPos{};
		auto chTid = character.GetFbxInstanceHandle().tid;
		//キャラクター自身の座標を取得
		auto chPos = updateContext.engineServices.transform->GetLocal(chTid);

		if (chPos) {
			myPos = chPos->pos.xyz;
		}
		else {
			return false;
		}

		//倉庫IDから選択したオブジェクトのtidを取得
		auto selectedTid = updateContext.gameServices.storageSystem.GetTransformId(selectStorageId);
		//ワールド上のオブジェクトからtidに対応するオブジェクトを探す
		auto selectedWorldObj = updateContext.gameServices.worldObjectSystem.Find(selectedTid);

		if (!selectedWorldObj) {
			return false;
		}

		DirectX::XMFLOAT3 foundPos{};
		auto targetPos = updateContext.engineServices.transform->GetLocal(selectedTid);
		if (!targetPos) {
			return false;
		}


		foundPos = targetPos->pos.xyz;

		target_.type = JobTargetType::Object;
		target_.targetPos = foundPos;
		target_.objectKey = selectedWorldObj->objectKey;
		target_.targetCharacterId = {};
		target_.targetTid = selectedTid;
		target_.hasCharacterId = false;
		target_.modelPath = selectedWorldObj->modelPath;
		target_.drawKey = selectedWorldObj->keyDrawName;

		return true;

	}

	bool JobActionComponent::SelectNearestTargetObject(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, Object::ObjectType searchObjectType, float searchRange)
	{
		//characterの現在位置を取得
		DirectX::XMFLOAT3 myPos{};
		auto chTid = character.GetFbxInstanceHandle().tid;
		//キャラクター自身の座標を取得
		auto chPos = updateContext.engineServices.transform->GetLocal(chTid);

		if (chPos) {
			myPos = chPos->pos.xyz;
		}
		else {
			return false;
		}

		std::vector<std::wstring> searchObjKeyList;
		
		auto objectKeyList = updateContext.gameServices.objectAttributeManager.GetAllObjectKey();
		for (const auto& objKey : objectKeyList) {
			auto objType = updateContext.gameServices.objectAttributeManager.GetObjectType(objKey);
			//もし、オブジェクトタイプがサーチするものと同じだったらサーチオブジェクトキーとしてリストに登録する
			if (objType && *objType == searchObjectType) {
				searchObjKeyList.push_back(objKey);
			}
		}

		std::vector<Engine::World::OutputWorldObjectData> outPutObjData;
		//ここでキーから取得したアウトプットリストを結合する
		for (const auto& objKey : searchObjKeyList) {
			auto outPutTempObjData = updateContext.gameServices.worldObjectSystem.FindTidByObjectKey(objKey);
			outPutObjData.insert(outPutObjData.end(), outPutTempObjData.begin(), outPutTempObjData.end());
		}

		//ワールドオブジェクトシステムから、objectKeyに対応するオブジェクトのTransformIdをすべて取得
		if (outPutObjData.empty()) {
			//ClearTarget();
			return false;
		}

		//objectKeyに対応するオブジェクトのTransformIdのリストから、characterの周囲のモデルを探すための候補リストを作成
		std::vector<Engine::Components::TransformId> candidateIds;

		for (const auto& objData : outPutObjData) {
			//unreachableTargets_からobjData.tidを探す
			auto it = std::find(unreachableTargets_.begin(), unreachableTargets_.end(), objData.tid);
			//除外リスト対象でなければ候補として追加（unreachableTargets_これが空であれば必ずendになるため）
			if (it == unreachableTargets_.end()) {
				candidateIds.push_back(objData.tid);

			}

		}


		Engine::Components::TransformId nearestObjTid;
		//中心となるキャラクターの周囲のモデルを探して最も近いものをnearestObjTidに格納する。見つからなければfalseを返す
		auto found = updateContext.engineServices.transform->FindNearestObject(chTid, candidateIds, searchRange, nearestObjTid);

		DirectX::XMFLOAT3 foundPos{};

		if (!found)
		{
			//ClearTarget();
			return false;
		}

		auto it = std::find_if(
			outPutObjData.begin(),
			outPutObjData.end(),
			[&](const auto& objData)
			{
				return objData.tid == nearestObjTid;
			}
		);

		if (it == outPutObjData.end()) {
			return false;
		}

		const auto& nearestObjData = *it;

		auto targetPos = updateContext.engineServices.transform->GetLocal(nearestObjTid);
		if (!targetPos) {
			return false;
		}

		foundPos = targetPos->pos.xyz;

		target_.type = JobTargetType::Object;
		target_.targetPos = foundPos;
		target_.objectKey = nearestObjData.objectKey;
		target_.targetCharacterId = {};
		target_.targetTid = nearestObjTid;
		target_.hasCharacterId = false;
		target_.modelPath = nearestObjData.modelPath;
		target_.drawKey = nearestObjData.keyDrawName;

		return true;

	}

	bool JobActionComponent::SelectNearestTargetObject(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext,const std::wstring& objectKey, float searchRange)
	{
		//characterの現在位置を取得
		DirectX::XMFLOAT3 myPos{};
		auto chTid = character.GetFbxInstanceHandle().tid;
		//キャラクター自身の座標を取得
		auto chPos = updateContext.engineServices.transform->GetLocal(chTid);

		if(chPos){
			myPos = chPos->pos.xyz;
		}
		else {
			return false;
		}

		//ワールドオブジェクトシステムから、objectKeyに対応するオブジェクトのTransformIdをすべて取得
		auto outPutObjData = updateContext.gameServices.worldObjectSystem.FindTidByObjectKey(objectKey);
		if (outPutObjData.empty()){
			//ClearTarget();
			return false;
		}

		//objectKeyに対応するオブジェクトのTransformIdのリストから、characterの周囲のモデルを探すための候補リストを作成
		std::vector<Engine::Components::TransformId> candidateIds;

		for (const auto& objData : outPutObjData) {
			//unreachableTargets_からobjData.tidを探す
			auto it = std::find(unreachableTargets_.begin(), unreachableTargets_.end(), objData.tid);
			//除外リスト対象でなければ候補として追加（unreachableTargets_これが空であれば必ずendになるため）
			if (it == unreachableTargets_.end()) {
				candidateIds.push_back(objData.tid);

			}

		}


		Engine::Components::TransformId nearestObjTid;
		//中心となるキャラクターの周囲のモデルを探して最も近いものをnearestObjTidに格納する。見つからなければfalseを返す
		auto found = updateContext.engineServices.transform->FindNearestObject(chTid, candidateIds, searchRange, nearestObjTid);

		DirectX::XMFLOAT3 foundPos{};

		if (!found)
		{
			//ClearTarget();
			return false;
		}

		auto it = std::find_if(
			outPutObjData.begin(),
			outPutObjData.end(),
			[&](const auto& objData)
			{
				return objData.tid == nearestObjTid;
			}
		);

		if (it == outPutObjData.end()) {
			return false;
		}

		const auto& nearestObjData = *it;

		auto targetPos = updateContext.engineServices.transform->GetLocal(nearestObjTid);
		if (!targetPos) {
			return false;
		}

		foundPos = targetPos->pos.xyz;

		target_.type = JobTargetType::Object;
		target_.targetPos = foundPos;
		target_.objectKey = objectKey;
		target_.targetCharacterId = {};
		target_.targetTid = nearestObjTid;
		target_.hasCharacterId = false;
		target_.modelPath = nearestObjData.modelPath;
		target_.drawKey = nearestObjData.keyDrawName;

		return true;

	}

	bool JobActionComponent::SelectNearestTargetObject(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, const std::wstring& objectKey, float searchRange, const std::function<bool(Engine::Components::TransformId)>& predicate)
	{
		//characterの現在位置を取得
		DirectX::XMFLOAT3 myPos{};
		auto chTid = character.GetFbxInstanceHandle().tid;
		//キャラクター自身の座標を取得
		auto chPos = updateContext.engineServices.transform->GetLocal(chTid);

		if (chPos) {
			myPos = chPos->pos.xyz;
		}
		else {
			return false;
		}

		//ワールドオブジェクトシステムから、objectKeyに対応するオブジェクトのTransformIdをすべて取得
		auto outPutObjData = updateContext.gameServices.worldObjectSystem.FindTidByObjectKey(objectKey);
		if (outPutObjData.empty()) {
			//ClearTarget();
			return false;
		}

		//objectKeyに対応するオブジェクトのTransformIdのリストから、characterの周囲のモデルを探すための候補リストを作成 
		std::vector<Engine::Components::TransformId> candidateIds;

		for (const auto& objData : outPutObjData) {
			//unreachableTargets_からobjData.tidを探す
			auto it = std::find(unreachableTargets_.begin(), unreachableTargets_.end(), objData.tid);

			//経路探索不能の対象は除外
			if (it != unreachableTargets_.end()) {
				continue;
			}

			//呼び出し側の条件を満たさない対象は除外
			if (!predicate(objData.tid)) {
				continue;
			}

			candidateIds.push_back(objData.tid);
		}


		Engine::Components::TransformId nearestObjTid;
		//中心となるキャラクターの周囲のモデルを探して最も近いものをnearestObjTidに格納する。見つからなければfalseを返す
		auto found = updateContext.engineServices.transform->FindNearestObject(chTid, candidateIds, searchRange, nearestObjTid);

		DirectX::XMFLOAT3 foundPos{};

		if (!found)
		{
			//ClearTarget();
			return false;
		}

		auto it = std::find_if(
			outPutObjData.begin(),
			outPutObjData.end(),
			[&](const auto& objData)
			{
				return objData.tid == nearestObjTid;
			}
		);

		if (it == outPutObjData.end()) {
			return false;
		}

		const auto& nearestObjData = *it;

		auto targetPos = updateContext.engineServices.transform->GetLocal(nearestObjTid);
		if (!targetPos) {
			return false;
		}

		foundPos = targetPos->pos.xyz;

		target_.type = JobTargetType::Object;
		target_.targetPos = foundPos;
		target_.objectKey = objectKey;
		target_.targetCharacterId = {};
		target_.targetTid = nearestObjTid;
		target_.hasCharacterId = false;
		target_.modelPath = nearestObjData.modelPath;
		target_.drawKey = nearestObjData.keyDrawName;

		return true;

	}

	bool JobActionComponent::BuildPathFromJobTarget(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, const PathGoal& goalInfo,const PathQuery& pathQuery)
	{
		return BuildPath(character, updateContext, goalInfo, pathQuery);
	}

	bool JobActionComponent::BuildPath(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext,const PathGoal& goalInfo, const PathQuery& pathQuery)
	{
		auto chTid = character.GetFbxInstanceHandle().tid;
		auto chPos = updateContext.engineServices.transform->GetLocal(chTid);
		if (!chPos) {
			return false;
		}


		auto path = updateContext.engineServices.navigationSystem->FindPath(*updateContext.engineServices.chunkManager, chPos->pos.xyz, goalInfo, pathQuery, true);
		if (!path.success) {
			//オブジェクトの場合のみ到達不能リストへ登録
			if (target_.type == JobTargetType::Object)
			{
				//経路探索に失敗したら、対象となっているオブジェクトを除外リストへ追加
				unreachableTargets_.push_back(target_.targetTid);
			}
			return false;
		}


		//オブジェクト用の到達不能リスト
		if (target_.type == JobTargetType::Object)
		{
			//経路探索に成功したら、除外リストをクリアする
			unreachableTargets_.clear();
		}

		Engine::Movement::PathFollowState pathFollowState(0, true, std::move(path.points));
		updateContext.engineServices.locomotion->InitPathFollowState(chTid,pathFollowState);
		return true;

	}

	bool JobActionComponent::MoveTo(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		XMFLOAT3 dest{};

		auto chTid = character.GetFbxInstanceHandle().tid;
		//もし、ターゲットに指定されたものがあればそこを目的地にして移動する
		bool hasDestination = updateContext.engineServices.locomotion->UpdateDestinationFromPath(chTid, dest, 0.5f);
		//有効な移動先が存在する場合だけ移動
		if(hasDestination){
			updateContext.engineServices.locomotion->MoveTowardsDest(
				chTid,
				dest,
				updateContext.simulationDeltaTime);
		}

		updateContext.engineServices.groundingSystem->ApplyTerrainHeight(chTid, updateContext.simulationDeltaTime);

		return true;

	}

	bool JobActionComponent::Attack(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		if (target_.type != JobTargetType::Object) {
			return false;
		}

		auto targetTid = target_.targetTid;
		auto chTid = character.GetFbxInstanceHandle().tid;

		auto worldObj = updateContext.gameServices.worldObjectSystem.Find(targetTid);
		if (!worldObj) {
			return false;
		}

		if (character.GetCharacterCombatState().combatFlags.isInCombat) {
			//戦闘中の処理
			//PrintWstring(L"\nIn Combat!"+ std::to_wstring(chTid.index));

			auto& comState = character.GetCharacterCombatState();
			comState.combatFlags.justEnteredCombat = false;

			auto lastSelectionOpt = character.GetLastSelection();
			auto targetName = character.GetTargetInfo().targetInfo.currentDrawName;
			if (character.GetTargetInfo().targetInfo.currentTid) {
				auto targetTid = *character.GetTargetInfo().targetInfo.currentTid;
				//攻撃リクエストの作成
				Combat::AttackRequest attackRequest(
					targetTid,
					targetName,
					1,
					updateContext.simulationDeltaTime,
					updateContext.engineServices.transform->GetLocal(chTid)->pos.xyz,
					lastSelectionOpt->obstaclePos
				);

				//攻撃可能かどうかの判定
				if (updateContext.gameServices.combatSystem.IsAttackEnabled(targetName)) {
					//攻撃リクエストをゲーム層のCombatSystemに送る
					updateContext.gameServices.combatSystem.EnqueueAttack(attackRequest);

				}
				else {
					comState.combatFlags.isInCombat = false;
					target_ = {};
					//戦闘終了の処理
					//PrintWstring(L"\nFinish Combat!");

				}

				return true;

			}
			return false;


		}

		
		auto targetModelName = worldObj->modelPath;
		auto targetDrawName = worldObj->keyDrawName;

		auto targetLocalPos = updateContext.engineServices.transform->GetLocal(targetTid);
		auto chLocalPos = updateContext.engineServices.transform->GetLocal(chTid);
		if (!targetLocalPos) {
			return false;
		}

		//今のターゲットのモデル名と描画名を前の選択情報として保存
		std::wstring previousModelName = character.GetTargetInfo().targetInfo.currentModelName; //前のターゲットのモデル名を保存
		std::wstring previousDrawName = character.GetTargetInfo().targetInfo.currentDrawName; //前のターゲットの描画名を保存


		//最後の選択結果を保存
		SelectionResult lastSelection;
		lastSelection.modelName = targetModelName;
		lastSelection.drawName = targetDrawName;
		lastSelection.hit = true;
		lastSelection.isObstacle = true;
		lastSelection.obstaclePos = targetLocalPos->pos.xyz;

		//最後にターゲットにしたオブジェ情報保持
		character.SetLastSelection(lastSelection);

		CombatTargetInfo combatInfo(
			targetTid,
			character.GetTargetInfo().targetInfo.currentTid,
			targetModelName, //今のターゲットのモデル名
			targetDrawName, //今のターゲットの描画名
			previousModelName, //前のターゲットのモデル名で昔のターゲットのモデル名を更新
			previousDrawName //前のターゲットの描画名で昔のターゲットの描画名を更新
		);

		CombatFlags combatFlags(
			true, //戦闘中
			true, //戦闘に入ったばかり
			false //戦闘をでたばかりではない
		);
		CharacterCombatState characterCombatState;
		characterCombatState.combatFlags = combatFlags;
		characterCombatState.targetInfo = combatInfo;
		//キャラクターの戦闘状態をセットする
		character.SetCharacterCombatState(characterCombatState);

		if (character.GetTargetInfo().targetInfo.currentTid) {

			auto targetTid = *character.GetTargetInfo().targetInfo.currentTid;
			//攻撃リクエストの作成
			Combat::AttackRequest attackRequest(
				targetTid,
				worldObj->keyDrawName,
				1,
				1.f,
				chLocalPos->pos.xyz,
				targetLocalPos->pos.xyz
			);

			//攻撃リクエストをゲーム層のCombatSystemに送る関数：GetfbxKeysIndex：キーが見つかりませんでした
			updateContext.gameServices.combatSystem.EnqueueAttack(attackRequest);

		}

		return true;
	}

	bool JobActionComponent::Pickup(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, const ::World::Runtime::DroppedItem& targetDropItem)
	{
		auto& itemSystem = updateContext.gameServices.itemSystem;
		auto& inventory = character.GetInventory();
		auto playerTid = character.GetFbxInstanceHandle().tid;
		auto playerPos = updateContext.engineServices.transform->GetLocal(playerTid)->pos.xyz;
		//アイテムを拾う処理近くに落ちているアイテムの中から一番近いものを拾う
		auto pickUpdata = itemSystem.TryPickupOne(inventory, playerPos, targetDropItem.item.masterKey);

		if (pickUpdata && pickUpdata->success) {
			//printf("\nアイテムを拾ってインベントリに反映しました。");
			//printf("\nPicked Up Item : %ls", pickUpdata->item.masterKey.c_str());
			updateContext.gameServices.worldItemDropVisualSystem.EracePrimitive(pickUpdata->item.masterKey, pickUpdata->droppedItemId);
			return true;
		}

		return false;
	}

	bool JobActionComponent::Deposit(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext)
	{
		auto& chInventory = character.GetInventory();
		auto& allInventoryItems = character.GetInventory().GetAllData();
		auto* storage = updateContext.gameServices.storageSystem.GetInventory(1);
		if (!storage) {
			return false;
		}

		for (const auto& item : allInventoryItems) {
			updateContext.gameServices.itemSystem.InventoryAddOrStack(*storage, item.value.masterKey, item.value.stackCount);
			chInventory.Remove(item.key);
		}

		return true;
	}

	void JobActionComponent::ClearTarget()
	{
		//初期化
		target_ = JobTarget{};
	}

	float JobActionComponent::Distance(const DirectX::XMFLOAT3& pos1, const DirectX::XMFLOAT3& pos2)
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&pos1), DirectX::XMLoadFloat3(&pos2))));
	}

	std::optional<Engine::Components::TransformId> JobActionComponent::SelectNearestTarget(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext,
		const std::vector<Engine::Components::TransformId>& targetTidList, float searchRange, const std::function<bool(Engine::Components::TransformId)>& predicate)
	{
		std::optional<Engine::Components::TransformId> nearestTid;

		float nearestDistance = searchRange;

		const auto characterTid = character.GetFbxInstanceHandle().tid;

		//キャラクターの距離取得
		const auto characterTransform = updateContext.engineServices.transform->GetLocal(characterTid);

		if (!characterTransform) {
			return std::nullopt;
		}

		for (const auto targetTid : targetTidList)
		{
			if (!predicate(targetTid)) {
				continue;
			}

			//ターゲットの距離取得
			const auto targetTransform = updateContext.engineServices.transform->GetLocal(targetTid);

			if (!targetTransform) {
				continue;
			}

			const float distance = Distance(characterTransform->pos.xyz, targetTransform->pos.xyz);

			//最も近い距離を更新
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearestTid = targetTid;
			}
		}

		return nearestTid;
	}

	bool JobActionComponent::SelectRandomTargetPosition(CharacterBase& character, const CharacterUpdateContext& updateContext, float radius , int maxAttempts)
	{
		const auto tid = character.GetFbxInstanceHandle().tid;

		const auto transform = updateContext.engineServices.transform->GetLocal(tid);

		if (!transform)
		{
			return false;
		}

		const auto currentPos = transform->pos.xyz;

		for (int i = 0; i < maxAttempts; i++) {
			DirectX::XMFLOAT3 targetPos = currentPos;
			targetPos.x += RandomFloat(-radius, radius);
			targetPos.z += RandomFloat(-radius, radius);


			std::unordered_map<std::wstring, std::vector<int>> nearColliders;
			//対象座標の近くにあるコリジョンを全て取得
			updateContext.engineServices.collisionSystem->FindNearbyColliders(targetPos, nearColliders, 2.0f);

			//候補座標がOBB内に入っていれば使用できない
			if (updateContext.engineServices.collisionSystem->IsPointInsideNearbyObb(targetPos, nearColliders)) {
				continue;
			}
			else {
				//移動先座標の決定
				SelectTargetPosition(targetPos);

				return true;
			}

		}
		return false;
	}

	float JobActionComponent::RandomFloat(float min, float max)
	{
		static std::random_device rd;
		static std::mt19937 engine(rd());

		std::uniform_real_distribution<float> dist(min, max);

		return dist(engine);
	}

	bool JobActionComponent::IsUnreachableTarget(Engine::Components::TransformId tid) const
	{
		return std::find(unreachableTargets_.begin(), unreachableTargets_.end(), tid) != unreachableTargets_.end();
	}

	bool JobActionComponent::TrySetTargetCollisionToPathGoal(const CharacterUpdateContext& updateContext, PathGoal& pathGoal)
	{
		const auto& modelPath = GetTargetInfo().modelPath;

		if (modelPath.empty()) {
			//モデルパスが空の場合は、経路探索に失敗とみなす
			return false;
		}

		std::wstring keyDrawName = GetTargetInfo().drawKey;
		const auto& model = updateContext.engineServices.modelProvider->GetFromFbxName(modelPath);
		int idx = model->GetfbxKeysIndex(keyDrawName);
		auto fbxPos = updateContext.engineServices.fbxInstanceRegistry->TryGetInstance(modelPath, idx);
		if (!fbxPos) {
			//fbxPosが空の場合は、経路探索に失敗とみなす
			return false;
		}
		//コリジョンで全体を覆っているものを指定するようにした
		for (const auto& colliderKey : fbxPos->collsionNameKey) {
			const auto& colliderIdx = model->ColliderIdx(colliderKey.nameKey);
			auto& collider = model->Collider(colliderIdx);
			if (model->Collider(colliderIdx).collisionType == CollisionType::WHOLE_OBJECT) {
				pathGoal.collision = &collider;
				return true;
			}
		}

		return false;
	}
}