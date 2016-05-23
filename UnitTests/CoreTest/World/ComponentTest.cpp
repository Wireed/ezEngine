#include <PCH.h>
#include <Foundation/Time/Clock.h>
#include <Core/World/World.h>

namespace
{
  class TestComponent;
  class TestComponentManager : public ezComponentManager<TestComponent>
  {
  public:
    TestComponentManager(ezWorld* pWorld) : ezComponentManager<TestComponent>(pWorld)
    {
    }

    virtual void Initialize() override
    {
      auto desc = EZ_CREATE_COMPONENT_UPDATE_FUNCTION_DESC(TestComponentManager::Update, this);
      auto desc2 = EZ_CREATE_COMPONENT_UPDATE_FUNCTION_DESC(TestComponentManager::Update2, this);
      desc.m_DependsOn.PushBack(desc2.m_Function); // update2 will be called before update

      auto descAsync = EZ_CREATE_COMPONENT_UPDATE_FUNCTION_DESC(TestComponentManager::UpdateAsync, this);
      descAsync.m_Phase = ezComponentManagerBase::UpdateFunctionDesc::Phase::Async;
      descAsync.m_uiGranularity = 20;

      this->RegisterUpdateFunction(desc);
      this->RegisterUpdateFunction(desc2);
      this->RegisterUpdateFunction(descAsync);
    }

    void Update(ezUInt32 uiStartIndex, ezUInt32 uiCount);
    void Update2(ezUInt32 uiStartIndex, ezUInt32 uiCount);
    void UpdateAsync(ezUInt32 uiStartIndex, ezUInt32 uiCount);
  };

  class TestComponent : public ezComponent
  {
    EZ_DECLARE_COMPONENT_TYPE(TestComponent, ezComponent, TestComponentManager);

  public:
    TestComponent() : m_iSomeData(1) {}
    ~TestComponent() {}

    virtual void SerializeComponent(ezWorldWriter& stream) const override {}
    virtual void DeserializeComponent(ezWorldReader& stream) override {}

    virtual void Initialize() override
    {
      ++s_iInitCounter;
    }

    virtual void Deinitialize() override
    {
      --s_iInitCounter;
    }

    virtual void OnAfterAttachedToObject() override
    {
      ++s_iAttachCounter;
    }

    virtual void OnBeforeDetachedFromObject() override
    {
      if (s_bGOInactiveCheck)
      {
        EZ_TEST_BOOL(!GetOwner()->IsActive());
      }

      --s_iAttachCounter;
    }

    void Update()
    {
      m_iSomeData *= 5;
    }

    void Update2()
    {
      m_iSomeData += 3;
    }

    ezInt32 m_iSomeData;

    static ezInt32 s_iInitCounter;
    static ezInt32 s_iAttachCounter;
    static bool s_bGOInactiveCheck;
  };

  ezInt32 TestComponent::s_iInitCounter = 0;
  ezInt32 TestComponent::s_iAttachCounter = 0;
  bool TestComponent::s_bGOInactiveCheck = false;

  EZ_BEGIN_COMPONENT_TYPE(TestComponent, 1)
  EZ_END_COMPONENT_TYPE

  void TestComponentManager::Update(ezUInt32 uiStartIndex, ezUInt32 uiCount)
  {
    for (auto it = this->m_ComponentStorage.GetIterator(uiStartIndex, uiCount); it.IsValid(); ++it)
    {
      if (it->IsActive())
        it->Update();
    }
  }

  void TestComponentManager::Update2(ezUInt32 uiStartIndex, ezUInt32 uiCount)
  {
    for (auto it = this->m_ComponentStorage.GetIterator(uiStartIndex, uiCount); it.IsValid(); ++it)
    {
      if (it->IsActive())
        it->Update2();
    }
  }

  void TestComponentManager::UpdateAsync(ezUInt32 uiStartIndex, ezUInt32 uiCount)
  {
    for (auto it = this->m_ComponentStorage.GetIterator(uiStartIndex, uiCount); it.IsValid(); ++it)
    {
      if (it->IsActive())
        it->Update();
    }
  }
}


