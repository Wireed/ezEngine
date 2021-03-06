#include <RtsGamePluginPCH.h>

#include <RendererCore/Messages/SetColorMessage.h>
#include <RmlUiPlugin/Components/RmlUiCanvas2DComponent.h>
#include <RmlUiPlugin/RmlUiContext.h>
#include <RtsGamePlugin/GameMode/EditLevelMode/EditLevelMode.h>
#include <RtsGamePlugin/GameState/RtsGameState.h>

#include <RmlUi/Controls/ElementFormControlSelect.h>

const char* g_BuildItemTypes[] = {
  "FederationShip1",
  "FederationShip2",
  "FederationShip3",
  "KlingonShip1",
  "KlingonShip2",
  "KlingonShip3",
};

RtsEditLevelMode::RtsEditLevelMode() = default;
RtsEditLevelMode::~RtsEditLevelMode() = default;

void RtsEditLevelMode::OnActivateMode()
{
  SetupEditUI();
}

void RtsEditLevelMode::OnDeactivateMode()
{
  EZ_LOCK(m_pMainWorld->GetWriteMarker());

  ezRmlUiCanvas2DComponent* pUiComponent = nullptr;
  if (m_pMainWorld->TryGetComponent(m_hEditUIComponent, pUiComponent))
  {
    pUiComponent->SetActiveFlag(false);
  }
}

void RtsEditLevelMode::OnBeforeWorldUpdate()
{
  DisplaySelectModeUI();
  DisplayEditUI();

  m_pGameState->RenderUnitSelection();
}

void RtsEditLevelMode::SetupEditUI()
{
  ezGameObject* pEditUIObject = nullptr;
  if (!m_pMainWorld->TryGetObjectWithGlobalKey(ezTempHashedString("EditUI"), pEditUIObject))
    return;

  ezRmlUiCanvas2DComponent* pUiComponent = nullptr;
  if (!pEditUIObject->TryGetComponentOfBaseType(pUiComponent))
    return;

  pUiComponent->EnsureInitialized();

  auto pDocument = pUiComponent->GetRmlContext()->GetDocument(0);

  if (auto pElement = pDocument->GetElementById("build"))
  {
    // should be rmlui_dynamic_cast but ElementFormControlSelect has no rtti
    if (auto pSelectElement = static_cast<Rml::Controls::ElementFormControlSelect*>(pElement))
    {
      if (pSelectElement->GetNumOptions() == 0)
      {
        ezStringBuilder sValue;

        for (ezUInt32 i = 0; i < EZ_ARRAY_SIZE(g_BuildItemTypes); ++i)
        {
          sValue.Format("{}", i);
          pSelectElement->Add(g_BuildItemTypes[i], sValue.GetData());
        }
      }
    }
  }

  if (auto pElement = pDocument->GetElementById("selectkey"))
  {
    ezStringBuilder s;
    s.Format("Select: {}", ezInputManager::GetInputSlotDisplayName(ezInputSlot_MouseButton0));
    pElement->SetInnerRML(s.GetData());
  }

  if (auto pElement = pDocument->GetElementById("createkey"))
  {
    ezStringBuilder s;
    s.Format("Create: {}", ezInputManager::GetInputSlotDisplayName("EditLevelMode", "PlaceObject"));
    pElement->SetInnerRML(s.GetData());
  }

  if (auto pElement = pDocument->GetElementById("removekey"))
  {
    ezStringBuilder s;
    s.Format("Remove: {}", ezInputManager::GetInputSlotDisplayName("EditLevelMode", "RemoveObject"));
    pElement->SetInnerRML(s.GetData());
  }

  pUiComponent->GetRmlContext()->RegisterEventHandler("teamChanged", [this](Rml::Core::Event& e) {
    m_uiTeam = static_cast<Rml::Controls::ElementFormControlSelect*>(e.GetTargetElement())->GetSelection();
  });
  pUiComponent->GetRmlContext()->RegisterEventHandler("buildChanged", [this](Rml::Core::Event& e) {
    m_iShipType = static_cast<Rml::Controls::ElementFormControlSelect*>(e.GetTargetElement())->GetSelection();
  });

  m_hEditUIComponent = pUiComponent->GetHandle();
}

