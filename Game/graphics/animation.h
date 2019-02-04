#pragma once

#include <zenload/modelScriptParser.h>

class Animation final {
  public:
    enum Flags : uint32_t {
      None         = 0,
      Move         = 0x00000001,
      Rotate       = 0x00000002,
      QueueAnim    = 0x00000004,
      Fly          = 0x00000008,
      Idle         = 0x00000010
      };

    struct Sequence final {
      Sequence(const std::string& name);

      bool isMove() const { return bool(flags&Flags::Move); }

      std::string                            name;
      float                                  fpsRate=60.f;
      uint32_t                               numFrames=0;
      Flags                                  flags=Flags::None;

      std::vector<ZenLoad::zCModelAniSample> samples;
      std::vector<uint32_t>                  nodeIndex;

      std::string                            next;
      ZenLoad::zCModelAniSample              moveTr={};

      ZMath::float3                          speed(uint64_t at, uint64_t dt) const;

      private:
        void setupMoveTr();
      };

    Animation(ZenLoad::ModelScriptBinParser& p, const std::string &name);

    const Sequence& sequence(const char* name) const;

  private:
    std::vector<Sequence> sequences;
    Sequence& loadMAN(const std::string &name);
  };
