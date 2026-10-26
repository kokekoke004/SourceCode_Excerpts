#include"SkinmeshAnimation.h"
#include"FbxAnimationData.h"
#include"FbxModel.h"
#include"FileManager.h"
#include"BoneMover.h"
#include"Engine/Model/FbxInstanceRegistry.h"
#include <algorithm>

using namespace std;
using namespace DirectX;



SkinmeshAnimation::SkinmeshAnimation(std::shared_ptr<FbxModel>& fbxModel, std::shared_ptr<FbxAnimationData>& fbxAnimationData, FileManager& fileManager, std::shared_ptr<BoneMover>& boneMover, Engine::Assets::FbxInstanceRegistry& fbxInstanceRegistry)
	:fbxModel(fbxModel), fbxAnimationData(fbxAnimationData), fileManager(fileManager),boneMover(boneMover), fbxInstanceRegistry_(fbxInstanceRegistry)
{

}

//FbxMatrixの行列をXMMATRIX型に変換
DirectX::XMMATRIX SkinmeshAnimation::ConvertMatrix(const FbxMatrix& fbxmat)
{
	XMMATRIX mat(
		(float)fbxmat[0][0], (float)fbxmat[0][1], (float)fbxmat[0][2], (float)fbxmat[0][3],
		(float)fbxmat[1][0], (float)fbxmat[1][1], (float)fbxmat[1][2], (float)fbxmat[1][3],
		(float)fbxmat[2][0], (float)fbxmat[2][1], (float)fbxmat[2][2], (float)fbxmat[2][3],
		(float)fbxmat[3][0], (float)fbxmat[3][1], (float)fbxmat[3][2], (float)fbxmat[3][3]
	);
	return mat;
}


void SkinmeshAnimation::setSkinMeshAnim(std::wstring animName)
{

	auto it = skinMeshAnim.find(animName);
	if (it == skinMeshAnim.end()) {
		ThrowMessageWstr(L"\nskinMeshAnimのキーが見つかりませんでした");
		return;
	}
	FbxAnimStack* animStack = it->second;
	if (!animStack) {
		ThrowMessageWstr(L"\nskinMeshAnimのポインタが無効です");
	}
	//アニメーションスタックをアクティブに設定
	fbxAnimationData->GetScene()->SetCurrentAnimationStack(animStack);

	//タイムレンジを取得
	FbxTimeSpan timeSpan;
	fbxAnimationData->GetScene()->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeSpan);

	//FbxTime startTime = timeSpan.GetStart();
	//FbxTime endTime = timeSpan.GetStop();
	//double durationInSeconds = endTime.GetSecondDouble();
	//FbxTime duration = endTime - startTime;

	//アニメーションが何フレームなのか取得
	FbxTime::EMode timeMode = fbxAnimationData->GetScene()->GetGlobalSettings().GetTimeMode();

	// アニメーションが何フレームなのか取得
	//int frameCount = static_cast<int>(durationInSeconds*fps);
	//ThrowMessageWstr(L"\n再生するアニメーション名：" + animName);


}

//スキンメッシュアニメーション
void SkinmeshAnimation::upDateAnimationSK(const std::wstring& drawFbxName, const std::wstring& animationStackName)
{
	//行列情報クリアしないと前のフレームのポーズが重ね掛けされてモデル壊れる
	fill(fbxAnimationData->GetBoneMatrices()[drawFbxName].begin(), fbxAnimationData->GetBoneMatrices()[drawFbxName].end(), XMMatrixIdentity());

	//仮
	auto elapsedTime = timeGetTime() - fbxAnimationData->GetAnimationFrameRate()[animationStackName].startTime;
	double currentTime = elapsedTime / 1000.0f;
	int frameNo = 60 * currentTime;

	//ThrowMessageWstr(L"\nFBX名 : " + fbxNameW + L"フレーム数 : " + to_wstring(frameNo));

	if (frameNo > fbxAnimationData->GetDuration(animationStackName)) {
		StartAnimationTime(animationStackName);
		//ThrowMessageWstr(L"\nFBX名 : " + fbxNameW + L"アニメーション再生時間 : " + to_wstring(elapsedTime / 1000.0f));
		//ThrowMessageWstr(L"\nFBX名 : " + fbxNameW + L"アニメーション再生開始！");
		frameNo = 0;
	}
	else if (frameNo == 0) {
		StartAnimationTime(animationStackName);
	}

	FbxTime t;
	t.SetSecondDouble(currentTime);

	for (int i = 0; i < fbxModel->M_numBones(drawFbxName); i++) {
		XMMATRIX m;
		//ｔでセットされた時間のアニメーション計算を行う
		m = ConvertMatrix(fbxAnimationData->GetAnimEvaluator()[drawFbxName]->GetNodeGlobalTransform(fbxModel->M_clusters(drawFbxName, i)->GetLink(), t));

		fbxModel->M_bones(drawFbxName, i).bindMatrix;
		XMMATRIX inv = XMMatrixInverse(0, fbxModel->M_bones(drawFbxName, i).bindMatrix);
		fbxAnimationData->GetBoneMatrices()[drawFbxName][i] = (inv * m);
	}

	//ThrowMessageWstr(L"\nアニメーションフレーム数：" + to_wstring(frameNo));

}