void RtsEditLevelMode::DisplayEditUI()
{
  ezRmlUiCanvas2DComponent* pUiComponent = nullptr;
  if (m_pMainWorld->TryGetComponent(m_hEditUIComponent, pUiComponent))
  {
    pUiComponent->SetActiveFlag(s_bUseRmlUi);
  }

  if (s_bUseRmlUi && pUiComponent != nullptr)
  {
    auto pDocument = pUiComponent->GetRmlContext()->GetDocument(0);

    if (auto pElement = pDocument->GetElementById("team"))
    {
      static_cast<Rml::Controls::ElementFormControlSelect*>(pElement)->SetSelection(m_uiTeam);
    }

    if (auto pElement = pDocument->GetElementById("build"))
    {
      static_cast<Rml::Controls::ElementFormControlSelect*>(pElement)->SetSelection(m_iShipType);
    }
  }
  else
  {
    ezImgui::GetSingleton()->SetCurrentContextForView(m_hMainView);

    const ezSizeU32 resolution = ezImgui::GetSingleton()->GetCurrentWindowResolution();

    const float ww = 200;

    ImGui::SetNextWindowPos(ImVec2((float)resolution.width - ww - 10, 10));
    ImGui::SetNextWindowSize(ImVec2(ww, 150));
    ImGui::Begin("Edit Level", nullptr,
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    int iTeam = m_uiTeam;
    if (ImGui::Combo("Team", &iTeam, "Red\0Green\0Blue\0Yellow\0\0", 4))
    {
      m_uiTeam = iTeam;
    }

    if (ImGui::Combo("Build", &m_iShipType, g_BuildItemTypes, EZ_ARRAY_SIZE(g_BuildItemTypes)))
    {
    }

    ImGui::Text("Select: %s", ezInputManager::GetInputSlotDisplayName(ezInputSlot_MouseButton0));
    ImGui::Text("Create: %s", ezInputManager::GetInputSlotDisplayName("EditLevelMode", "PlaceObject"));
    ImGui::Text("Remove: %s", ezInputManager::GetInputSlotDisplayName("EditLevelMode", "RemoveObject"));

    ImGui::End();
  }
}

void RtsEditLevelMode::RegisterInputActions()
{
  ezInputActionConfig cfg;

  // Level Editing
  {
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeySpace;
    ezInputManager::SetInputActionConfig("EditLevelMode", "PlaceObject", cfg, true);

    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyDelete;
    ezInputManager::SetInputActionConfig("EditLevelMode", "RemoveObject", cfg, true);
  }
}

void RtsEditLevelMode::OnProcessInput(const RtsMouseInputState& MouseInput)
{
  DoDefaultCameraInput(MouseInput);

  ezVec3 vPickedGroundPlanePos;
  if (m_pGameState->PickGroundPlanePosition(vPickedGroundPlanePos).Failed())
    return;

  if (ezInputManager::GetInputActionState("EditLevelMode", "PlaceObject") == ezKeyState::Pressed)
  {
    ezGameObject* pSpawned = nullptr;

    pSpawned = m_pGameState->SpawnNamedObjectAt(ezTransform(vPickedGroundPlanePos, ezQuat::IdentityQuaternion()),
      g_BuildItemTypes[m_iShipType], m_uiTeam);

    ezMsgSetColor msg;
    msg.m_Color = RtsGameMode::GetTeamColor(m_uiTeam);

    pSpawned->PostMessageRecursive(msg, ezTime::Zero(), ezObjectMsgQueueType::AfterInitialized);

    return;
  }

  auto& unitSelection = m_pGameState->m_SelectedUnits;

  if (ezInputManager::GetInputActionState("EditLevelMode", "RemoveObject") == ezKeyState::Pressed)
  {
    for (ezUInt32 i = 0; i < unitSelection.GetCount(); ++i)
    {
      ezGameObjectHandle hObject = unitSelection.GetObject(i);
      m_pMainWorld->DeleteObjectDelayed(hObject);
    }

    return;
  }

  m_pGameState->DetectHoveredSelectable();

  if (MouseInput.m_LeftClickState == ezKeyState::Released)
  {
    m_pGameState->SelectUnits();
  }
}
