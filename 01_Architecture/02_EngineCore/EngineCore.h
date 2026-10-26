#pragma once

#include <windows.h>
#include <filesystem>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "d3dx12.h"

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>
#include<map>
#include <array>

#include "EngineServices.h"
#include "DXAppUtillity.h"
#include "PostProcessContext.h"
#include "helper.h"


#define kFrameCount 2

class FileManager;
class FbxLoader;
class CollisionAttributeManager;
class CreateResource;
class PythonEngine;
class LoadTex;
class CameraManager;
class MouseInput;
class KeyInput;
class TextInputManager;
class GameTimer;
class ComputeShaderManager;
class DisplacementMapping;
class DebugDrawManager;
class PipelineStateObject;
class FrustumCuller;
class ChunkManager;
class UiManager;
class CollisionSystem;
class Screenshot;

namespace Compute::UpdateInstancePos 
{
    class UpdateInstancePosCompute;

}

namespace Primitive::Manager
{
    class PrimitiveManager;
}

struct FbxPos;

namespace Engine
{
    class TextRenderer;
    class ModelRenderSystem;
    class ModelLoadSystem;

    namespace Movement
    {
        class LocomotionSystem;
    }


    namespace Assets
    {
        class FbxInstanceRegistry;
    }

    namespace Components
    {
        class TransformSystem;
    }


    namespace Navigation
    {
		class NavigationSystem;
    }

    namespace Scene
    {
		class SceneManager;
    }

    namespace Render
    {
        class FbxRender;
        class CommandSystem;


        namespace Pipeline
        {
            class PipelineManager;
        }

        namespace PostProcess
        {
            class PostProcess;
        }

		namespace Lighting
		{
			class LightManager;
		}
    }


    namespace Animation
    {
        class AnimationController;
    }

    namespace Model::Lod
    {
        class ModelLodManager;
    }

    namespace Time
    {
        class TimeScaleManager;
    }


    struct EngineCoreConfig {
        uint32_t width;
        uint32_t height;
        std::wstring title;

        //いまは EngineCore では使わないが、Config として残す（上位層が使う可能性があるため）
        std::string pyRoot;
        std::string pySitePackages;

        EngineCoreConfig() = default;

        EngineCoreConfig(uint32_t w, uint32_t h, std::wstring t)
            : width(w), height(h), title(std::move(t)) {
        }

        EngineCoreConfig(uint32_t w, uint32_t h, std::wstring t,
            std::string pyRoot_, std::string pySitePackages_)
            : width(w),
            height(h),
            title(std::move(t)),
            pyRoot(std::move(pyRoot_)),
            pySitePackages(std::move(pySitePackages_)) {
        }
    };

    class EngineCore
    {
    public:
        EngineCore(const EngineCoreConfig& cfg);
        ~EngineCore();

        void Init(HWND& hwnd);

        //EngineServicesにはいったクラスのインスタンス化と初期化
        void InitEngineServices();

        void OnDestroy();

        // 1フレーム開始：CommandList Reset / PRESENT->RT 遷移 / RTV+DSV セット / クリア
        bool Clear();

        // 1フレーム終了：RT->PRESENT 遷移 / Execute / Present / Fence 同期 / FPS 更新
        void Flip();

        void PreDrawShadow();

        const EngineServices& GetServices() const { return services_; }

        unsigned int GetWindowWidth() const { return windowWidth_; }
        unsigned int GetWindowHeight() const { return windowHeight_; }

        const WCHAR* GetTiele() const { return title_.c_str(); }

        EngineServices& Services() { return services_; }

        const EngineServices& Services() const { return services_; }

        const PostProcessContext& GetPostProcessContext() const
        {
            return postProcessContext_;
        }

        ID3D12GraphicsCommandList* GetGraphicsListFrom()
        {
            return GraphicsListFrom(commandContext); 
        }

        //デバッグ用ワイヤーフレーム描画
        void DebugWireframeDraw();

        CommandContext& GetCommandContext()
        {
			return commandContext;
        }

        const CommandContext& GetCommandContext() const 
        {
            return commandContext;
        }

        ViewportScissor& GetViewScissor()
        {
			return viewScissor;
        }

        const ViewportScissor& GetViewScissor() const
        {
            return viewScissor;
        }

        void BeginFrame();

    private:
        Microsoft::WRL::ComPtr<IDXGIAdapter4> SelectAdapterWithMostVRAM(IDXGIFactory6* factory);
        void InitializeD3D12(HWND hwnd);

        //深度値関係///////
        void CreateDepthHeap();
        //描画で使用する深度値ようのリソースとビュー生成
        void CreateDepthBuffer();
        //影で使用するライトからみた深度値ようのリソースとビュー生成
        void CreateShadowDepthBuffer();

        void CreateDepthResourceView();

        /////////////////
        void UpdateFrameRate();

        void CreateCbvSrvUavHeap();

        //描画で必要なパイプラインの構築などを行う
        void InitRenderResources();

		//ここで、CommandContextに必要なクラスのインスタンスをセットする
        void BindCommandContext();


