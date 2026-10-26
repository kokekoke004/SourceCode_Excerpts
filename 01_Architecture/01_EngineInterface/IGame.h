#pragma once

#include"Engine/FrameContext.h"

namespace Engine
{
    class Engine;



    class IGame
    {
    public:
        IGame() = default;
        virtual ~IGame() = default;
        virtual void OnInit(Engine& engine) = 0;
        //１フレームのデルタタイムを渡す
        virtual void OnUpdate(Engine& engine, ::Engine::FrameContext& frameContext) = 0;
        virtual void OnRender(Engine& engine) = 0;
        virtual void OnShutdown() = 0;
    };
}