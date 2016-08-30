#pragma once

#include <ParticlePlugin/Basics.h>
#include <Foundation/Math/Transform.h>
#include <ParticlePlugin/System/ParticleSystemInstance.h>

class EZ_PARTICLEPLUGIN_DLL ezParticleEffectInstance 
{
  friend class ezParticleWorldModule;

public:
  struct SharedInstance
  {
    ezUInt32 m_uiIdentifier;
    ezTransform m_Transform;
  };

public:
  ezParticleEffectInstance();
  ~ezParticleEffectInstance();

  void Clear();

  void Interrupt();

  const ezParticleEffectHandle& GetHandle() const { return m_hHandle; }

  void SetEmitterEnabled(bool enable);
  bool GetEmitterEnabled() const { return m_bEmitterEnabled; }

  bool HasActiveParticles() const;

  void ClearParticleSystems();

  void Configure(const ezParticleEffectResourceHandle& hResource, ezWorld* pWorld, ezParticleWorldModule* pOwnerModule, ezUInt64 uiRandomSeed, bool bIsShared);

  void PreSimulate();


  /// \brief Returns false when the effect is finished
  bool Update(const ezTime& tDiff);

  ezWorld* GetWorld() const { return m_pWorld; }

  const ezParticleEffectResourceHandle& GetResource() const { return m_hResource; }

  const ezHybridArray<ezParticleSystemInstance*, 4>& GetParticleSystems() const { return m_ParticleSystems; }

  bool IsSharedEffect() const { return m_bIsShared; }
  bool IsSimulatedInLocalSpace() const { return m_bSimulateInLocalSpace; }

  void AddSharedInstance(ezUInt32 uiSharedInstanceIdentifier);
  void RemoveSharedInstance(ezUInt32 uiSharedInstanceIdentifier);
  void SetTransform(ezUInt32 uiSharedInstanceIdentifier, const ezTransform& transform);
  const ezTransform& GetTransform(ezUInt32 uiSharedInstanceIdentifier) const;

  const ezDynamicArray<SharedInstance>& GetAllSharedInstances() const { return m_SharedInstances; }

  ezParticleEventQueue* GetEventQueue(const ezTempHashedString& EventType);

private:
  void Reconfigure(ezUInt64 uiRandomSeed, bool bFirstTime);
  void ClearParticleSystem(ezUInt32 index);
  void DestroyEventQueues();
  void ProcessEventQueues();

  ezDynamicArray<SharedInstance> m_SharedInstances;
  ezParticleEffectHandle m_hHandle;
  bool m_bIsShared;
  bool m_bEmitterEnabled;
  bool m_bSimulateInLocalSpace;
  ezUInt8 m_uiReviveTimeout;
  ezTime m_PreSimulateDuration;
  ezParticleEffectResourceHandle m_hResource;

  ezParticleWorldModule* m_pOwnerModule;
  ezWorld* m_pWorld;
  ezTransform m_Transform;
  ezHybridArray<ezParticleSystemInstance*, 4> m_ParticleSystems;

  struct EventQueue
  {
    EZ_DECLARE_POD_TYPE();

    ezUInt32 m_EventTypeHash;
    ezParticleEventQueue* m_pQueue;
  };

  ezHybridArray<EventQueue, 4> m_EventQueues;
};