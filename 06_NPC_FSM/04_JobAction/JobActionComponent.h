#pragma once
#include<memory>
#include<DirectXMath.h>
#include<string>
#include<functional>
#include<optional>
#include"JobTarget.h"
#include"NavigationSystemUtility.h"
#include"ObjectAttributeUtility.h"

namespace World::Runtime
{
	struct DroppedItem;
}

namespace Game::Character
{
	class CharacterBase;
}

namespace Game::Character
{
	struct CharacterUpdateContext;
}

struct PathQuery;


namespace Game::Character::Job
{
	struct JobTarget;


	class JobActionComponent
	{
	public:
		JobActionComponent();
		~JobActionComponent();

		//ターゲットを座標指定で選択する
		bool SelectTargetPosition(const DirectX::XMFLOAT3& targetPos);

		//一番近くのターゲットをオブジェクト指定で選択する
		bool SelectTargetStorageById(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, uint32_t selectStorageId);

		//一番近くのターゲットをオブジェクト指定で選択する（ラムダ関数でサーチするオブジェクトを指定する1）
		bool SelectNearestTargetObject(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, const std::wstring& objectKey, float searchRange,const std::function<bool(Engine::Components::TransformId)>& predicate);

		//一番近くのターゲットをオブジェクトタイプ指定で選択する
		bool SelectNearestTargetObject(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, Object::ObjectType searchObjectType, float searchRange);

		//一番近くのターゲットをオブジェクト指定で選択する
		bool SelectNearestTargetObject(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext,const std::wstring& objectKey, float searchRange);
		//ターゲットをキャラクター指定で選択する
		//bool SelectTargetCharacter(CharacterBase& character, Game::Character::CharacterId targetId, const DirectX::XMFLOAT3& targetPos);
		
		//オブジェクトのターゲットを直接指定する
		bool SelectTargetObject(const std::wstring& objectKey, const std::wstring& modelPath, const std::wstring& drawKey, 
			const DirectX::XMFLOAT3& targetPos, const Engine::Components::TransformId& targetTid);

		//JobTargetをもとに目的地までの経路を構築する
		bool BuildPathFromJobTarget(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, const PathGoal& goalInfo,const PathQuery& pathQuery);


		//目的地までの経路を構築する
		bool BuildPath(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext,const PathGoal& goalInfo, const PathQuery& pathQuery);
		//目標まで移動する
		bool MoveTo(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext);

		bool Attack(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext);

		//アイテムを拾う
		bool Pickup(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext, const ::World::Runtime::DroppedItem& targetDropItem);

		//アイテムを預ける
		bool Deposit(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext);
		//bool Attack(CharacterBase& character, CharacterId targetId, float deltaTime);
		//bool CarryTo(CharacterBase& character, const DirectX::XMFLOAT3& targetPos, float deltaTime);

		//ターゲットをクリアする
		void ClearTarget();

		//2点間の距離を計算する
		float Distance(const DirectX::XMFLOAT3& pos1, const DirectX::XMFLOAT3& pos2);

		const JobTarget& GetTargetInfo() const
		{
			return target_;
		}

		/**
		 * @brief 候補となるTransformIdの中から、キャラクターに最も近い対象を選択する
		 * @param character 探索の基準となるキャラクター
		 * @param updateContext キャラクター更新に必要なコンテキスト
		 * @param targetTidList 探索対象となるTransformIdの一覧
		 * @param searchRange 探索範囲
		 * @param predicate 対象を候補として採用するか判定する関数
		 * @return 条件を満たす最も近い対象のTransformId。対象が存在しない場合はstd::nullopt
		 */
		std::optional<Engine::Components::TransformId> SelectNearestTarget(CharacterBase& character, const Game::Character::CharacterUpdateContext& updateContext,
			const std::vector<Engine::Components::TransformId>& targetTidList, float searchRange, const std::function<bool(Engine::Components::TransformId)>& predicate);

		/**
		* @brief ランダムに移動座標を決定する
		* @param character 探索の基準となるキャラクター
		* @param updateContext キャラクター更新に必要なコンテキスト
		* @param searchRange 探索範囲
		* @param maxAttempts 最大試行回数
		* @return 成功 true / 失敗 false
		*/
		bool SelectRandomTargetPosition(CharacterBase& character, const CharacterUpdateContext& updateContext, float radius, int maxAttempts);

		/**
		* @brief ランダムに数値を取得する
		* @param min 取りうる下限
		* @param max 取りうる上限
		* @return ランダムな値
		*/
		float RandomFloat(float min, float max);

		/**
		* @brief 既に到達不可能な建築物（オブジェクト）として登録されているかを確認する
		* @param tid 対象のトランスフォームID
		* @return true 登録されている　/ false 登録されていない
		*/
		bool IsUnreachableTarget(Engine::Components::TransformId tid) const;

		/**
		* @brief 目的となるオブジェクトのOBBをゴールパスに設定する
		* @param updateContext キャラクター更新に必要なコンテキスト
		* @out pathGoal 関数内で作成したゴールパスを出力する
		* @return true 生成成功　/ false 生成失敗
		*/
		bool TrySetTargetCollisionToPathGoal(const CharacterUpdateContext& updateContext, PathGoal& pathGoal);


		/**
		* @brief 引数で受け取ったターゲット情報をターゲットとしてセットする
		* @param target ターゲットとしてセットするものを受け取る
		*/
		void SetTargetInfo(const JobTarget& target)
		{
			target_ = target;
		}
	private:
		//経路探索をして目的地まで行けなかったとき用に、除外リストを作成する
		std::vector<Engine::Components::TransformId> unreachableTargets_;

	private:

		//現在の職業のターゲット情報を保持する構造体
		JobTarget target_;

	private:
	};
}