void SkinmeshAnimation::StartAnimationTime(const std::wstring& animationStackName)
{
	fbxAnimationData->GetAnimationFrameRate()[animationStackName].startTime = timeGetTime();
}

void SkinmeshAnimation::motionUpdate(const std::wstring& drawFbxName, const std::wstring& animationStackName)
{
	int idxNum = 0;

	//ThrowMessageWstr(L"\nアニメーション名 ：" + animName[idxNum] + L"-----------------------");

	//frameNo = 160;
	//行列情報クリアしないと前のフレームのポーズが重ね掛けされてモデル壊れる
	fill(fbxAnimationData->GetBoneMatrices()[drawFbxName].begin(), fbxAnimationData->GetBoneMatrices()[drawFbxName].end(), XMMatrixIdentity());
	//モーションデータ更新
	//frameNoと一致するボーンのアニメーションをこのループで値を渡して再生する

	for (auto& animStack : fbxAnimationData->GetMotiondata()) {
		auto elapsedTime = timeGetTime() - fbxAnimationData->GetAnimationFrameRate()[animStack.first].startTime;
		float frameFloat = fbxAnimationData->GetAnimationFrameRate().at(animStack.first).animationFps * (elapsedTime / 1000.0f);
		unsigned int frameNo = static_cast<unsigned int>(frameFloat);

		//_durationがブレンダーのアニメーションの最大フレームと一致しないのも原因確かめる
		if (frameNo > fbxAnimationData->GetDuration(animationStackName)) {
			StartAnimationTime(animStack.first);
			frameNo = 0;
		}


		for (auto& boneMotion : animStack.second) {

			auto& node = fbxModel->BoneNodeTable(drawFbxName, boneMotion.first);
			auto keyFrames = boneMotion.second;
			auto rit = find_if(keyFrames.rbegin(), keyFrames.rend(), [frameNo](const KeyFrame& keyframe) {
				return keyframe.frameNo <= frameNo;
				});
			if (rit == keyFrames.rend())continue;//合致するものがなければ飛ばす

			//アニメーションがあるフレームナンバーに存在しなかった
			auto it = rit.base();
			XMMATRIX rotation;
			if (it != keyFrames.end()) {
				auto t = static_cast<float>(frameNo - rit->frameNo) /
					static_cast<float>(it->frameNo - rit->frameNo);

				t = getYFromXOnBezier(t, it->p1, it->p2, 12);

				rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));


			}
			else {
				rotation = XMMatrixRotationQuaternion(rit->quaternion);
			}

			int i = rit->frameNo;

			//アニメーションの最小キーフレームと最大キーフレーム読み込むとおかしくなるから単位行列渡してcontinue
			if (i == 0 || i ==fbxAnimationData->GetDuration(animationStackName)) {
				fbxAnimationData->GetBoneMatrices()[drawFbxName][node.boneIdx] = XMMatrixIdentity();
				continue;
			}

			auto& pos = node.startPos;
			auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
				* rotation
				* XMMatrixTranslation(pos.x, pos.y, pos.z);
			fbxAnimationData->GetBoneMatrices()[drawFbxName][node.boneIdx] = mat;

			node.localRoteMatrix = rotation;

			//ThrowMessageWstr(L"\nアニメーションバグ:フレームNo : " + to_wstring(i));

			float x = XMVectorGetX(rit->quaternion);
			float y = XMVectorGetY(rit->quaternion);
			float z = XMVectorGetZ(rit->quaternion);
			float w = XMVectorGetW(rit->quaternion);
			//ThrowMessageWstr(L"\nアニメーション ： ボーン名 :" + boneMotion.first + L", frameNo :" + to_wstring(frameNo));
			//ThrowMessageWstr(L", Rotetion:(" + to_wstring(x) + L"," + to_wstring(y) + L"," + to_wstring(z) + L"," + to_wstring(w) + L")");
		}
	}
	//frameNoのアニメーション再生
	boneMover->moveBone(drawFbxName, fbxModel->FbxBones(drawFbxName, 0).boneName, XMMatrixIdentity());

}

