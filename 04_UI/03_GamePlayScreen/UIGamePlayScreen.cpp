#include"UIGamePlayScreen.h"
#include"GameUIScreenKeys.h"
#include"UIPartsGamePlayScreen.h"

#include"UIResourcePanel.h"
#include"UIGamePlayScreenTopBar.h"
#include"UIGamePlayScreenUnderBar.h"
#include"UIGamePlayScreenResidentPanel.h"
#include"UIGamePlayScreenBuildPanel.h"
#include"UIBuildDetailPanel.h"
#include"UIBuildingInfoPanel.h"
#include"UIGamePlayFieldDetailPanel.h"
#include"ItemMasterIconProvider.h"
#include"ItemInventory.h"

#include"CharacterSystem.h"
#include"CharacterBase.h"
#include"ObjectAttributeManager.h"
#include"GameTimeSystemUtility.h"
#include"AgricultureSystem.h"
#include"BuildingDatabase.h"
#include"TransformSystem.h"
#include"StorageSystem.h"

namespace Game::UI::Screens
{
	UIGamePlayScreen::UIGamePlayScreen(UIService& uiService,
		UIContext& uiContext,
		const UIPipeline& uiPipe,
		const ViewportScissor& viewportScissor)
		:uiCommonServices_(uiService,uiContext,uiPipe,viewportScissor)
	{
		GetNowUiScreenKey() = Game::UI::ScreenKeys::GamePlay;
		GetChangeUiScreenKey() = GetNowUiScreenKey();

	}

	UIGamePlayScreen::~UIGamePlayScreen() = default;

	void UIGamePlayScreen::SetGametime(Game::Time::GameDateTime gameDateTime)
	{
		topBar_->SetGametime(gameDateTime);
	}

	Game::UI::GamePlayScreen::Parts::SelectedBuildData UIGamePlayScreen::GetSelectedBuildData() const
	{
		if (buildDetailPanel_) {
			return buildDetailPanel_->GetSelectedBuildData();
		}
		return Game::UI::GamePlayScreen::Parts::SelectedBuildData{};
	}

	void UIGamePlayScreen::ClearSelectedBuildData()
	{
		buildDetailPanel_->ClearSelectedBuildData();
	}

	void UIGamePlayScreen::SetTargetField(Engine::Components::TransformId fieldTid)
	{
		uiGamePlayFieldDetailPanel_->SetTargetField(fieldTid);
	}

	void UIGamePlayScreen::ClearTargetField()
	{
		uiGamePlayFieldDetailPanel_->ClearTargetField();
	}

	std::optional<Game::Character::CharacterId> UIGamePlayScreen::ConsumeCameraFollowRequest()
	{
		return residentPanel_->ConsumeCameraFollowRequest();
	}

	void UIGamePlayScreen::CreateParts(Item::Master::ItemMasterIconProvider& iconProvider,
		Item::Runtime::Inventory& inventory, Game::Character::CharacterSystem& characterSystem, Game::Object::ObjectAttributeManager& objectAttributeManager, 
		Game::Build::BuildingDatabase& buildingDatabase, Game::Timer::GameSpeedController& gameSpeedController , Game::Agriculture::AgricultureSystem& agricultureSystem,
		Game::World::Storage::StorageSystem& storageSystem)
	{
		//リソースパネルインスタンス化
		resourcePanel_.reset(new Parts::UIResourcePanel(uiCommonServices_.uiService, uiCommonServices_.uiContext, iconProvider, inventory));
		uiParts_.push_back(resourcePanel_.get());
		//トップバーインスタンス化
		topBar_.reset(new Game::UI::GamePlayScreen::Parts::UIGamePlayScreenTopBar(uiCommonServices_.uiService, uiCommonServices_.uiContext, gameSpeedController));
		uiParts_.push_back(topBar_.get());

		//アンダーバーインスタンス化
		underBar_.reset(new Game::UI::GamePlayScreen::Parts::UIGamePlayScreenUnderBar(uiCommonServices_.uiService, uiCommonServices_.uiContext));
		uiParts_.push_back(underBar_.get());

		//レジデントパネルインスタンス化
		residentPanel_.reset(new Game::UI::GamePlayScreen::Parts::UIGamePlayScreenResidentPanel(uiCommonServices_.uiService, uiCommonServices_.uiContext, characterSystem, iconProvider));
		uiParts_.push_back(residentPanel_.get());

		Game::UI::GamePlayScreen::Parts::UIGamePlayScreenBuildPanelService buildPanelService(objectAttributeManager, buildingDatabase, iconProvider);
		//ビルドパネルインスタンス化
		buildPanel_.reset(new Game::UI::GamePlayScreen::Parts::UIGamePlayScreenBuildPanel(uiCommonServices_.uiService, uiCommonServices_.uiContext, buildPanelService));
		uiParts_.push_back(buildPanel_.get());

		//ビルド詳細パネルインスタンス化
		buildDetailPanel_.reset(new Game::UI::GamePlayScreen::Parts::UIBuildDetailPanel(uiCommonServices_.uiService, uiCommonServices_.uiContext));
		uiParts_.push_back(buildDetailPanel_.get());

		//建築物にカーソル合わせたときに表示するUI
		uiBuildingInfoPanel_.reset(new Game::UI::GamePlayScreen::Parts::UIBuildingInfoPanel(uiCommonServices_.uiService, uiCommonServices_.uiContext, iconProvider));
		uiParts_.push_back(uiBuildingInfoPanel_.get());

		//畑の詳細UIの初期化
		uiGamePlayFieldDetailPanel_.reset(new Game::UI::GamePlayScreen::Parts::UIGamePlayFieldDetailPanel(uiCommonServices_.uiService, uiCommonServices_.uiContext, agricultureSystem, characterSystem, storageSystem));
		uiParts_.push_back(uiGamePlayFieldDetailPanel_.get());


		//すべてのUIパーツのインスタンス化完了したら実行する
		LoadResources();
		SetupLayout();
	}

