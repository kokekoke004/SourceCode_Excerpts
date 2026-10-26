#pragma once

namespace Engine
{
	struct FrameContext;
}

namespace Game::Character
{
	struct CharacterUpdateContext;

	//キャラクターのインターフェースクラス
	//キャラクター関連処理の基底クラス
	class ICharacter
	{
	public:
		virtual ~ICharacter() = default;
		//キャラクターの更新処理
		virtual void Update(const CharacterUpdateContext& updateContext) = 0;
	};
}