std::filesystem::path SkinmeshAnimation::SkinMeshAnimPass(const std::wstring& fbxName)
{
	std::filesystem::path outPutPass = L"AnimationData";
	auto outPass = fbxName + L"-anim";
	outPutPass = outPutPass / outPass;
	return outPutPass;
}

void SkinmeshAnimation::SaveAnimationData(const std::wstring& drawFbxName)
{
	auto outPutPass = SkinMeshAnimPass(drawFbxName);
	const auto& animStacks = fbxAnimationData->GetAnimName();

	if (animStacks.size() != 0) {
		fileManager.WriteAnimationListFile(outPutPass, fbxAnimationData->GetAnimName());
	}
	for (const auto& animName : fbxAnimationData->GetAnimationData()) {
		wstring stackName = animName.first;
		int duration = fbxAnimationData->GetDuration(animName.first);
		int numBones = fbxModel->M_numBones(drawFbxName);

		//アニメーションスタック名でシーンを切り替える
		auto* animStack = fbxAnimationData->GetAnimStack()[stackName];
		fbxAnimationData->GetScene()->SetCurrentAnimationStack(animStack);

		//auto* evaluator = fbxAnimationData->GetScene()->GetAnimationEvaluator();

		vector<vector<XMMATRIX>> animData(duration);

		for (auto& frameVec : animData) {
			frameVec.reserve(numBones);
		}
		auto& gs = fbxAnimationData->GetScene()->GetGlobalSettings();
		auto mode = gs.GetTimeMode();

		for (int keyFrame = 0; keyFrame < duration; keyFrame++) {
			FbxTime t;
			fbxAnimationData->GetAnimationFrameRate();
			t.SetFrame(keyFrame, mode);

			//ボーンの数で初期化したマトリックスのベクター作成
			vector<XMMATRIX> mat(kMaxBones, XMMatrixTranspose(XMMatrixIdentity()));
			animData[keyFrame] = mat;
			for (int i = 0; i < fbxModel->M_numBones(drawFbxName); i++) {
				XMMATRIX m;
				//ｔでセットされた時間のアニメーション計算を行う
				m = ConvertMatrix(fbxAnimationData->GetAnimEvaluator()[stackName]->GetNodeGlobalTransform(fbxModel->M_clusters(drawFbxName, i)->GetLink(), t));

				fbxModel->M_bones(drawFbxName, i).bindMatrix;
				XMMATRIX inv = XMMatrixInverse(0, fbxModel->M_bones(drawFbxName, i).bindMatrix);
				animData[keyFrame][i]=(XMMatrixTranspose(inv * m));
			}

		}

		if (fileManager.ExportAnimationMatrices(outPutPass, animName.first,animData)) {

			wprintf(
				L"\n%ls",
				outPutPass.c_str()
			);
			printf(" へのアニメーション出力成功しました。");
		}

		if (fileManager.WriteAnimationToCSV(outPutPass, stackName, animData, fbxAnimationData->GetDuration(stackName), fbxModel->M_numBones(fbxModel->GetFbxName()))) {
			wprintf(
				L"\n%ls",
				outPutPass.c_str()
			);
			printf(" python経由でCSVファイルに書き出ししました");

			wprintf(
				L"\n%ls",
				stackName.c_str()
			);
			printf(" CSVファイルに書き出したアニメーションスタック名");


		}
		
	}


}

