#pragma once
#include<memory>
#include"Engine/IGame.h"

namespace Game
{
    class Game : public Engine::IGame
    {
    public:
        void OnInit(Engine::Engine& engine) override;

        void OnUpdate(Engine::Engine& engine, ::Engine::FrameContext& frameContext) override;

        void OnRender(Engine::Engine& engine) override;

        void OnShutdown() override;

    private:



    };
}

