#include <string>
#include <functional>
#include <memory>
#include <tuple>    
#include <utility> 
#include <type_traits>
#include "IScene.h"
#include "IndexedRegistry.h"
#include"Engine/FrameContext.h"

namespace Engine::Scene {


    class SceneManager
    {
    public:
        // シーン生成関数の型定義
        using SceneFactory = std::function<std::unique_ptr<IScene>()>;
        using SceneRegistry = IndexedRegistry<std::wstring, SceneFactory>;

        //シーン生成関数の作成ヘルパー： MakeSceneFactory<Scene::GameplayScene>(引数) こんな感じで使う
        //引数で参照を渡すなら、std::ref(engine)こんな感じで渡す
        template<class T, class... Args>
        static SceneFactory MakeSceneFactory(Args&&... args)
        {
            // args... をコピー/ムーブして lambda に保持しておき、生成時に渡す
            using TupleT = std::tuple<std::decay_t<Args>...>;
            auto captured = std::make_shared<TupleT>(std::forward<Args>(args)...);

            return [captured]() mutable
                {
                    return std::apply(
                        //タプルに格納されたものからここでインスタンス生成している
                        [](auto&... xs) { return std::make_unique<T>(xs...); },
                        *captured
                    );
                };
        }

        //シーンの登録（wstringをキーにする）
        void RegisterScene(const std::wstring& name, SceneFactory factory) {
            registry.InsertUnique(name, std::move(factory));
        }

		//即時シーン切り替え
        void ChangeSceneNow(const std::wstring& name) {
            nextSceneKey = name;
            ExecuteSceneChange();
        }

        //シーン切り替え予約
        void QueueSceneChange(const std::wstring& name) {
            nextSceneKey = name;
        }

        void Update(::Engine::FrameContext& frameContext) {
            //シーン切り替えのリクエストがあるかチェック
            if (!nextSceneKey.empty()) {
                ExecuteSceneChange();
            }

            if (currentScene) {
                currentScene->Update(frameContext);
            }
        }

        void Draw() {
            if (currentScene) {
                currentScene->Draw();
            }
        }

    private:
        void ExecuteSceneChange() {
            // Registryから生成関数を取得
            auto factory = registry.GetDataByKey(nextSceneKey);

            if (factory) {
                if (currentScene) {
                    currentScene->Finalize();
                    // 必要ならここで先に解放してVRAM/リソース圧を下げる
                    // currentScene.reset();
                }

                // 新しいシーンをインスタンス化
                currentScene = (*factory)();
                currentScene->Initialize();

                currentSceneKey = nextSceneKey;
            }
            else {
                // 未登録キーの場合は何も起きないので、ここでログ/アサート入れるとデバッグしやすい
                // assert(false);
            }

            nextSceneKey.clear();
        }

        // キー：wstring, 値：生成関数
        SceneRegistry registry;

        std::unique_ptr<IScene> currentScene;
        std::wstring currentSceneKey;
        std::wstring nextSceneKey;
    };
}