void SkinmeshAnimation::LoadAnimationData(const std::wstring& fbxName)
{
	auto outPutPass = SkinMeshAnimPass(fbxName);
	outPutPass = outPutPass / L"AnimationList.txt";

	vector<string> animPath;
	fileManager.readFileFbxPass(outPutPass, animPath);
	if (animPath.size() == 0) {
		return;
	}
	//インデックス０にアニメションを示すテキストファイルのペアレントパスが保存される
	std::filesystem::path parentPath = animPath[0];
	auto& frameBoneMatrices = fbxAnimationData->GetFrameBoneMatrices();
	int idx = 0;
	for (const auto& animName : animPath) {
		if (idx == 0) {
			idx++;
			continue;
		}
		auto path = parentPath;
		auto animTxt = animName + ".txt";
		auto animCSV= animName + ".csv";
		auto oldPath = path;
		auto pathTxt = path / animTxt;
		vector<vector<XMMATRIX>> animData;
		//fileManager->ImportAnimationData(pathTxt, animData);
		auto pathCSV = oldPath / animCSV;
		animData= fileManager.LoadAnimCSV(pathCSV);
		frameBoneMatrices[fbxName][UTF8_to_UTF16(animName)] = animData;
		printf("\nアニメーション %s を読込ました。", animName.c_str());

		idx++;
	}

}

void SkinmeshAnimation::LoadAnimationDataPosRoteScale(const std::wstring& fbxName)
{
	auto outPutPass = SkinMeshAnimPass(fbxName);
	outPutPass = outPutPass / L"AnimationList.txt";

	vector<string> animPath;
	fileManager.readFileFbxPass(outPutPass, animPath);
	if (animPath.size() == 0) {
		return;
	}
	//インデックス０にアニメションを示すテキストファイルのペアレントパスが保存される
	std::filesystem::path parentPath = animPath[0];
	auto& frameBoneMatrices = fbxAnimationData->GetFrameBoneMatrices();
	int idx = 0;
	for (const auto& animName : animPath) {
		if (idx == 0) {
			idx++;
			continue;
		}
		auto path = parentPath;
		auto animCSV = animName +"_PosRoteScale"  + ".csv";
		vector<vector<XMMATRIX>> animData;
		auto pathCSV = path / animCSV;
		animData = fileManager.LoadAnimCSV_PosRoteScale(pathCSV);
		frameBoneMatrices[fbxName][UTF8_to_UTF16(animName)] = animData;
		printf("\nアニメーション %s を読込ました。", animName.c_str());

		idx++;
	}

}

void SkinmeshAnimation::SaveAnimationToPosRoteSCale(const std::wstring& drawFbxName)
{
	auto outPutPass = SkinMeshAnimPass(drawFbxName);

	for (const auto& animName : fbxAnimationData->GetAnimationData()) {
		wstring stackName = animName.first;
		int duration = fbxAnimationData->GetDuration(animName.first);
		int numBones = fbxModel->M_numBones(drawFbxName);

		//アニメーションスタック名でシーンを切り替える
		auto* animStack = fbxAnimationData->GetAnimStack()[stackName];
		fbxAnimationData->GetScene()->SetCurrentAnimationStack(animStack);

		//auto* evaluator = fbxAnimationData->GetScene()->GetAnimationEvaluator();

		vector<vector<XMMATRIX>> animData(duration);


		for (auto& frameVec : animData) {
			frameVec.reserve(numBones);
		}
		auto& gs = fbxAnimationData->GetScene()->GetGlobalSettings();
		auto mode = gs.GetTimeMode();
		
		const float toDeg = 180.0f / XM_PI;
		for (int keyFrame = 0; keyFrame < duration; keyFrame++) {
			FbxTime t;
			fbxAnimationData->GetAnimationFrameRate();
			t.SetFrame(keyFrame, mode);

			//ボーンの数で初期化したマトリックスのベクター作成
			vector<XMMATRIX> mat(kMaxBones, XMMatrixTranspose(XMMatrixIdentity()));
			animData[keyFrame] = mat;
			for (int i = 0; i < fbxModel->M_numBones(drawFbxName); i++) {
				XMMATRIX m;
				//ｔでセットされた時間のアニメーション計算を行う
				m = ConvertMatrix(fbxAnimationData->GetAnimEvaluator()[stackName]->GetNodeGlobalTransform(fbxModel->M_clusters(drawFbxName, i)->GetLink(), t));
				XMMATRIX inv = XMMatrixInverse(0, fbxModel->M_bones(drawFbxName, i).bindMatrix);

				animData[keyFrame][i] = inv*m;

			}

		}
		if (fileManager.SaveAnimationToPosRoteSCale(outPutPass,stackName,animData, fbxAnimationData->GetDuration(stackName), fbxModel->M_numBones(fbxModel->GetFbxName()))) {
			wprintf(
				L"\n%ls",
				outPutPass.c_str()
			);
			printf(" python経由でCSVファイルに書き出ししました");

			wprintf(
				L"\n%ls",
				stackName.c_str()
			);
			printf(" CSVファイルに書き出したアニメーションスタック名");


		}



	}

}

