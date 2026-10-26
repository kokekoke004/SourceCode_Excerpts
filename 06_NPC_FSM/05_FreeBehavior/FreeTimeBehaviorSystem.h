#pragma once
#include<unordered_map>
#include<memory>
#include <optional>

namespace Game::Character
{
	class CharacterBase;
	struct CharacterUpdateContext;
}


namespace Game::Character::FreeTime
{
	enum class FreeTimeBehaviorType;
	class FreeTimeBehaviorBase;

	class FreeTimeBehaviorSystem
	{
	public:
		FreeTimeBehaviorSystem() = default;
		~FreeTimeBehaviorSystem() = default;

		/**
		* @brief 暇時間中に実行する行動を登録する
		* @param type 登録する暇時間行動の種類
		* @param behavior 行動の処理を定義したクラスのインスタンス
		*/
		void RegisterBehavior(FreeTimeBehaviorType type, std::unique_ptr<FreeTimeBehaviorBase> behavior);

		/**
		* @brief キャラクターの暇時間行動を更新する
		* @param character 更新対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @details
		* 現在のキャラクター状態から実行する暇時間行動を決定し、
		* 必要に応じて行動の切り替えを行った後、現在の行動を更新する。
		*/
		void UpdateCharacterBehavior(CharacterBase& character,const CharacterUpdateContext& updateContext);

		/**
		* @brief 現在実行中の暇時間行動を終了する
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @details
		* 現在アクティブな行動の終了処理を実行し、
		* 暇時間行動の状態を未選択状態へ戻す。
		*/
		void StopCurrentBehavior(CharacterBase& character, const CharacterUpdateContext& updateContext);

		/**
		* @brief 現在アクティブな行動があるか取得
		* 
		* @return true アクティブあり
		* @return false アクティブなし
		*/
		bool HasActiveBehavior() const;

		/**
		* @brief 現在の自由時間行動を仕事によって割り込み可能かどうかを返す
		*
		* @return true 割込み可
		* @return false 割込み不可
		*/
		bool CanCurrentBehaviorInterruptByJob() const;

	private:

		/**
		* @brief 現在実行中の暇時間行動を終了する
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		* 
		* @return 選択された行動のタイプ
		*/
		FreeTimeBehaviorType SelectBehavior(CharacterBase& character, const CharacterUpdateContext& updateContext);

	private:
		std::unordered_map<FreeTimeBehaviorType,std::unique_ptr<FreeTimeBehaviorBase>> behaviors_;
		
		//現在実行中の自由時間行動
		std::optional<FreeTimeBehaviorType> currentBehaviorType_;
	};

}