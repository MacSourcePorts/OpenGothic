#include "mdlvisual.h"
#include "objvisual.h"

#include "graphics/pfx/particlefx.h"
#include "graphics/mesh/skeleton.h"
#include "game/serialize.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/fileext.h"
#include "gothic.h"

using namespace Tempest;

MdlVisual::MdlVisual()
  :skInst(std::make_unique<Pose>()) {
  pos.identity();
  }

MdlVisual::~MdlVisual() {
  }

void MdlVisual::save(Serialize &fout, const Npc&) const {
  fout.write(fgtMode);
  fout.write(headRotX,headRotY);
  if(skeleton!=nullptr)
    fout.write(skeleton->name()); else
    fout.write(std::string(""));
  solver.save(fout);
  skInst->save(fout);
  }

void MdlVisual::save(Serialize& fout, const Interactive&) const {
  solver.save(fout);
  skInst->save(fout);
  }

void MdlVisual::load(Serialize& fin, Npc& npc) {
  std::string s;

  fin.read(fgtMode);
  if(fin.version()>30)
    fin.read(headRotX,headRotY);

  fin.read(s);
  if(fin.version()<=26)
    FileExt::exchangeExt(s,"MDH","MDS");
  npc.setVisual(s.c_str());

  solver.load(fin);
  skInst->load(fin,solver);
  }

void MdlVisual::load(Serialize& fin, Interactive&) {
  if(fin.version()>=17)
    solver.load(fin);
  if(fin.version()>=11)
    skInst->load(fin,solver);
  syncAttaches();
  }

// Mdl_SetVisual
void MdlVisual::setVisual(const Skeleton *v) {
  if(skeleton!=nullptr && v!=nullptr)
    rebindAttaches(*skeleton,*v);
  solver.setSkeleton(v);
  skInst->setSkeleton(v);
  view.setSkeleton(v);

  skeleton = v;
  setObjMatrix(pos); // invalidate obj matrix
  }

void MdlVisual::setYTranslationEnable(bool e) {
  if(e)
    skInst->setFlags(Pose::NoFlags); else
    skInst->setFlags(Pose::NoTranslation);
  }

void MdlVisual::setVisualBody(MeshObjects::Mesh&& body, World& owner) {
  implSetBody(std::move(body),owner,0);
  }

// Mdl_SetVisualBody
void MdlVisual::setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body, World& owner, int32_t version) {
  bind(head,std::move(h),"BIP01 HEAD");
  implSetBody(std::move(body),owner,version);
  head.view.setAsGhost(hnpcFlagGhost);
  }

bool MdlVisual::hasOverlay(const Skeleton* sk) const {
  return solver.hasOverlay(sk);
  }

// Mdl_ApplyOverlayMdsTimed, Mdl_ApplyOverlayMds
void MdlVisual::addOverlay(const Skeleton *sk, uint64_t time) {
  solver.addOverlay(sk,time);
  }

// Mdl_RemoveOverlayMDS
void MdlVisual::delOverlay(std::string_view sk) {
  solver.delOverlay(sk);
  }

// Mdl_RemoveOverlayMDS
void MdlVisual::delOverlay(const Skeleton *sk) {
  solver.delOverlay(sk);
  }

void MdlVisual::clearOverlays() {
  solver.clearOverlays();
  }

void MdlVisual::setBody(MeshObjects::Mesh&& a, World& owner, const int32_t version) {
  implSetBody(std::move(a),owner,version);
  setObjMatrix(pos);
  }

void MdlVisual::setArmour(MeshObjects::Mesh &&a, World& owner) {
  // NOTE: Giant_Bug have no version tag in attachments;
  // Light dragon hunter armour has broken attachment with no tags
  // Big dragon hunter armour has many atachments with version tags
  implSetBody(std::move(a),owner,-1);
  setObjMatrix(pos);
  }

void MdlVisual::implSetBody(MeshObjects::Mesh&& body, World& owner, const int32_t version) {
  attach.clear();
  if(auto p = body.protoMesh()) {
    for(auto& att:p->attach) {
      if(!att.hasNode) {
        auto view = owner.addAtachView(att,version);
        setSlotAttachment(std::move(view),att.name.c_str());
        }
      }
    syncAttaches();
    }
  view = std::move(body);
  view.setAsGhost(hnpcFlagGhost);
  view.setSkeleton(skeleton);
  view.setPose(pos,*skInst);
  hnpcVisual.view.setMesh(&view);
  }

