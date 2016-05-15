#pragma once

#include <ToolsFoundation/Document/Document.h>
#include <ToolsFoundation/Object/DocumentObjectManager.h>
#include <EditorFramework/DocumentWindow3D/DocumentWindow3D.moc.h>
#include <CoreUtils/DataStructures/ObjectMetaData.h>
#include <EditorFramework/Assets/AssetDocument.h>
#include <ToolsFoundation/Project/ToolsProject.h>

class ezAssetFileHeader;

enum class ActiveGizmo
{
  None,
  Translate,
  Rotate,
  Scale,
  DragToPosition,
};

enum GameMode
{
  Off,
  Simulate,
  Play,
};

struct ezSceneDocumentEvent
{
  enum class Type
  {
    ActiveGizmoChanged,
    ShowSelectionInScenegraph,
    FocusOnSelection_Hovered,
    FocusOnSelection_All,
    SnapSelectionPivotToGrid,
    SnapEachSelectedObjectToGrid,
    GameModeChanged,
    RenderSelectionOverlayChanged,
    RenderVisualizersChanged,
    RenderShapeIconsChanged,
    SimulationSpeedChanged,
    TriggerGameModePlay,
    TriggerStopGameModePlay,
  };

  Type m_Type;
};

struct ezSceneDocumentExportEvent
{
  const char* m_szTargetFile;
  const ezAssetFileHeader* m_pAssetFileHeader;
  ezStatus m_ReturnStatus;
};

class ezSceneObjectMetaData : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezSceneObjectMetaData, ezReflectedClass);

public:

  enum ModifiedFlags
  {
    CachedName = EZ_BIT(2),

    AllFlags = 0xFFFFFFFF
  };

  ezSceneObjectMetaData()
  {
  }

  ezString m_CachedNodeName;
  QIcon m_Icon;
};

class ezSceneDocument : public ezAssetDocument
{
  EZ_ADD_DYNAMIC_REFLECTION(ezSceneDocument, ezAssetDocument);

public:
  ezSceneDocument(const char* szDocumentPath, bool bIsPrefab);
  ~ezSceneDocument();

  virtual const char* GetDocumentTypeDisplayString() const override { return "Scene"; }

  void SetActiveGizmo(ActiveGizmo gizmo);
  ActiveGizmo GetActiveGizmo() const;

  enum class ShowOrHide
  {
    Show,
    Hide
  };

  void TriggerShowSelectionInScenegraph();
  void TriggerFocusOnSelection(bool bAllViews);
  void TriggerSnapPivotToGrid();
  void TriggerSnapEachObjectToGrid();
  void GroupSelection();
  
  /// \brief Opens the Duplicate Special dialog
  void DuplicateSpecial();

  /// \brief Creates a new empty node, either top-level (selection empty) or as a child of the selected item
  void CreateEmptyNode();

  void DuplicateSelection();
  void ShowOrHideSelectedObjects(ShowOrHide action);
  void ShowOrHideAllObjects(ShowOrHide action);
  void HideUnselectedObjects();
  void UpdatePrefabs();
  void RevertPrefabs(const ezDeque<const ezDocumentObject*>& Selection);
  void UnlinkPrefabs(const ezDeque<const ezDocumentObject*>& Selection);

  bool IsPrefab() const { return m_bIsPrefab; }

  void SetGizmoWorldSpace(bool bWorldSpace);
  bool GetGizmoWorldSpace() const;

  virtual bool Copy(ezAbstractObjectGraph& out_objectGraph) override;
  virtual bool Paste(const ezArrayPtr<PasteInfo>& info, const ezAbstractObjectGraph& objectGraph, bool bAllowPickedPosition) override;
  bool Duplicate(const ezArrayPtr<PasteInfo>& info, const ezAbstractObjectGraph& objectGraph, bool bSetSelected);
  bool Copy(ezAbstractObjectGraph& graph, ezMap<ezUuid, ezUuid>* out_pParents);
  bool PasteAt(const ezArrayPtr<PasteInfo>& info, const ezVec3& vPos);
  bool PasteAtOrignalPosition(const ezArrayPtr<PasteInfo>& info);

  const ezTransform& GetGlobalTransform(const ezDocumentObject* pObject);
  void SetGlobalTransform(const ezDocumentObject* pObject, const ezTransform& t);

  void SetPickingResult(const ezObjectPickingResult& res) { m_PickingResult = res; }

  static ezTransform QueryLocalTransform(const ezDocumentObject* pObject);
  ezTransform ComputeGlobalTransform(const ezDocumentObject* pObject) const;

