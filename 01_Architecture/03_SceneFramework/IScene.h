#pragma once
#include"Engine/FrameContext.h"


namespace Engine::Scene
{
    // インターフェース
    class IScene {
    public:
        virtual ~IScene() = default;
		//シーンの初期化、更新、描画、終了処理の純粋仮想関数
        virtual void Initialize() = 0;
		//シーンの更新処理
        virtual void Update(::Engine::FrameContext& frameContext) = 0;
		//シーンの描画処理
        virtual void Draw() = 0;
		//シーンの終了処理
        virtual void Finalize() = 0;
    };

}