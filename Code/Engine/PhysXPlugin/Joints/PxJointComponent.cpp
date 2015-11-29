#include <PhysXPlugin/PCH.h>
#include <PhysXPlugin/Joints/PxJointComponent.h>
#include <PhysXPlugin/PhysXSceneModule.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <PhysXPlugin/Components/PxDynamicActorComponent.h>

EZ_BEGIN_COMPONENT_TYPE(ezPxJointComponent, 1);
  EZ_BEGIN_PROPERTIES
    EZ_MEMBER_PROPERTY("Break Force", m_fBreakForce),
    EZ_MEMBER_PROPERTY("Break Torque", m_fBreakTorque),
    EZ_MEMBER_PROPERTY("Pair Collision", m_bPairCollision),
  EZ_END_PROPERTIES
EZ_END_DYNAMIC_REFLECTED_TYPE();

ezPxJointComponent::ezPxJointComponent()
{
  m_fBreakForce = 0.0f;
  m_fBreakTorque = 0.0f;
  m_bPairCollision = false;
  m_pJoint = nullptr;
}


void ezPxJointComponent::SerializeComponent(ezWorldWriter& stream) const
{
  SUPER::SerializeComponent(stream);

  auto& s = stream.GetStream();

  s << m_fBreakForce;
  s << m_fBreakTorque;
  s << m_bPairCollision;
}


void ezPxJointComponent::DeserializeComponent(ezWorldReader& stream, ezUInt32 uiTypeVersion)
{
  SUPER::DeserializeComponent(stream, uiTypeVersion);

  auto& s = stream.GetStream();

  s >> m_fBreakForce;
  s >> m_fBreakTorque;
  s >> m_bPairCollision;
}


void ezPxJointComponent::Deinitialize()
{
  if (m_pJoint)
  {
    m_pJoint->release();
    m_pJoint = nullptr;
  }
}

PxJoint* ezPxJointComponent::SetupJoint()
{
  auto pOwner = GetOwner();
  auto pParent = pOwner->GetParent();
  ezGameObject* pChild = nullptr;

  ezPxDynamicActorComponent* pParentRbComp = nullptr;
  ezPxDynamicActorComponent* pChildRbComp = nullptr;

  if (pParent)
    pParent->TryGetComponentOfBaseType<ezPxDynamicActorComponent>(pParentRbComp);

  if (pOwner->TryGetComponentOfBaseType<ezPxDynamicActorComponent>(pChildRbComp))
  {
    pChild = pOwner;
  }
  else
  {
    for (auto itChild = pOwner->GetChildren(); itChild.IsValid(); itChild.Next())
    {
      if (itChild->TryGetComponentOfBaseType<ezPxDynamicActorComponent>(pChildRbComp))
      {
        pChild = itChild;
        break;
      }
    }
  }

  if (pChildRbComp == nullptr)
  {
    ezLog::Error("%s '%s' does not have a direct child with a ezPxDynamicActorComponent component. Joint is ignored.", GetDynamicRTTI()->GetTypeName(), pOwner->GetName());
    return nullptr;
  }

  if (pChildRbComp->GetKinematic())
  {
    ezLog::Error("%s '%s' has a child with a ezPxDynamicActorComponent which is set to be kinematic. Joint is ignored.", GetDynamicRTTI()->GetTypeName(), pOwner->GetName());
    return nullptr;
  }

  ezPhysXSceneModule* pModule = static_cast<ezPhysXSceneModule*>(GetManager()->GetUserData());

  PxRigidActor* pParentActor = pParentRbComp ? pParentRbComp->GetActor() : nullptr;
  PxRigidActor* pChildActor = pChildRbComp->GetActor();

  if (pChildActor == nullptr)
  {
    ezLog::Error("%s '%s' has an invalid child. Joint is ignored.", GetDynamicRTTI()->GetTypeName(), pOwner->GetName());
    return nullptr;
  }

  ezTransform tLocalToParent;
  ezTransform tLocalToChild;
  const ezTransform tOwn = pOwner->GetGlobalTransform();

  if (pParentRbComp)
  {
    ezTransform tParent = pParent->GetGlobalTransform();
    tParent.Invert();

    tLocalToParent = tParent * tOwn;
  }
  else
  {
    tLocalToParent = tOwn;
  }

  {
    ezTransform tChild = pChild->GetGlobalTransform();
    tChild.Invert();
    tLocalToChild = tChild * tOwn;
  }

  PxTransform tPxLocalToParent = PxTransform::createIdentity();
  {
    const ezVec3 pos = tLocalToParent.m_vPosition;
    ezQuat rot; rot.SetFromMat3(tLocalToParent.m_Rotation);
    tPxLocalToParent.p = PxVec3(pos.x, pos.y, pos.z);
    tPxLocalToParent.q = PxQuat(rot.v.x, rot.v.y, rot.v.z, rot.w);
  }

  PxTransform tPxLocalToChild = PxTransform::createIdentity();
  {
    const ezVec3 pos = tLocalToChild.m_vPosition;
    ezQuat rot; rot.SetFromMat3(tLocalToChild.m_Rotation);
    tPxLocalToChild.p = PxVec3(pos.x, pos.y, pos.z);
    tPxLocalToChild.q = PxQuat(rot.v.x, rot.v.y, rot.v.z, rot.w);
  }

  m_pJoint = CreateJointType(pModule->GetPxScene()->getPhysics(), pParentActor, tPxLocalToParent, pChildActor, tPxLocalToChild);
  EZ_ASSERT_DEV(m_pJoint != nullptr, "Joint creation failed");

  m_pJoint->setBreakForce(m_fBreakForce <= 0.0f ? ezMath::BasicType<float>::MaxValue() : m_fBreakForce, m_fBreakTorque <= 0.0f ? ezMath::BasicType<float>::MaxValue() : m_fBreakTorque);
  m_pJoint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, m_bPairCollision);

  return m_pJoint;
}
