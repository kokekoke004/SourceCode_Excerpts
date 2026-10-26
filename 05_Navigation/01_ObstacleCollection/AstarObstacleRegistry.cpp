#include"AstarObstacleRegistry.h"
#include"Engine/IModelProvider.h"
#include"FbxLoader.h"
#include"Engine/Model/FbxInstanceRegistry.h"
using namespace DirectX;

AstarObstacleRegistry::AstarObstacleRegistry(Engine::IModelProvider& iModelProvider, Engine::Assets::FbxInstanceRegistry& fbxInstance)
	:iModelProvider_(&iModelProvider),fbxInstance_(fbxInstance)
{

}


void AstarObstacleRegistry::SetObstacle()
{
	//障害物情報を初期化してからセットする
	obData.clear();
	for (auto fbx : iModelProvider_->GetAllModels()) {
		auto insRec = fbxInstance_.GetInstanceRecords(fbx->GetFbxName());
		for (auto ins : insRec) {

		}
		if (insRec.empty()) {
			continue;
		}
		if (insRec[0].entityType == PLAYER || insRec[0].entityType == NPC) {
			continue;
		}

		auto& colliders=fbx->Collider();
		AddObstacle(colliders);
	}
}

// 障害物を登録
void AstarObstacleRegistry::AddObstacle(const std::vector<ColliderDesc>& colliders)
{
	for (auto& colli:colliders) {
		if (colli.shape != EColliderShape::OBB) {
			continue;
		}

		ObstacleData obD;

		XMMATRIX mRot = {  // Axis0,1,2 を列にした行列
			colli.obb.m_NormaDirect[0].x, colli.obb.m_NormaDirect[0].y, colli.obb.m_NormaDirect[0].z, 0,
			colli.obb.m_NormaDirect[1].x, colli.obb.m_NormaDirect[1].y, colli.obb.m_NormaDirect[1].z, 0,
			colli.obb.m_NormaDirect[2].x, colli.obb.m_NormaDirect[2].y, colli.obb.m_NormaDirect[2].z, 0,
			0,       0,       0,       1,
		};

		XMVECTOR q = XMQuaternionRotationMatrix(mRot);

		XMStoreFloat4(&obD.rotQuat, q);


		obD.halfExtents.x= colli.obb.m_fLength[0];   //width
		obD.halfExtents.y = colli.obb.m_fLength[1]; //height
		obD.halfExtents.z = colli.obb.m_fLength[2]; //depth

		//Aスターでは、OBBの原点としての座標でなく、FBXの座標として扱うから、OBBのセンターから半分の高さYを引いてFBXの座標に修正する
		auto obbPos=colli.obb.m_Pos;
		//obbPos.y += wy;
		obD.position= obbPos;			   //位置座標
		obData.emplace_back(obD);
	}
}