void SkinmeshAnimation::PlayAnimation(const std::wstring& fbxName, const std::wstring& keyDrawFbx, const std::wstring& animationStackName)
{
	const auto& frameBoneMatricesAll = fbxAnimationData->GetFrameBoneMatrices();
	auto itFbx = frameBoneMatricesAll.find(fbxName);
	if(itFbx == frameBoneMatricesAll.end()) {
		ThrrowError("PlayAnimation : 登録されていないfbxNameです。");
		return;
	}

	const auto& animStackName = itFbx->second;
	auto itAnim = animStackName.find(animationStackName);
	if (itAnim == animStackName.end()) {
		ThrrowError("PlayAnimation : 登録されていないanimationStackNameです。");
		return;
	}


	const auto& animation = frameBoneMatricesAll.at(keyDrawFbx).at(animationStackName);

	const auto& animFps = fbxAnimationData->GetAnimationFrameRate().at(animationStackName);

	auto elapsedTime = timeGetTime() - animFps.startTime;
	double fps= animFps.animationFps;
	float frameFloat = fps * (elapsedTime / 1000.0f);
	unsigned int frameNo = static_cast<unsigned int>(frameFloat);



	//_durationがブレンダーのアニメーションの最大フレームと一致しないのも原因確かめる
	int animIdx=fbxAnimationData->GetAnimationNameToIndexToIndex()[fbxInstanceRegistry_.GetBatches().at(fbxName).instances[0].animationName];
	if (frameNo > fbxAnimationData->GetDuration(animationStackName)) {
		StartAnimationTime(animationStackName);
		frameNo = 0;
	}
	//frameNo = 160;
	//行列情報クリアしないと前のフレームのポーズが重ね掛けされてモデル壊れる
	fill(fbxAnimationData->GetBoneMatrices()[fbxName].begin(), fbxAnimationData->GetBoneMatrices()[fbxName].end(), XMMatrixIdentity());

	if (animation.size() - 1 < frameNo) {
		StartAnimationTime(animationStackName);
		frameNo = 0;
	}


	fbxAnimationData->GetAnimationInstance(keyDrawFbx).animationStackName = animationStackName;
	fbxAnimationData->GetAnimationInstance(keyDrawFbx).drawFbxName = fbxName;
	fbxAnimationData->GetAnimationInstance(keyDrawFbx).frame = frameNo;


	auto& mat = fbxAnimationData->GetBoneMatrices()[keyDrawFbx];


	mat = animation[frameNo];

}

float SkinmeshAnimation::getYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y) {
		return x;
	}

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * a.x - 3 * b.x;	//t~3の係数
	const float k1 = 3 * b.x - 6 * a.x;	//t~2の係数
	const float k2 = 3 * a.x;

	//誤差範囲内かどうかに使用する定数
	constexpr float epsilon = 0.0005f;

	//tを近似で求める
	for (int i = 0; i < n; ++i) {
		//f(t)を求める
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;

		//もし結果が0に近く誤差の範囲内ならば打ち切る
		if (ft <= epsilon && ft >= -epsilon) {
			break;
		}
		t -= ft / 2;	//刻む
	}

	//求めたいtはすでに求まっているのでyを計算する
	auto r = 1 - t;
	return (t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y);
}


AnimationData& SkinmeshAnimation::GetAnimationData(const std::wstring& stackName, const std::wstring& boneNameW, UINT frameNum)
{
	return fbxAnimationData->GetAnimationData(stackName, boneNameW, frameNum);
}

const AnimationData& SkinmeshAnimation::GetAnimationData(const std::wstring& stackName, const std::wstring& boneNameW, UINT frameNum) const
{
	return fbxAnimationData->GetAnimationData(stackName, boneNameW, frameNum);
}

void SkinmeshAnimation::LoadAnimationFromPath(const std::wstring fbxName, const std::wstring animName, const std::wstring& animationPass)
{
	auto& frameBoneMatrices = fbxAnimationData->GetFrameBoneMatrices();
	vector<vector<XMMATRIX>> animData = fileManager.LoadAnimCSV_PosRoteScale(animationPass);
	frameBoneMatrices[fbxName][animName] = animData;

}