#pragma once

// Small non-automatable VST3 interface used by the editor and production-path
// verification. It carries no audio-thread callback and no file bytes.

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace lxr::vst
{

struct KitOperationWire final
{
  Steinberg::uint32 action = 0;
  Steinberg::uint32 status = 0;
  Steinberg::uint32 fileError = 0;
  Steinberg::uint32 codecError = 0;
  Steinberg::uint32 issue = 0;
  Steinberg::uint32 publicationStatus = 0;
  Steinberg::uint32 field = 0xffffu;
  Steinberg::uint32 route = 0xffu;
  Steinberg::uint32 nativeError = 0;
  Steinberg::uint32 published = 0;
  Steinberg::uint8 name[8] {};
};

class IKitManagement : public Steinberg::FUnknown
{
public:
  virtual Steinberg::tresult PLUGIN_API importSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    KitOperationWire* result) = 0;
  virtual Steinberg::tresult PLUGIN_API exportSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    KitOperationWire* result) = 0;
  virtual Steinberg::tresult PLUGIN_API getKitOperation(
    KitOperationWire* result) = 0;

  static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID(IKitManagement,
  0x8E44215B, 0xEBBD4C52, 0xA714F47B, 0x2489C9D1)

} // namespace lxr::vst