EZ_CREATE_SIMPLE_TEST(World, Components)
{
  ezWorld world("TestComp");
  EZ_LOCK(world.GetWriteMarker());

  TestComponentManager* pManager = world.GetOrCreateComponentManager<TestComponentManager>();

  ezGameObjectDesc gd;
  ezGameObject* pDummy;
  ezGameObjectHandle hDummy = world.CreateObject(gd, pDummy);

  TestComponent* pComponent = nullptr;
  ezGameObject* pObject = nullptr;

  TestComponent::s_iAttachCounter = 0;
  TestComponent::s_iInitCounter = 0;
  TestComponent::s_bGOInactiveCheck = false;

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Component Init")
  {
    // test recursive write lock
    EZ_LOCK(world.GetWriteMarker());

    ezComponentHandle handle;
    EZ_TEST_BOOL(!world.TryGetComponent(handle, pComponent));

    // Update with no components created
    world.Update();

    handle = TestComponent::CreateComponent(&world, pComponent);
    pDummy->AttachComponent(pComponent);

    TestComponent* pTest = nullptr;
    EZ_TEST_BOOL(world.TryGetComponent(handle, pTest));
    EZ_TEST_BOOL(pTest == pComponent);
    EZ_TEST_BOOL(pComponent->GetHandle() == handle);

    EZ_TEST_INT(pComponent->m_iSomeData, 1);
    EZ_TEST_INT(TestComponent::s_iInitCounter, 0);

    for (ezUInt32 i = 1; i < 100; ++i)
    {
      pManager->CreateComponent(pComponent);
      pComponent->m_iSomeData = i + 1;

      pDummy->AttachComponent(pComponent);
    }

    EZ_TEST_INT(pManager->GetComponentCount(), 100);
    EZ_TEST_INT(TestComponent::s_iInitCounter, 0);

    // Update with no components created
    world.Update();

    EZ_TEST_INT(pManager->GetComponentCount(), 100);
    EZ_TEST_INT(TestComponent::s_iInitCounter, 100);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Component Update")
  {
    // test recursive read lock
    EZ_LOCK(world.GetReadMarker());

    world.Update();

    ezUInt32 uiCounter = 0;
    for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
    {
      EZ_TEST_INT(it->m_iSomeData, (((uiCounter + 4) * 25) + 3) * 25);
      ++uiCounter;
    }

    EZ_TEST_INT(uiCounter, 100);
  }

  {
    for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
    {
      pDummy->DetachComponent(it);
      EZ_TEST_BOOL(it->GetOwner() == nullptr);
    }
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Attach to objects")
  {
    world.CreateObject(ezGameObjectDesc(), pObject);

    EZ_TEST_INT(TestComponent::s_iAttachCounter, 0);

    for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
    {
      pObject->AttachComponent(it);
      EZ_TEST_BOOL(it->GetOwner() != nullptr);
    }

    EZ_TEST_INT(TestComponent::s_iAttachCounter, 100);
    EZ_TEST_INT(pObject->GetComponents().GetCount(), 100);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Detach from objects")
  {
    ezComponent* pFirstComponent = pObject->GetComponents()[0];
    pObject->DetachComponent(pFirstComponent);

    EZ_TEST_INT(TestComponent::s_iInitCounter, 100);
    EZ_TEST_INT(TestComponent::s_iAttachCounter, 99);
    EZ_TEST_INT(pObject->GetComponents().GetCount(), 99);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Delete Component")
  {
    pManager->DeleteComponent(pComponent->GetHandle());
    EZ_TEST_INT(pManager->GetComponentCount(), 99);
    EZ_TEST_INT(TestComponent::s_iInitCounter, 99);
    EZ_TEST_INT(TestComponent::s_iAttachCounter, 98);

    // component should also be removed from the game object
    EZ_TEST_INT(pObject->GetComponents().GetCount(), 98);

    world.DeleteObjectNow(pObject->GetHandle());
    world.Update();

    EZ_TEST_INT(TestComponent::s_iInitCounter, 1);
    EZ_TEST_INT(TestComponent::s_iAttachCounter, 0);

    world.DeleteComponentManager<TestComponentManager>();
    pManager = nullptr;
    EZ_TEST_INT(TestComponent::s_iInitCounter, 0);
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Delete Objects with Component")
  {
    TestComponent::s_bGOInactiveCheck = true;

    ezGameObjectDesc desc;

    ezGameObject* pObjectA = nullptr;
    ezGameObject* pObjectB = nullptr;
    ezGameObject* pObjectC = nullptr;

    desc.m_sName.Assign("A"); ezGameObjectHandle hObjectA = world.CreateObject(desc, pObjectA);
    desc.m_sName.Assign("B"); ezGameObjectHandle hObjectB = world.CreateObject(desc, pObjectB);
    desc.m_sName.Assign("C"); ezGameObjectHandle hObjectC = world.CreateObject(desc, pObjectC);

    TestComponent* pComponentA = nullptr;
    TestComponent* pComponentB = nullptr;
    TestComponent* pComponentC = nullptr;

    ezComponentHandle hComponentA = TestComponent::CreateComponent(&world, pComponentA);
    ezComponentHandle hComponentB = TestComponent::CreateComponent(&world, pComponentB);
    ezComponentHandle hComponentC = TestComponent::CreateComponent(&world, pComponentC);

    pObjectA->AttachComponent(pComponentA);
    pObjectB->AttachComponent(pComponentB);
    pObjectC->AttachComponent(pComponentC);

    world.DeleteObjectNow(pObjectB->GetHandle());

    EZ_TEST_BOOL(pObjectA->IsActive());
    EZ_TEST_BOOL(pComponentA->IsActive());
    EZ_TEST_BOOL(pComponentA->GetOwner() == pObjectA);

    EZ_TEST_BOOL(!pObjectB->IsActive());
    EZ_TEST_BOOL(!pComponentB->IsActive());
    EZ_TEST_BOOL(pComponentB->GetOwner() == nullptr);

    EZ_TEST_BOOL(pObjectC->IsActive());
    EZ_TEST_BOOL(pComponentC->IsActive());
    EZ_TEST_BOOL(pComponentC->GetOwner() == pObjectC);

    world.Update();

    EZ_TEST_BOOL(world.TryGetObject(hObjectA, pObjectA));
    EZ_TEST_BOOL(world.TryGetObject(hObjectC, pObjectC));

    // Since we're not recompacting storage for components, pointer should still be valid.
    //EZ_TEST_BOOL(world.TryGetComponent(hComponentA, pComponentA));
    //EZ_TEST_BOOL(world.TryGetComponent(hComponentC, pComponentC));

    EZ_TEST_BOOL(pObjectA->IsActive());
    EZ_TEST_STRING(pObjectA->GetName(), "A");
    EZ_TEST_BOOL(pComponentA->IsActive());
    EZ_TEST_BOOL(pComponentA->GetOwner() == pObjectA);

    EZ_TEST_BOOL(pObjectC->IsActive());
    EZ_TEST_STRING(pObjectC->GetName(), "C");
    EZ_TEST_BOOL(pComponentC->IsActive());
    EZ_TEST_BOOL(pComponentC->GetOwner() == pObjectC);

    // creating a new component should reuse memory from component B
    TestComponent* pComponentB2 = nullptr;
    ezComponentHandle hComponentB2 = TestComponent::CreateComponent(&world, pComponentB2);
    EZ_TEST_BOOL(pComponentB2 == pComponentB);

    TestComponent::s_bGOInactiveCheck = false;
  }

  EZ_TEST_BLOCK(ezTestBlock::Enabled, "Get Components")
  {
    const ezWorld& constWorld = world;

    const TestComponentManager* pConstManager = constWorld.GetComponentManager<TestComponentManager>();

    for (auto it = pConstManager->GetComponents(); it.IsValid(); it.Next())
    {
      ezComponentHandle hComponent = it->GetHandle();

      const TestComponent* pConstComponent = nullptr;
      EZ_TEST_BOOL(constWorld.TryGetComponent(hComponent, pConstComponent));
      EZ_TEST_BOOL(pConstComponent == (const TestComponent*)it);

      EZ_TEST_BOOL(pConstManager->TryGetComponent(hComponent, pConstComponent));
      EZ_TEST_BOOL(pConstComponent == (const TestComponent*)it);
    }
  }
}