void MdlVisual::setSlotAttachment(MeshObjects::Mesh&& itm, std::string_view bone) {
  if(bone.empty() || skeleton==nullptr)
    return;

  size_t id = skeleton->findNode(bone);
  if(id==size_t(-1))
    return;

  for(auto& i:attach) {
    if(i.boneId==id) {
      i.view=std::move(itm);
      syncAttaches();
      return;
      }
    }

  MeshAttach slt;
  slt.bone = skeleton->nodes[id].name.c_str();
  bind(slt,std::move(itm),slt.bone);
  attach.push_back(std::move(slt));
  }

void MdlVisual::setSword(MeshObjects::Mesh &&s) {
  bind(sword,std::move(s),"ZS_RIGHTHAND");
  }

void MdlVisual::setRangeWeapon(MeshObjects::Mesh &&b) {
  bind(bow,std::move(b),"ZS_BOW");
  }

void MdlVisual::setAmmoItem(MeshObjects::Mesh&& a, std::string_view bone) {
  bind(ammunition,std::move(a),bone);
  }

void MdlVisual::setMagicWeapon(Effect&& spell, World& owner) {
  auto n = std::move(pfx.view);
  n.setLooped(false);
  startEffect(owner,std::move(n),0,true);

  pfx.view = std::move(spell);
  pfx.view.setLooped(true);
  if(skeleton!=nullptr)
    pfx.view.bindAttaches(*skInst,*skeleton);
  }

void MdlVisual::setMagicWeaponKey(World& owner, SpellFxKey key, int32_t keyLvl) {
  pfx.view.setKey(owner,key,keyLvl);
  }

void MdlVisual::setSlotItem(MeshObjects::Mesh &&itm, std::string_view bone) {
  if(bone.empty() || skeleton==nullptr)
    return;

  size_t id = skeleton->findNode(bone);
  if(id==size_t(-1))
    return;

  // HACK: light dragon hunter armour
  if(id==head.boneId && !head.view.isEmpty())
    return;

  for(auto& i:item) {
    if(i.boneId==id) {
      i.view=std::move(itm);
      syncAttaches();
      return;
      }
    }

  MeshAttach slt;
  slt.bone = skeleton->nodes[id].name.c_str();
  bind(slt,std::move(itm),slt.bone);
  item.push_back(std::move(slt));
  syncAttaches();
  }

void MdlVisual::setStateItem(MeshObjects::Mesh&& a, std::string_view bone) {
  bind(stateItm,std::move(a),bone);
  syncAttaches();
  }

void MdlVisual::clearSlotItem(std::string_view bone) {
  size_t id = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);

  for(size_t i=0;i<item.size();++i) {
    if(id==size_t(-1) || item[i].boneId==id) {
      item[i] = std::move(item.back());
      item.pop_back();
      syncAttaches();
      return;
      }
    }
  }

bool MdlVisual::setFightMode(const ZenLoad::EFightMode mode) {
  WeaponState f=WeaponState::NoWeapon;

  switch(mode) {
    case ZenLoad::FM_LAST:
      return false;
    case ZenLoad::FM_NONE:
      f=WeaponState::NoWeapon;
      break;
    case ZenLoad::FM_FIST:
      f=WeaponState::Fist;
      break;
    case ZenLoad::FM_1H:
      f=WeaponState::W1H;
      break;
    case ZenLoad::FM_2H:
      f=WeaponState::W2H;
      break;
    case ZenLoad::FM_BOW:
      f=WeaponState::Bow;
      break;
    case ZenLoad::FM_CBOW:
      f=WeaponState::CBow;
      break;
    case ZenLoad::FM_MAG:
      f=WeaponState::Mage;
      break;
    }

  return setToFightMode(f);
  }

