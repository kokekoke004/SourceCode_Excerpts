#include"GpuClipPlayer.h"
#include"AnimationState.h"
#include <cmath>
#include<algorithm>

namespace Engine::Animation
{
    void GpuClipPlayer::Play(AnimState& animState, uint32_t clip, bool loop, float speed , bool restart )
    {
		animState.clipId = clip;
		animState.loop = loop;
		animState.speed = speed;
		animState.playing = true;

        if (restart) {
            animState.time = 0.0f;
			animState.frame = 0;
        }
    }

    void GpuClipPlayer::Update(AnimState& animState, const std::vector<AnimClipMeta>* meta, float dt)
    {
		//再生中で、メタデータがセットされていて、クリップインデックスが有効な範囲内であることを確認
        if (!animState.playing || !meta || animState.clipId >= meta->size()) return;
		//現在のクリップのメタデータを取得
        const auto& anim = (*meta)[animState.clipId];
		//フレーム総数が0の場合は、フレームインデックスを0にして終了
        if (anim.frameCount == 0 || anim.fps <= 0.0f) {
            animState.frame = 0;
            return;
        }
		//時間を更新して、現在のフレームインデックスを計算
        animState.time += dt * animState.speed;
		//フレームインデックスを計算
        //ループ再生の場合は、フレーム総数で割った余りを使用
        //非ループ再生の場合は、フレーム総数-1を上限とする
        if (animState.loop) {
            //アニメーションの1周の秒数
            const float duration = static_cast<float>(anim.frameCount) / anim.fps;
            //再生するアニメーションの総時間こえたらここで時間をはじめに巻き戻す処理
            animState.time = std::fmod(animState.time, duration);
			//time_が負の値になる可能性があるため、正の値に変換
            if (animState.time < 0.0f) animState.time += duration;
        }
		//フレームインデックスを計算
        uint32_t fi = static_cast<uint32_t>(animState.time * anim.fps);
		//フレームインデックスが有効な範囲内に収まるようにする
        animState.frame = (std::min)(fi, anim.frameCount - 1);
    }

    uint32_t GpuClipPlayer::Clip(const AnimState& animState) const
    {
        return animState.clipId;
    }
    //再生中のアニメーションの現在フレームを取得する
    uint32_t GpuClipPlayer::Frame(const AnimState& animState) const
    {
        return animState.frame;
    }


}