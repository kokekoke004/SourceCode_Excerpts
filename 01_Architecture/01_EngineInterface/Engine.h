#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "d3dx12.h"
#include<memory>
#include <cstdint>
#include <string>

#include"GraphicsUtility.h"
#include"IModelRenderer.h"
#include"DebugViewMode.h"
#include"FrameContext.h"

struct ShaderInfo;

namespace Engine
{
	class EngineCore;
    class IGame;

    struct EngineServices;
    struct FrameRenderContext;


    struct EngineConfig {
        uint32_t width;
        uint32_t height;
        std::wstring title;

        //いまは EngineCore では使わないが、Config として残す（上位層が使う可能性があるため）
        std::string pyRoot;
        std::string pySitePackages;

        EngineConfig() = default;

        EngineConfig(uint32_t w, uint32_t h, std::wstring t)
            : width(w), height(h), title(std::move(t)) {
        }

        EngineConfig(uint32_t w, uint32_t h, std::wstring t,
            std::string pyRoot_, std::string pySitePackages_)
            : width(w),
            height(h),
            title(std::move(t)),
            pyRoot(std::move(pyRoot_)),
            pySitePackages(std::move(pySitePackages_)) {
        }
    };


	class Engine
	{
	public:
		Engine(const EngineConfig& cfg);

        ~Engine();

		void Init(IGame& game,HWND& hwnd);

        void RunFrame(IGame& game);

        void OnDestroy(IGame& game);

        //マウスやキー入力などの更新処理
        void UpdateInput();

        void Update(IGame& game);

        void RenderFrame(IGame& game);

        unsigned int GetWindowWidth();
        unsigned int GetWindowHeight();

		//アスペクト比を返す関数
        float GetAspectRatio();


        //デフォルトのシェーダーの値などをセットする関数
        void InitFrameRenderContextDefaults();


        //ゲームクラスの処理で使う関数//////////

        //エンジンのサービスクラスゲッター
        EngineServices& Services();
        const EngineServices& Services() const;

        //これをゲームの処理で使ってシェーダに渡す情報の更新する
        void UpdateShaderInfoForFrame();

        CommandContext& GetCommandContext();

        const CommandContext& GetCommandContext() const;

        ViewportScissor& GetViewScissor();

        const ViewportScissor& GetViewScissor() const;

        //フレームコンテキストの更新処理
        void UpdateFrameContext(float deltaTime);


        const ShaderInfo& GetShaderInfo() const
        {
            return shaderInfo_;
        }

		//デバッグモードのオンオフ取得
        Render::DebugViewMode GetDebugViewMode() const
        {
            return debugViewMode_;
        }

        //デバッグモードのオンオフ切り替え
        Render::DebugViewMode& GetDebugViewMode()
        {
            return debugViewMode_;
        }


    private:
        //ポストエフェクトで使う定数バッファ更新処理
        void UpdateSsaoConstantForFrame();

        void UpdateDeferredRenderingForFrame();

        void UpdateInstanceData();

        void UpdateGameCamera(float deltaTime);
	private:
        Render::DebugViewMode debugViewMode_ = Render::DebugViewMode::Final;


        FrameContext frameContext_;
        FrameRenderContext frameRenderContext_;
        ShaderInfo shaderInfo_;

		EngineConfig cfg_;
		std::unique_ptr<EngineCore> core_;
	};
}