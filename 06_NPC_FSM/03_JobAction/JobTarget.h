#pragma once
#include <string>
#include <DirectXMath.h>

#include "CharacterTypes.h"
#include"TransformSystem.h"

namespace Game::Character::Job
{

	//ターゲットの種類
    enum class JobTargetType
    {
        None,
        Position,
        Object
    };


	//職業ごとのターゲット情報を保持する構造体
    struct JobTarget
    {
        JobTargetType type = JobTargetType::None;
		DirectX::XMFLOAT3 targetPos = { 0.0f, 0.0f, 0.0f }; //位置をターゲットにする場合の座標

		std::wstring modelPath; //オブジェクトをターゲットにする場合のモデルパス
		std::wstring drawKey; //オブジェクトをターゲットにする場合の描画キー
		std::wstring objectKey; //オブジェクトをターゲットにする場合の識別子
		Engine::Components::TransformId targetTid; //オブジェクトをターゲットにする場合のTransformId
        CharacterId targetCharacterId = CharacterId{}; //キャラクターをターゲットにする場合のID
        bool hasCharacterId = false; 
    };
}