	void UIGamePlayScreen::LoadResources()
	{
		for (auto* ui : uiParts_) {
			if (ui) {
				ui->LoadResources();
			}
		}

	}

	void UIGamePlayScreen::SetupLayout()
	{
		for (auto* ui : uiParts_) {
			if (ui) {
				ui->SetupLayout();
			}
		}


	}

	void UIGamePlayScreen::Initialize()
	{
		for (auto* ui : uiParts_) {
			if (ui) {
				ui->Initialize();
			}
		}

	}

	void UIGamePlayScreen::Update()
	{
		if (resourcePanel_) {
			resourcePanel_->Update();
		}

		if (topBar_) {
			topBar_->Update();
		}

		if (underBar_) {
			underBar_->Update();
		}


		if (uiBuildingInfoPanel_) {
			uiBuildingInfoPanel_->Update();
		}

		if (uiGamePlayFieldDetailPanel_) {
			uiGamePlayFieldDetailPanel_->Update();
		}


		//住人パネルは、アンダーバーのどのボタンが押されているかによって、表示する内容が変わるため、UpdateResidentPanel関数で更新する
		UpdateUnderBarPanel();



	}

	void UIGamePlayScreen::Draw()
	{
		if (resourcePanel_) {
			resourcePanel_->Draw();
		}

		if (topBar_) {
			topBar_->Draw();
		}


		if (underBar_) {
			underBar_->Draw();
		}

		if (uiBuildingInfoPanel_) {
			uiBuildingInfoPanel_->Draw();
		}


		if (uiGamePlayFieldDetailPanel_ && uiGamePlayFieldDetailPanel_->GetTargetFieldTid().has_value()) {
			uiGamePlayFieldDetailPanel_->Draw();
		}
		else {
			DrawResidentPanel();
		}



	}

	void UIGamePlayScreen::Exit()
	{
		for (auto* ui : uiParts_) {
			if (ui) {
				ui->Exit();
			}
		}

	}

	void UIGamePlayScreen::DrawUIText()
	{
		if (resourcePanel_) {
			resourcePanel_->DrawUIText();
		}

		if (topBar_) {
			topBar_->DrawUIText();
		}

		if (underBar_) {
			underBar_->DrawUIText();
		}

		if (uiBuildingInfoPanel_) {
			uiBuildingInfoPanel_->DrawUIText();
		}

		if (uiGamePlayFieldDetailPanel_ && uiGamePlayFieldDetailPanel_->GetTargetFieldTid().has_value()) {
			uiGamePlayFieldDetailPanel_->DrawUIText();
		}
		else {
			DrawUITextResidentPanel();

		}





	}

	const std::optional<GamePlayScreen::Parts::UnderBarButtonDef>& UIGamePlayScreen::GetCurrentClickUnderBarButton() const
	{
		return underBar_->GetCurrentClickUnderBarButton();
	}

	const std::optional<GamePlayScreen::Parts::UnderBarButtonType>& UIGamePlayScreen::GetLastClickedUnderBarButtonType() const
	{
		return underBar_->GetLastClickedUnderBarButtonType();
	}

	bool UIGamePlayScreen::WasUnderBarButtonDeselected() const
	{
		return underBar_->WasUnderBarButtonDeselected();
	}

