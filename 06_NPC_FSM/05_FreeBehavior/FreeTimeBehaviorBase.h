#pragma once
#include"CharacterBase.h"
#include"FreeTimeBehaviorUtility.h"
#include"CharacterUpdateContext.h"

namespace Game::Character::FreeTime
{
	class FreeTimeBehaviorBase
	{
	public:
		FreeTimeBehaviorBase() = default;
		~FreeTimeBehaviorBase() = default;

		/**
		* @brief 現在の自由時間行動を仕事によって割り込み可能かどうかを返す
		*
		* @return true 割込み可能 / false 割込み不可
		*/
		virtual bool CanInterruptByJob() const = 0;

		/**
		* @brief FSMの行動優先順位の設定
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		* 
		* @return 優先順位
		*/
		virtual int GetPriority(CharacterBase& character,const CharacterUpdateContext& updateContext) const = 0;

		/**
		* @brief FSMが実行可能であるかどうか
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*
		* @return 可能かどうか(true 可 / false 不可)
		*/
		virtual bool CanStart(CharacterBase& character, const CharacterUpdateContext& updateContext) const = 0;

		/**
		* @brief FSMの初期化をする
		* @param character 対象のキャラクター
		*/
		virtual void Enter(CharacterBase& character) = 0;

		/**
		* @brief FSMの更新処理
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		* 
		* @return 更新の結果
		*/
		virtual FreeTimeBehaviorUpdateResult Update(CharacterBase& character, const CharacterUpdateContext& updateContext) = 0;

		/**
		* @brief FSMを出るときの処理
		* @param character 対象のキャラクター
		* @param updateContext キャラクター更新時に使用する各種コンテキスト
		*/
		virtual void Exit(CharacterBase& character, const CharacterUpdateContext& updateContext) = 0;


	private:

	};

}