void MdlVisual::dropWeapon(Npc& npc) {
  MeshAttach* att  = &sword;
  auto&       pose = *skInst;

  if(fgtMode!=WeaponState::W1H && fgtMode!=WeaponState::W2H &&
     fgtMode!=WeaponState::Bow && fgtMode!=WeaponState::CBow)
    return;

  auto p = pos;
  if(att->boneId<pose.boneCount())
    p.mul(pose.bone(att->boneId));

  Item* itm = nullptr;
  if(fgtMode==WeaponState::W1H || fgtMode==WeaponState::W2H)
    itm = npc.currentMeleWeapon(); else
    itm = npc.currentRangeWeapon();

  if(itm==nullptr)
    return;

  auto it = npc.world().addItemDyn(itm->clsId(),p);
  it->setCount(1);

  npc.delItem(itm->clsId(),1);
  }

void MdlVisual::startEffect(World& owner, Effect&& vfx, int32_t slot, bool noSlot) {
  uint64_t timeUntil = vfx.effectPrefferedTime();
  if(timeUntil!=0)
    timeUntil+=owner.tickCount();

  if(skeleton==nullptr)
    return;

  vfx.setMesh(&view);
  vfx.setBullet(nullptr,owner);

  for(auto& i:effects) {
    if(i.id==slot && !i.noSlot) {
      i.timeUntil = timeUntil;
      i.view = std::move(vfx);
      i.view.bindAttaches(*skInst,*skeleton);
      syncAttaches();
      return;
      }
    }

  PfxSlot slt;
  slt.id        = slot;
  slt.timeUntil = timeUntil;
  slt.noSlot    = noSlot;
  slt.view      = std::move(vfx);
  slt.view.bindAttaches(*skInst,*skeleton);
  effects.push_back(std::move(slt));
  syncAttaches();
  }

void MdlVisual::stopEffect(const VisualFx& vfx) {
  for(size_t i=0;i<effects.size();++i) {
    if(effects[i].view.is(vfx)) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      syncAttaches();
      return;
      }
    }
  }

void MdlVisual::stopEffect(int32_t slot) {
  for(size_t i=0;i<effects.size();++i) {
    if(effects[i].id==slot) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      syncAttaches();
      return;
      }
    }
  }

void MdlVisual::setNpcEffect(World& owner, Npc& npc, const Daedalus::ZString& s, Daedalus::GEngineClasses::C_Npc::ENPCFlag flags) {
  if(hnpcVisualName!=s) {
    hnpcVisualName = s;
    auto vfx = Gothic::inst().loadVisualFx(s.c_str());
    if(vfx==nullptr) {
      hnpcVisual.view = Effect();
      return;
      }
    hnpcVisual.view = Effect(*vfx,owner,npc,SpellFxKey::Count);
    if(skeleton!=nullptr)
      hnpcVisual.view.bindAttaches(*skInst,*skeleton);
    hnpcVisual.view.setActive(true);
    hnpcVisual.view.setLooped(true);
    hnpcVisual.view.setMesh(&view);
    }

  const bool nextGhost = (flags & Daedalus::GEngineClasses::C_Npc::ENPCFlag::EFLAG_GHOST);
  if(hnpcFlagGhost!=nextGhost) {
    hnpcFlagGhost=nextGhost;
    view.setAsGhost(hnpcFlagGhost);
    head.view.setAsGhost(hnpcFlagGhost);
    for(auto& i:attach)
      i.view.setAsGhost(hnpcFlagGhost);
    }
  }

bool MdlVisual::setToFightMode(const WeaponState f) {
  if(f==fgtMode)
    return false;
  fgtMode = f;
  return true;
  }

void MdlVisual::setPosition(float x, float y, float z, bool syncAttach) {
  pos.set(3,0,x);
  pos.set(3,1,y);
  pos.set(3,2,z);
  view.setObjMatrix(pos);
  if(syncAttach)
    syncAttaches();
  }

void MdlVisual::setObjMatrix(const Tempest::Matrix4x4 &m, bool syncAttach) {
  pos = m;
  view.setObjMatrix(pos);
  if(syncAttach)
    syncAttaches();
  }

void MdlVisual::setHeadRotation(float dx, float dz) {
  headRotX = dx;
  headRotY = dz;
  syncHead(head);
  }

Vec2 MdlVisual::headRotation() const {
  return Vec2(headRotX,headRotY);
  }

