#pragma once
// P15-M2F PROPOSED TEST CONTRACT ONLY. Not included by production serialization.
// Original table identities/semantics: Copyright 2013 Julian Schmidt.
// Derived material remains subject to LICENSE-LXR.txt.
#include <array>
#include <cstdint>
#include <cstddef>
namespace lxr::m2f_proposal {
inline constexpr std::uint16_t kVersion=2, kHeaderBytes=32, kRouteVersion=1;
inline constexpr std::uint32_t kParameterBytes=228, kMaximumPluginBytes=4092;
inline constexpr std::uint32_t kMaximumStreamBytes=4096, kRouteTag=1;
inline constexpr std::uint16_t kRouteCount=12, kRouteBytes=16, kRouteHeaderBytes=48;
inline constexpr std::uint16_t kProfile=1, kHostMaximum=65535;
inline constexpr std::uint32_t kIdentityTable=1;
inline constexpr std::uint16_t kFirstVelocityParam=228, kFirstLfoParam=234;
// Public IDs 228..239 are a proposal; no parameters are added by this phase.
// Values 0..204 retain original AVR row identity. Other 16-bit values are
// reserved/rejected in profile 1; 0x8000|PAR is reserved for a future STM32 profile.
enum class Source : std::uint8_t { Velocity=0, Lfo=1 };
enum class IdentityNamespace : std::uint16_t { Off=0, Avr037=1, Stm32Reserved=2 };
struct Route {
 std::uint8_t owner=0, source=0;
 std::uint16_t flags=0, nameSpace=0, index=0, parameter=0;
 std::uint8_t type=255, scope=0;
 std::uint16_t physicalKey=0;
 std::uint8_t menuHint=0, reserved=0;
};
// These C++ fields describe logical data, never a packed object wire layout.
// The normative offsets and validation rules are in the M2F design/schema.
} // namespace lxr::m2f_proposal
