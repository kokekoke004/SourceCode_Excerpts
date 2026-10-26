#pragma once
#include <cstdint>
#include <vector>
#include"AnimationTypes.h"


namespace Engine::Animation
{
    struct AnimState;

    class GpuClipPlayer {
    public:

		//指定したアニメーションクリップを再生する（アニメーションの設定など）
        //再生するアニメーションを変えるときとかに呼び出す
        //animState : アニメーションの再生状態管理する構造体
		//clip : 再生するアニメーションデータのインデックス
		//loop : アニメーションをループ再生するかどうか
		//speed : アニメーションの再生速度（1.0fが通常速度）
		//restart : trueの場合、再生開始時にアニメーションを最初から再生する。falseの場合、現在のフレーム位置を維持して再生する。
        void Play(AnimState& animState,uint32_t clip, bool loop, float speed = 1.0f, bool restart = true);

		//アニメーションの再生状態を更新する
        //animState : アニメーションの再生状態管理する構造体
        //meta : アニメーションクリップのメタデータ（フレーム数やFPSなど）
		//dt : １フレームごとの時間
        void Update(AnimState& animState, const std::vector<AnimClipMeta>* meta,float dt);

		//再生中のアニメーションクリップのインデックスを取得する
        uint32_t Clip(const AnimState& animState)const;
		//再生中のアニメーションの現在フレームを取得する
        uint32_t Frame(const AnimState& animState)const;
    private:

    };
}