void MdlVisual::updateWeaponSkeleton(const Item* weapon,const Item* range) {
  auto st = fgtMode;
  if(st==WeaponState::W1H || st==WeaponState::W2H){
    bind(sword,"ZS_RIGHTHAND");
    } else {
    bool twoHands = weapon!=nullptr && weapon->is2H();
    bind(sword,twoHands ? "ZS_LONGSWORD" : "ZS_SWORD");
    }

  if(st==WeaponState::Bow || st==WeaponState::CBow){
    if(st==WeaponState::Bow)
      bind(bow,"ZS_LEFTHAND"); else
      bind(bow,"ZS_RIGHTHAND");
    } else {
    bool cbow  = range!=nullptr && range->isCrossbow();
    bind(bow,cbow ? "ZS_CROSSBOW" : "ZS_BOW");
    }

  pfx.view.setActive(st==WeaponState::Mage);
  syncAttaches();
  }

void MdlVisual::setTorch(bool t, World& owner) {
  if(!t) {
    torch.view.reset();
    return;
    }
  size_t torchId = owner.script().getSymbolIndex("ItLsTorchburning");
  if(torchId==size_t(-1))
    return;

  Daedalus::GEngineClasses::C_Item  hitem={};
  owner.script().initializeInstance(hitem,torchId);
  owner.script().clearReferences(hitem);
  torch.view.reset(new ObjVisual());
  torch.view->setVisual(hitem,owner);
  torch.boneId = (skeleton==nullptr ? size_t(-1) : skeleton->findNode("ZS_LEFTHAND"));
  }

bool MdlVisual::updateAnimation(Npc* npc, World& world) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world.tickCount();
  auto     pos3      = Vec3{pos.at(3,0), pos.at(3,1), pos.at(3,2)};

  if(npc!=nullptr && world.isInSfxRange(pos3))
    pose.processSfx(*npc,tickCount);
  if(world.isInPfxRange(pos3))
    pose.processPfx(*this,world,tickCount);

  for(size_t i=0;i<effects.size();) {
    if(effects[i].timeUntil<tickCount) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      } else {
      ++i;
      }
    }

  solver.update(tickCount);
  const bool changed = pose.update(tickCount);

  if(changed)
    view.setPose(pos,pose);
  return changed;
  }

void MdlVisual::processLayers(World& world) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world.tickCount();
  pose.processLayers(solver,tickCount);
  }

Vec3 MdlVisual::mapBone(const size_t boneId) const {
  Pose&  pose = *skInst;
  if(boneId==size_t(-1))
    return {0,0,0};

  auto mat = pos;
  mat.mul(pose.bone(boneId));

  return {mat.at(3,0) - pos.at(3,0),
          mat.at(3,1) - pos.at(3,1),
          mat.at(3,2) - pos.at(3,2)};
  }

Vec3 MdlVisual::mapWeaponBone() const {
  if(fgtMode==WeaponState::Bow || fgtMode==WeaponState::CBow)
    return mapBone(ammunition.boneId);
  if(fgtMode==WeaponState::Mage && skeleton!=nullptr)
    return mapBone(skeleton->findNode("ZS_RIGHTHAND"));
  return {0,0,0};
  }

void MdlVisual::stopAnim(Npc& npc, std::string_view anim) {
  skInst->stopAnim(anim);
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,0,fgtMode,npc.walkMode(),false);
  }

bool MdlVisual::stopItemStateAnim(Npc& npc) {
  if(!skInst->stopItemStateAnim(solver,npc.world().tickCount()))
    return false;
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,0,fgtMode,npc.walkMode(),false);
  return true;
  }

void MdlVisual::stopWalkAnim(Npc &npc) {
  skInst->stopWalkAnim();
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,0,fgtMode,npc.walkMode(),false);
  }

bool MdlVisual::isStanding() const {
  return skInst->isStanding();
  }

