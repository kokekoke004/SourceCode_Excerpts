#pragma once
#include"FreeTimeBehaviorBase.h"
#include"StorageSystem.h"
#include<optional>

namespace Game::Character::FreeTime
{
    class EatBehavior : public FreeTimeBehaviorBase
    {
	public:
		EatBehavior() = default;
		~EatBehavior() = default;


		/**
		* @brief 現在の自由時間行動を仕事によって割り込み可能かどうかを返す
		* @return true 仕事による割り込みが可能
		* @return false 仕事による割り込みが不可能
		*/
		bool CanInterruptByJob() const override;

		/**
		* @brief FSMの行動優先順位の設定
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @return 優先順位
		*/
		int GetPriority(CharacterBase& character, const CharacterUpdateContext& updateContext) const override;

		/**
		* @brief FSMが実行可能であるかどうか
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @return 可能かどうか(true 可 / false 不可)
		*/
		bool CanStart(CharacterBase& character, const CharacterUpdateContext& updateContext) const override;

		/**
		* @brief FSMの初期化をする
		* @param character 対象のキャラクター
		*/
		void Enter(CharacterBase& character) override;

		/**
		* @brief FSMの更新処理
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		* @return 更新の結果
		*/
		FreeTimeBehaviorUpdateResult Update(CharacterBase& character, const CharacterUpdateContext& updateContext) override;

		/**
		* @brief FSMを出るときの処理
		* @param character 対象のキャラクター
		*/
		void Exit(CharacterBase& character, const CharacterUpdateContext& updateContext) override;

	private:

		/**
		* @brief 倉庫に確保した予約をキャンセルする処理
		* @freeTimePhase 現在の自由時間行動のフェーズ
		* @param storageSystem 倉庫クラス
		*/
		void CancelCurrentReservation(FreeTimeBehaviorPhase& freeTimePhase, World::Storage::StorageSystem& storageSystem);

	private:
		//食料の確保リクエスト
		struct EatReqest
		{
			//ターゲットの倉庫トランスフォームID
			Engine::Components::TransformId targetTid;
			//ターゲット倉庫の座標
			DirectX::XMFLOAT3 position;

			//ストレージID
			World::Storage::StorageId storageId;

			//引き出すアイテムID
			std::wstring itemMasterKey;

			//ソース元の倉庫内でのアイテムのインスタンスID
			Item::ItemInstanceId sourceStorageItemInstanceId;

			//アイテムのリクエスト個数
			int requestNum = 0;

		};

	private:
		std::optional<World::Storage::ReservationId> currentReservedId_;
		
		//経過時間
		float eatingElapsedTime_ = 0.0f;
		//食べる作業に必要な秒数
		static constexpr float EatingDuration = 2.0f;

		//食べるリクエスト
		EatReqest eatRequest_;
    };
}