  const ezString& GetCachedPrefabGraph(const ezUuid& AssetGuid);
  ezString ReadDocumentAsString(const char* szFile) const;

  ezEvent<const ezSceneDocumentEvent&> m_SceneEvents;
  ezEvent <ezSceneDocumentExportEvent&> m_ExportEvent;
  ezObjectMetaData<ezUuid, ezSceneObjectMetaData> m_SceneObjectMetaData;

  ezStatus CreatePrefabDocumentFromSelection(const char* szFile);
  ezStatus CreatePrefabDocument(const char* szFile, const ezDocumentObject* pRootObject, const ezUuid& invPrefabSeed, ezUuid& out_NewDocumentGuid);
  void ReplaceByPrefab(const ezDocumentObject* pRootObject, const char* szPrefabFile, const ezUuid& PrefabAsset, const ezUuid& PrefabSeed);

  GameMode GetGameMode() const { return m_GameMode; }

  void StartSimulateWorld();
  void TriggerGameModePlay();
  void StopGameMode();

  float GetSimulationSpeed() const { return m_fSimulationSpeed; }
  void SetSimulationSpeed(float f);

  bool GetRenderSelectionOverlay() const { return m_CurrentMode.m_bRenderSelectionOverlay; }
  void SetRenderSelectionOverlay( bool b );

  bool GetRenderVisualizers() const { return m_CurrentMode.m_bRenderVisualizers; }
  void SetRenderVisualizers(bool b);

  bool GetRenderShapeIcons() const { return m_CurrentMode.m_bRenderShapeIcons; }
  void SetRenderShapeIcons(bool b);

  void HandleGameModeMsg(const ezGameModeMsgToEditor* pMsg);

  ezStatus ExportScene();

  /// \brief Traverses the pObject hierarchy up until it hits an ezGameObject, then computes the global transform of that.
  virtual ezResult ComputeObjectTransformation(const ezDocumentObject* pObject, ezTransform& out_Result) const override;

protected:
  void SetGameMode(GameMode mode);

  virtual void InitializeAfterLoading() override;

  template<typename Func>
  void ApplyRecursive(const ezDocumentObject* pObject, Func f)
  {
    f(pObject);

    for (auto pChild : pObject->GetChildren())
    {
      ApplyRecursive<Func>(pChild, f);
    }
  }

  virtual void AttachMetaDataBeforeSaving(ezAbstractObjectGraph& graph) override;
  virtual void RestoreMetaDataAfterLoading(const ezAbstractObjectGraph& graph) override;

private:
  void ObjectPropertyEventHandler(const ezDocumentObjectPropertyEvent& e);
  void ObjectStructureEventHandler(const ezDocumentObjectStructureEvent& e);
  void ObjectEventHandler(const ezDocumentObjectEvent& e);
  void EngineConnectionEventHandler(const ezEditorEngineProcessConnection::Event& e);
  void ToolsProjectEventHandler(const ezToolsProject::Event& e);

  void InvalidateGlobalTransformValue(const ezDocumentObject* pObject);

  void UpdatePrefabsRecursive(ezDocumentObject* pObject);
  void UpdatePrefabObject(ezDocumentObject* pObject, const ezUuid& PrefabAsset, const ezUuid& PrefabSeed, const char* szBasePrefab);

  virtual const char* QueryAssetType() const override;

  virtual void UpdateAssetDocumentInfo(ezAssetDocumentInfo* pInfo) override;

  virtual ezStatus InternalTransformAsset(const char* szTargetFile, const char* szPlatform, const ezAssetFileHeader& AssetHeader) override;
  virtual ezStatus InternalTransformAsset(ezStreamWriter& stream, const char* szPlatform) override;

  virtual ezStatus InternalRetrieveAssetInfo(const char* szPlatform) override;

  virtual ezUInt16 GetAssetTypeVersion() const override;

  virtual ezBitflags<ezAssetDocumentFlags> GetAssetFlags() const override;

  struct GameModeData
  {
    bool m_bRenderSelectionOverlay;
    bool m_bRenderVisualizers;
    bool m_bRenderShapeIcons;
  };

  bool m_bIsPrefab;
  bool m_bGizmoWorldSpace; // whether the gizmo is in local/global space mode
  GameMode m_GameMode;
  float m_fSimulationSpeed;

  GameModeData m_CurrentMode;
  GameModeData m_GameModeData[3];

  ActiveGizmo m_ActiveGizmo;
  ezObjectPickingResult m_PickingResult;

  mutable ezHashTable<const ezDocumentObject*, ezTransform> m_GlobalTransforms;

  ezMap<ezUuid, ezString> m_CachedPrefabGraphs;

};