bool MdlVisual::isAnimExist(std::string_view name) const {
  const Animation::Sequence *sq = solver.solveFrm(name);
  return sq!=nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(std::string_view name, uint64_t tickCount, bool forceAnim) {
  auto sq = solver.solveFrm(name);
  if(sq!=nullptr) {
    const Pose::StartHint hint = Pose::StartHint(forceAnim  ? Pose::Force : Pose::NoHint);
    if(skInst->startAnim(solver,sq,0,BS_NONE,hint,tickCount)) {
      return sq;
      }
    }
  return nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc &npc, std::string_view name, uint8_t comb,
                                                      bool forceAnim, BodyState bs) {
  const Animation::Sequence *sq = solver.solveFrm(name);
  const Pose::StartHint hint = Pose::StartHint(forceAnim  ? Pose::Force : Pose::NoHint);
  if(skInst->startAnim(solver,sq,comb,bs,hint,npc.world().tickCount())) {
    return sq;
    }
  return nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc& npc, AnimationSolver::Anim a,
                                                      uint8_t comb,
                                                      WeaponState st, WalkBit wlk,
                                                      bool noInterupt) {
  // for those use MdlVisual::setRotation
  assert(a!=AnimationSolver::Anim::RotL && a!=AnimationSolver::Anim::RotR);

  if(a==AnimationSolver::InteractIn ||
     a==AnimationSolver::InteractOut ||
     a==AnimationSolver::InteractToStand ||
     a==AnimationSolver::InteractFromStand) {
    auto inter = npc.interactive();
    const Animation::Sequence *sq = solver.solveAnim(inter,a,*skInst);
    if(sq!=nullptr){
      if(skInst->startAnim(solver,sq,comb,BS_MOBINTERACT,Pose::NoHint,npc.world().tickCount())) {
        return sq;
        }
      }
    return nullptr;
    }

  if(a==AnimationSolver::Anim::NoAnim) {
    skInst->stopAllAnim();
    return nullptr;
    }

  const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);
  if(sq==nullptr)
    return nullptr;

  bool forceAnim=false;
  if(a==AnimationSolver::Anim::DeadA || a==AnimationSolver::Anim::UnconsciousA ||
     a==AnimationSolver::Anim::DeadB || a==AnimationSolver::Anim::UnconsciousB) {
    skInst->stopAllAnim();
    forceAnim = true;
    }
  if(!noInterupt) {
    if(a==AnimationSolver::Anim::StumbleA || a==AnimationSolver::Anim::StumbleB ||
       a==AnimationSolver::Anim::JumpHang)
      forceAnim = true;
    }

  BodyState bs = BS_NONE;
  switch(a) {
    case AnimationSolver::Anim::NoAnim:
    case AnimationSolver::Anim::Fallen:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::Idle:
    case AnimationSolver::Anim::MagNoMana:
      bs = BS_STAND;
      break;
    case AnimationSolver::Anim::Move:
    case AnimationSolver::Anim::MoveL:
    case AnimationSolver::Anim::MoveR:
    case AnimationSolver::Anim::MoveBack:
      if(bool(wlk & WalkBit::WM_Walk))
        bs = BS_WALK; else
        bs = BS_RUN;
      break;
    case AnimationSolver::Anim::RotL:
    case AnimationSolver::Anim::RotR:
      break;
    case AnimationSolver::Anim::Fall:
    case AnimationSolver::Anim::FallDeep:
      bs = BS_FALL;
      break;
    case AnimationSolver::Anim::Jump:
    case AnimationSolver::Anim::JumpUp:
      bs = BS_JUMP;
      break;
    case AnimationSolver::Anim::JumpUpLow:
    case AnimationSolver::Anim::JumpUpMid:
    case AnimationSolver::Anim::JumpHang:
      bs = BS_CLIMB;
      break;
    case AnimationSolver::Anim::SlideA:
    case AnimationSolver::Anim::SlideB:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::DeadA:
    case AnimationSolver::Anim::DeadB:
      bs = BS_DEAD;
      break;
    case AnimationSolver::Anim::UnconsciousA:
    case AnimationSolver::Anim::UnconsciousB:
      bs = BS_UNCONSCIOUS;
      break;
    case AnimationSolver::Anim::InteractIn:
    case AnimationSolver::Anim::InteractOut:
    case AnimationSolver::Anim::InteractToStand:
    case AnimationSolver::Anim::InteractFromStand:
      bs = BS_MOBINTERACT;
      break;
    case AnimationSolver::Anim::Atack:
    case AnimationSolver::Anim::AtackL:
    case AnimationSolver::Anim::AtackR:
    case AnimationSolver::Anim::AtackFinish:
      bs = BS_HIT;
      break;
    case AnimationSolver::Anim::AtackBlock:
      bs = BS_PARADE;
      break;
    case AnimationSolver::Anim::StumbleA:
    case AnimationSolver::Anim::StumbleB:
      bs = BS_STUMBLE;
      break;
    case AnimationSolver::Anim::AimBow:
      bs = BS_AIMNEAR; //TODO: BS_AIMFAR
      break;
    case AnimationSolver::Anim::ItmGet:
      bs = BS_TAKEITEM;
      break;
    case AnimationSolver::Anim::ItmDrop:
      bs = BS_DROPITEM;
      break;
    case AnimationSolver::Anim::PointAt:
      bs = BS_STAND;
      break;
    }

  if(bool(wlk & WalkBit::WM_Dive))
    bs = BS_DIVE;
  else if(bool(wlk & WalkBit::WM_Swim))
    bs = BS_SWIM;
  else if(bool(wlk & WalkBit::WM_Sneak))
    bs = BS_SNEAK;

  Pose::StartHint hint = Pose::StartHint((forceAnim  ? Pose::Force : Pose::NoHint) |
                                         (noInterupt ? Pose::NoInterupt : Pose::NoHint));

  if(skInst->startAnim(solver,sq,comb,bs,hint,npc.world().tickCount()))
    return sq;
  return nullptr;
  }

