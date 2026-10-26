#pragma once


namespace Primitive::Manager
{
	class PrimitiveManager;
}

namespace Engine::Render::Pipeline
{
    class PipelineManager;
}


class KeyInput;
class MouseInput;
class CameraManager;
class DebugDrawManager;
class TextInputManager;
class GameTimer;
class DisplacementMapping;
class ComputeShaderManager;
class FrustumCuller;
class ChunkManager;
class UiManager;
class CollisionSystem;
class CollisionAttributeManager;
class FileManager;
class Screenshot;
class PythonEngine;
class LoadTex;
class CreateResource;

namespace Engine
{
    class IModelLoader;
	class IModelProvider;
    class IModelRenderer;
    class ITextRenderer;

    namespace Model::Lod
    {
		class ModelLodManager;
    }

    namespace Animation
    {
        class AnimationController;
    }

    namespace Assets
    {
        class FbxInstanceRegistry;
    }


    namespace Components
    {
        class TransformSystem;
    }

    namespace Render
    {
        namespace PostProcess
        {
            class IPostProcess;
        }

        namespace Lighting
        {
            class LightManager;

        }


        class CommandSystem;
    }

    namespace Navigation
    {
        class NavigationSystem;

    }

    namespace Movement
    {
        class LocomotionSystem;
        class GroundingSystem;
    }


    namespace Scene
    {
		class SceneManager;
    }

    namespace Time
    {
        class TimeScaleManager;
    }

    struct EngineServices
    {

        //キー入力
        KeyInput* key = nullptr;
        
        //マウス入力
        MouseInput* mouse = nullptr;
        
        //カメラ制御系
        CameraManager* camera = nullptr;

        //ファイル管理
		FileManager* fileManager = nullptr;

        //スクリーンショット撮影
		Screenshot* screenshot = nullptr;

        //プリミティブ関連
        Primitive::Manager::PrimitiveManager* primitive = nullptr;

        //デバッグマネージャ
        DebugDrawManager* debugDrawManager = nullptr;

        //モデルのローダー
        IModelLoader* modelLoader = nullptr;

        //モデルを取得する専用
        IModelProvider* modelProvider = nullptr;

        //モデルの描画
        IModelRenderer* modelRenderer = nullptr;
        
        //テキストの描画   
        ITextRenderer* textRenderer = nullptr;

        //ポストプロセスに関する処理
        Render::PostProcess::IPostProcess* postProcess = nullptr;

        //ウィンドウズシステムに入力する文字を伝える時に使ったりする
        TextInputManager* textInputManager = nullptr;

        //ゲームで使う時間を測るクラス
        GameTimer* gameTimer = nullptr;

        DisplacementMapping* displacementMapping = nullptr;

        //コンピュートシェーダクラスインスタンス化
        ComputeShaderManager* computeShaderManager = nullptr;

		//フラスタムカリングクラスインスタンス化
		FrustumCuller* frustumCuller = nullptr;

		//チャンクマネージャー
		ChunkManager* chunkManager = nullptr;

        //経路探索
        Navigation::NavigationSystem* navigationSystem = nullptr;

        //UI関連
		UiManager* uiManager = nullptr;

        //コリジョンシステム
		CollisionSystem* collisionSystem = nullptr;

        //コリジョン特性（現状は使わないが後々使うかもしれないから残しておく）
        CollisionAttributeManager* collisionAttribute = nullptr;

        //パイソン側のコードの実行など
		PythonEngine* pythonEngine = nullptr;

        //テクスチャをロードする
        LoadTex* loadTex = nullptr;

        //パイプラインを管理する
		Render::Pipeline::PipelineManager* pipelineManager = nullptr;

        //リソースを生成する
        CreateResource* createResource = nullptr;

        //ゲームのシーン管理
		Scene::SceneManager* sceneManager = nullptr;

        //モデルの描画位置の管理
        Components::TransformSystem* transform = nullptr;

        //FBXの特性情報の管理
        Assets::FbxInstanceRegistry* fbxInstance = nullptr;

        //経路探索結果から実際に移動するときなどに使う
        Movement::LocomotionSystem* locomotion = nullptr;

        //コマンドリスト動機など
		Render::CommandSystem* commandSystem = nullptr;

		//モデルの地形への接地判定や、接地しているかの判定などを行うクラス
		Movement::GroundingSystem* groundingSystem = nullptr;

		//アニメーション関連の再生や、アニメーションの更新などを行うクラス
		Animation::AnimationController* animationController = nullptr;

		//LODの管理クラス
		Model::Lod::ModelLodManager* modelLodManager = nullptr;

		//ライトの管理クラス
		Render::Lighting::LightManager* lightManager = nullptr;

        //時間倍率を返却する
        Time::TimeScaleManager* timeScaleManager = nullptr;
    };
}