        //実行中のエグゼファイルがあるファイルパスを取得する
        static std::string GetExeDirectory()
        {
            wchar_t path[MAX_PATH]{};

            DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

            if (length == 0 || length >= MAX_PATH) {
                throw std::runtime_error("EXEディレクトリの取得に失敗しました。");
            }

            std::filesystem::path exePath(path);
            std::wstring widePath = exePath.parent_path().wstring();

            //UTF-16からUTF-8
            int size = WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                widePath.c_str(),
                static_cast<int>(widePath.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );

            if (size == 0) {
                throw std::runtime_error("UTF-8 変換失敗しました。");
            }

            std::string utf8Path(size, '\0');

            int result = WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                widePath.c_str(),
                static_cast<int>(widePath.size()),
                utf8Path.data(),
                size,
                nullptr,
                nullptr
            );

            if (result == 0) {
                throw std::runtime_error("UTF-8 変換失敗しました。");
            }

            return utf8Path;
        }
    private: //クラス類の宣言
        std::shared_ptr<CreateResource> createResource_;
        std::unique_ptr<TextRenderer> textRenderer_;
        std::unique_ptr<Render::FbxRender> fbxRenderer_;
        std::unique_ptr<ModelRenderSystem> modelRenderSystem_;
        std::unique_ptr<ModelLoadSystem> modelLoadSystem_;

        std::shared_ptr <FileManager> fileManager_;
        std::shared_ptr<CollisionAttributeManager> collisionAttributeManager_;
        std::shared_ptr<PythonEngine> pythonEngine_;
        
        std::unique_ptr<Render::Pipeline::PipelineManager> pipelineManager_;
        std::unique_ptr<Render::PostProcess::PostProcess> postProcess_;

        std::shared_ptr<LoadTex> loadTex_;
        std::unique_ptr <CameraManager> cameraManager_;

        std::unique_ptr <MouseInput> mouseInput_;
        std::unique_ptr<KeyInput> keyInput_;


        std::unique_ptr<TextInputManager> textInputManager_;
        std::unique_ptr<GameTimer> gameTimer_;

        std::shared_ptr<ComputeShaderManager> computeShaderManager_;

        std::shared_ptr<DisplacementMapping> displacementMapping_;
        std::shared_ptr<DebugDrawManager> debugDrawManager_;

        std::shared_ptr<FrustumCuller> frustumCuller_;

        std::shared_ptr<Compute::UpdateInstancePos::UpdateInstancePosCompute> updateInstancePosCompute_;

        std::shared_ptr<Navigation::NavigationSystem> navigationSystem_;

        std::unique_ptr<ChunkManager> chunkManager_;

        std::unique_ptr<UiManager> uiManager;

        std::unique_ptr<Primitive::Manager::PrimitiveManager> primitiveManager_;

        std::unique_ptr<PipelineStateObject> pipelineStateObject_;

		std::unique_ptr<CollisionSystem> collisionSystem_;

        std::unique_ptr<Screenshot> sreenshot_;

        std::unique_ptr<Scene::SceneManager> sceneManager_;

        std::unique_ptr<Components::TransformSystem> transformSystem_;
        
        std::unique_ptr<Assets::FbxInstanceRegistry> fbxInstance_;

        std::unique_ptr <Movement::LocomotionSystem>locomotion_;

		std::unique_ptr<Render::CommandSystem> commandSystem_;

        std::unique_ptr<Movement::GroundingSystem> groundingSystem_;

		std::unique_ptr<Animation::AnimationController> animationController_;

		std::unique_ptr<Model::Lod::ModelLodManager> modelLodManager_;

        std::unique_ptr<Render::Lighting::LightManager> lightManager_;

        std::unique_ptr<Time::TimeScaleManager> timeScaleManager_;
    private:
        HANDLE frameLatencyWaitableObject_ = nullptr;
        //window関係
        std::wstring title_;
        uint32_t windowWidth_{};
        uint32_t windowHeight_{};

        CD3DX12_VIEWPORT viewport_{};
        CD3DX12_RECT scissorRect_{};

        ViewportScissor viewScissor;
        //

        Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mainHeap;

        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> renderTargets_{ kFrameCount };
        Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
        Microsoft::WRL::ComPtr<ID3D12Resource> lightDepthBuffer_;

        UINT rtvDescriptorSize_ = 0;

        std::array<uint64_t, kFrameCount> frameFenceValues_{}; // 0 初期化
        uint32_t currentBbidx_ = 0;

        CommandContext commandContext{};

        EngineServices services_{};



        //FPS
        struct FrameRate
        {
            int frameCount = 0;
            float fps = 0.0f;
            std::chrono::high_resolution_clock::time_point prevTime = std::chrono::high_resolution_clock::now();
        };
        FrameRate frameRate_{};

        //オブジェクトの位置座標
        std::map<std::wstring, std::vector<FbxPos>> cPos_;
        
        //ポストプロセス初期化で必要なデータ
        PostProcessContext postProcessContext_;

    };
}
