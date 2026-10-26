#include"AnimationController.h"

namespace Engine::Animation
{
	AnimationController::AnimationController()
	{
		gpuClipPlayer_ = GpuClipPlayer();
	}

	AnimationController::~AnimationController() = default;

	//GpuClipPlayer関連///////////////
	void AnimationController::Play(AnimState& animState, uint32_t clip, bool loop, float speed, bool restart)
	{
		gpuClipPlayer_.Play(animState,clip, loop, speed, restart);
	}

	void AnimationController::UpdateBatch(Assets::BatchMap& batchMap, float dt)
	{
		for (auto& [key, batch] : batchMap) {
			const auto* meta = &batch.animClipMeta;
			//アニメーションクリップのメタデータがセットされていない場合は、アニメーションの更新処理をスキップする
			if (meta->empty()) continue;

			//バッチ内の全てのインスタンスに対して、アニメーションの再生状態を更新する
			for (auto& inst : batch.instances)
			{
				if (inst.animationType == NONE) continue;
				gpuClipPlayer_.Update(inst.animState, meta, dt);

				auto& state = inst.animState;
				if (state.clipId != state.lastSentClipId || state.frame != state.lastSentFrame) {
					animDirtyList_.push_back(inst.transformId);
					state.lastSentClipId = state.clipId;
					state.lastSentFrame = state.frame;
				}
			}

		}
	}

	std::vector<Components::TransformId> AnimationController::ConsumeAnimDirty()
	{
		auto out = std::move(animDirtyList_);
		animDirtyList_.clear();
		return out;
	}

	uint32_t AnimationController::Clip(const AnimState& animState) const
	{
		return gpuClipPlayer_.Clip(animState);
	}

	uint32_t AnimationController::Frame(const AnimState& animState) const
	{
		return gpuClipPlayer_.Frame(animState);
	}

	/////////////////////////////////

}