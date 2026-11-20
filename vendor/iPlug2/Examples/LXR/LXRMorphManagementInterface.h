#pragma once

// Minimal non-automatable target-B ingress for P18-B. File I/O is synchronous
// and non-audio; morph amount remains the ordinary host parameter 245.

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace lxr::vst
{

struct MorphOperationWire final
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
  Steinberg::uint32 targetPresent = 0;
  Steinberg::uint32 amount = 0;
  Steinberg::uint64 generation = 0;
  Steinberg::uint8 name[8] {};
};

class IMorphManagement : public Steinberg::FUnknown
{
public:
  virtual Steinberg::tresult PLUGIN_API importMorphSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    MorphOperationWire* result) = 0;
  virtual Steinberg::tresult PLUGIN_API clearMorphTarget(
    MorphOperationWire* result) = 0;
  virtual Steinberg::tresult PLUGIN_API exportMorphSnd(
    const Steinberg::Vst::TChar* path,
    Steinberg::uint32 pathCharacters,
    MorphOperationWire* result) = 0;
  virtual Steinberg::tresult PLUGIN_API cancelMorphOperation(
    Steinberg::uint32 action,
    MorphOperationWire* result) = 0;
  virtual Steinberg::tresult PLUGIN_API getMorphState(
    MorphOperationWire* result) = 0;

  static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID(IMorphManagement,
  0x768F676A, 0xE8F044C3, 0xA9B04F39, 0x981ABF52)

} // namespace lxr::vst