bool MdlVisual::startAnim(Npc &npc, WeaponState st) {
  const bool run = (skInst->bodyState()&BS_MAX)==BS_RUN;

  if(st==fgtMode)
    return true;
  const Animation::Sequence *sq = solver.solveAnim(st,fgtMode,run);
  if(sq==nullptr)
    return false;
  if(skInst->startAnim(solver,sq,0,run ? BS_RUN : BS_NONE,Pose::NoHint,npc.world().tickCount()))
    return true;
  return false;
  }

void MdlVisual::setRotation(Npc &npc, int dir) {
  skInst->setRotation(solver,npc,fgtMode,dir);
  }

void MdlVisual::interrupt() {
  skInst->interrupt();
  }

Tempest::Vec3 MdlVisual::displayPosition() const {
  if(skeleton!=nullptr)
    return {0,skeleton->colisionHeight()*1.5f,0};
  return {0.f,0.f,0.f};
  }

float MdlVisual::viewDirection() const {
  auto p = pos;
  if(nullptr!=skeleton) {
    size_t nodeId = skeleton->findNode("BIP01");
    if(nodeId!=size_t(-1))
      p.mul(pose().bone(nodeId));
    }
  float rx = p.at(2,0);
  float rz = p.at(2,2);
  return float(std::atan2(rz,rx)) * 180.f / float(M_PI);
  }

const Animation::Sequence* MdlVisual::continueCombo(Npc& npc, AnimationSolver::Anim a,
                                                    WeaponState st, WalkBit wlk)  {
  if(st==WeaponState::Fist || st==WeaponState::W1H || st==WeaponState::W2H) {
    const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);
    if(auto ret = skInst->continueCombo(solver,sq,npc.world().tickCount()))
      return ret;
    }
  return startAnimAndGet(npc,a,0,st,wlk,false);
  }

uint32_t MdlVisual::comboLength() const {
  return skInst->comboLength();
  }

Bounds MdlVisual::bounds() const {
  auto b = view.bounds();
  if(!head.view.isEmpty())
    b.assign(b,head.view.bounds());
  return b;
  }

void MdlVisual::bind(MeshAttach& slot, MeshObjects::Mesh&& itm, std::string_view bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.view   = std::move(itm);
  slot.bone   = bone;
  // sync?
  }

void MdlVisual::bind(PfxAttach& slot, Effect&& itm, std::string_view bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.view   = std::move(itm);
  slot.bone   = bone;
  // sync?
  }

template<class View>
void MdlVisual::bind(Attach<View>& slot, std::string_view bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.bone   = bone;
  // sync?
  }

void MdlVisual::rebindAttaches(const Skeleton& from, const Skeleton& to) {
  auto  mesh = {&head,&sword,&bow,&ammunition,&stateItm};
  for(auto i:mesh)
    rebindAttaches(*i,from,to);
  for(auto& i:item)
    rebindAttaches(i,from,to);
  for(auto& i:attach)
    rebindAttaches(i,from,to);
  for(auto& i:effects)
    i.view.bindAttaches(*skInst,to);
  pfx.view.bindAttaches(*skInst,to);
  hnpcVisual.view.bindAttaches(*skInst,to);
  }

