#pragma once

#include <ParticlePlugin/Basics.h>
#include <Foundation/Configuration/Singleton.h>
#include <Core/ResourceManager/ResourceHandle.h>

class ezWorld;
class ezParticleSystemInstance;
class ezParticleEffectInstance;

class EZ_PARTICLEPLUGIN_DLL ezParticleEffectManager
{
  EZ_DECLARE_SINGLETON(ezParticleEffectManager);
public:
  ezParticleEffectManager();

  ezParticleSystemInstance* CreateParticleSystemInstance(ezUInt32 uiMaxParticles, ezWorld* pWorld, ezUInt64 uiRandomSeed, ezParticleEffectInstance* pOwnerEffect);
  void DestroyParticleSystemInstance(ezParticleSystemInstance* pInstance);

  void Shutdown();

private:
  ezDeque<ezParticleSystemInstance*> m_ParticleSystemFreeList;
};