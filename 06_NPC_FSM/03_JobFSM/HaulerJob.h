#pragma once
#include<unordered_map>
#include<vector>
#include<optional>
#include"JobBehaviorBase.h"

#include"WorldItemDropData.h"
#include"BuildPlacementUtility.h"
#include"BuildProgressManager.h"
#include"StorageSystem.h"
#include"JobTarget.h"

namespace Game::Build
{
	struct BuildMaterialRequest;
}

//運搬職の行動パターン設計
namespace Game::Character::Job
{
	enum class HaulTargetPurpose
	{
		None,
		//落ちているアイテムを拾う
		PickupDroppedItem,	  
		//倉庫にアイテムを預ける
		DepositStorage, 
		//予約済みアイテムを受け取るために倉庫へ移動中
		WithdrawReservedItem, 
		//生産物
		WithdrawProductionOutput,
		//建築現場にアイテムを運ぶ
		DeliverToConstruction 
	};

	//運搬のリクエストタイプ一覧
	enum class HaulingRequestType
	{
		BuildingSupply,
		DroppedItemPickup,
		ProductionOutput
	};

	struct ProductionOutputRequest
	{
		std::wstring itemKey;
		int32_t amount = 0;
	};

	//運搬リクエスト
	struct HaulingRequest
	{
		HaulingRequestType type;

		Engine::Components::TransformId targetTid;

		DirectX::XMFLOAT3 position;

		int priority = 0;
		//ターゲットとする建築物の資材リクエスト
		std::optional<Build::BuildMaterialRequest> buildMaterialRequest;

		//生産物のアウトプットリクエスト処理
		std::optional<ProductionOutputRequest> productionOutputRequest;

		//ドロップアイテム用
		std::optional<::World::Runtime::DroppedItemId> droppedItem;

		//どの種類のアイテムかを示すID
		std::optional<Item::ItemInstanceId> sourceItemInstanceId;

		//ProductionOutput用
		std::optional<World::Storage::StorageId> storageId;

	};

	struct HaulTaskExcutor
	{
		Game::Build::BuildingId buildId; //建築ID
		std::wstring materialKey; //資材のキー
		int reservedAmount; //この運搬で確保した量
	};

	class HaulerJob : public JobBehaviorBase
	{
	public:
		HaulerJob();

		JobUpdateResult Update(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext) override;
		void Exit(Game::Character::CharacterBase& character, const CharacterUpdateContext& updateContext) override;

		/**
		* @brief 仕事を再開する際の処理を行う
		* @param character キャラクター
		* @param updateContext 更新コンテキスト
		*/
		void Resume(CharacterBase& character, const CharacterUpdateContext& updateContext) override;


	private:
		//運搬の優先順位計算
		int CalculatePriority(const HaulingRequest& request) const;
		std::optional<std::vector< Game::Build::BuildMaterialRequest >> SelectingBuildingObject(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext);
		
		//予約のキャンセル
		void CancelCurrentReservation(CharacterBase& character, const CharacterUpdateContext& updateContext);

		void DropCarriedItems(Game::Character::CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext);
	private:
		//建築側に資材の予約を入れているか
		bool hasBuildReservation_ = false;   
		//倉庫から取り出し済みか
		bool itemsWithdrawn_ = false;        
		//ターゲットとしたものを保持する変数
		std::optional<JobTarget> suspendedTarget_;

		//ターゲットとする建築物の資材リクエスト
		Build::BuildMaterialRequest targetBuildMaterialRequest;

		//倉庫からアイテム取り出すときの予約ID
		using ReservedId = uint32_t;
		std::optional<World::Storage::ReservationId> currentReservedId_;

		std::optional<World::Storage::StorageId> targetSourceStorageId_;

		//資材の運搬で扱うデータ
		//キー：建築ID
		//値：建築ID
		std::unordered_map<Game::Build::BuildingId, std::vector<HaulTaskExcutor>> haulTaskExcutor;
		::World::Runtime::DroppedItem targetDroppedItem_;
		HaulTargetPurpose haulTargetPurpose_ = HaulTargetPurpose::None;
	};
}