template<class View>
void MdlVisual::rebindAttaches(Attach<View>& mesh, const Skeleton& from, const Skeleton& to) {
  if(mesh.boneId<from.nodes.size()) {
    size_t nid = 0;
    if(mesh.bone.empty())
      nid = to.findNode(from.nodes[mesh.boneId].name); else
      nid = to.findNode(mesh.bone);
    mesh.boneId = nid;
    }
  }

void MdlVisual::syncAttaches() {
  MdlVisual::MeshAttach* mesh[] = {&sword,&bow,&ammunition,&stateItm};
  syncHead(head);
  for(auto i:mesh)
    syncAttaches(*i);
  for(auto& i:item)
    syncAttaches(i);
  for(auto& i:attach)
    syncAttaches(i);
  for(auto& i:effects) {
    i.view.setObjMatrix(pos);
    // i.view.setTarget(targetPos);
    }
  pfx.view.setObjMatrix(pos);
  hnpcVisual.view.setObjMatrix(pos);
  if(torch.view!=nullptr) {
    auto& pose = *skInst;
    auto  p    = pos;
    if(torch.boneId<pose.boneCount())
      p.mul(pose.bone(torch.boneId));
    torch.view->setObjMatrix(p);
    }
  }

const Skeleton* MdlVisual::visualSkeleton() const {
  return skeleton;
  }

template<class View>
void MdlVisual::syncAttaches(Attach<View>& att) {
  if(att.view.isEmpty())
    return;
  auto& pose = *skInst;
  auto  p    = pos;
  if(att.boneId<pose.boneCount())
    p.mul(pose.bone(att.boneId));
  att.view.setObjMatrix(p);
  }

void MdlVisual::syncHead(MeshAttach& att) {
  if(att.view.isEmpty())
    return;
  // use BIP01 NECK?
  auto& pose = *skInst;
  auto  p    = pos;
  if(att.boneId<pose.boneCount())
    p.mul(pose.bone(att.boneId));
  if(headRotX!=0 || headRotY!=0) {
    p.rotateOY(headRotY);
    p.rotateOX(headRotX);
    }
  att.view.setObjMatrix(p);
  }

bool MdlVisual::startAnimItem(Npc &npc, std::string_view scheme, int state) {
  return skInst->setAnimItem(solver,npc,scheme,state);
  }

bool MdlVisual::startAnimSpell(Npc &npc, std::string_view scheme, bool invest) {
  char name[128]={};

  if(invest)
    std::snprintf(name,sizeof(name),"S_%.*sCAST", int(scheme.size()), scheme.data()); else
    std::snprintf(name,sizeof(name),"S_%.*sSHOOT",int(scheme.size()), scheme.data());

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,0,BS_CASTING,Pose::NoHint,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

bool MdlVisual::startAnimDialog(Npc &npc) {
  if((npc.bodyState()&BS_FLAG_FREEHANDS)==0 || fgtMode!=WeaponState::NoWeapon)
    return true;
  if(npc.bodyStateMasked()!=BS_STAND)
    return true;

  //const int countG1 = 21;
  const int countG2 = 11;
  const int id      = std::rand()%countG2 + 1;

  char name[32]={};
  std::snprintf(name,sizeof(name),"T_DIALOGGESTURE_%02d",id);

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,0,BS_STAND,Pose::NoHint,npc.world().tickCount()))
    return true;
  return false;
  }

void MdlVisual::startMMAnim(Npc&, std::string_view anim, std::string_view bone) {
  MdlVisual::MeshAttach* mesh[] = {&head,&sword,&bow,&ammunition,&stateItm};
  for(auto i:mesh) {
    if(i->bone!=bone)
      continue;
    i->view.startMMAnim(anim,1,uint64_t(-1));
    }
  }

void MdlVisual::startFaceAnim(Npc& npc, std::string_view anim, float intensity, uint64_t duration) {
  if(duration!=uint64_t(-1) && duration!=0)
    duration += npc.world().tickCount();
  head.view.startMMAnim(anim,intensity,duration);
  }

void MdlVisual::stopDlgAnim(Npc& npc) {
  //const int countG1 = 21;
  const int countG2 = 11;
  for(uint16_t i=0; i<countG2; i++){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",i+1);
    skInst->stopAnim(buf);
    }
  startFaceAnim(npc,"VISEME",1,0);
  }