	void UIGamePlayScreen::GetBuildingInfo(const std::optional<Game::Build::BuildProgressData>& buildProgressData)
	{
		return uiBuildingInfoPanel_->GetBuildingInfo(buildProgressData);
	}

	void UIGamePlayScreen::UpdateUnderBarPanel()
	{
		auto buttonInfo = underBar_->GetCurrentClickUnderBarButton();

		if (!buttonInfo) {
			return;
		}

		switch (buttonInfo->buttonType) {
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Build:
			{
				//建築のUIを表示する処理
				if (buildPanel_) {
					buildPanel_->Update();
					auto selectedIconData = buildPanel_->GetSelectedBuildIconData();

					//もし建築アイコンが選択されていれば、建築詳細パネルを更新する
					if (selectedIconData.isSelected && buildDetailPanel_) {
						//建築詳細パネルに選択されている建築アイコンのデータをセットして、建築詳細パネルを更新する
						buildDetailPanel_->SetSelectedBuildIconData(selectedIconData);
						buildDetailPanel_->Update();
					}
				}



				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Production:
			{
				//生産のUIを表示する処理
				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Research:
			{
				//研究のUIを表示する処理
				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Resident:
			{
				//住人のUIを更新する処理
				if (residentPanel_) {
					residentPanel_->Update();
				}
				break;
			}

			default:
			{
				break;
			}
		}



	}

	void UIGamePlayScreen::DrawResidentPanel()
	{
		auto buttonInfo = underBar_->GetCurrentClickUnderBarButton();

		if (!buttonInfo) {
			return;
		}

		switch (buttonInfo->buttonType) {
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Build:
			{
				//建築のUIを表示する処理
				if (buildPanel_) {
					buildPanel_->Draw();
					auto selectedIconData = buildPanel_->GetSelectedBuildIconData();
					//もし建築アイコンが選択されていれば、建築詳細パネルを描画する
					if (selectedIconData.isSelected && buildDetailPanel_) {
						buildDetailPanel_->Draw();
					}

				}

				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Production:
			{
				//生産のUIを表示する処理
				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Research:
			{
				//研究のUIを表示する処理
				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Resident:
			{
				//住人のUIを更新する処理
				if (residentPanel_) {
					residentPanel_->Draw();
				}
				break;
			}
			default:
			{
				break;
			}

		}

	}

	void UIGamePlayScreen::DrawUITextResidentPanel()
	{
		auto buttonInfo = underBar_->GetCurrentClickUnderBarButton();

		if (!buttonInfo) {
			return;
		}

		switch (buttonInfo->buttonType) {
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Build:
			{
				if (buildPanel_) {
					buildPanel_->DrawUIText();
					auto selectedIconData = buildPanel_->GetSelectedBuildIconData();
					//もし建築アイコンが選択されていれば、建築詳細パネルのテキストを描画する
					if (selectedIconData.isSelected && buildDetailPanel_) {
						buildDetailPanel_->DrawUIText();
					}

				}

				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Production:
			{
				//生産のUIを表示する処理
				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Research:
			{
				//研究のUIを表示する処理
				break;
			}
			case Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Resident:
			{
				//住人のUIを更新する処理
				if (residentPanel_) {
					residentPanel_->DrawUIText();
				}
				break;
			}
			default:
			{
				break;
			}

		}

	}

	UIHitResult UIGamePlayScreen::IsHoveringUI()
	{
		if (resourcePanel_) {
			if (resourcePanel_->IsHoveringUI().isHit) {
				return resourcePanel_->IsHoveringUI();
			}
		}

		if (topBar_) {
			if (topBar_->IsHoveringUI().isHit) {
				return topBar_->IsHoveringUI();
			}
		}

		if (underBar_) {
			if (underBar_->IsHoveringUI().isHit) {
				return underBar_->IsHoveringUI();
			}
		}

		if(residentPanel_){
			if (residentPanel_->IsHoveringUI().isHit) {
				return residentPanel_->IsHoveringUI();
			}
		}

		if (buildPanel_) {
			if (buildPanel_->IsHoveringUI().isHit) {
				return buildPanel_->IsHoveringUI();
			}
		}

		if (buildDetailPanel_) {
			if (buildDetailPanel_->IsHoveringUI().isHit) {
				return buildDetailPanel_->IsHoveringUI();
			}
		}

		if (uiGamePlayFieldDetailPanel_) {
			if (uiGamePlayFieldDetailPanel_->IsHoveringUI().isHit) {
				return uiGamePlayFieldDetailPanel_->IsHoveringUI();
			}
		}



		return {};
	}

}