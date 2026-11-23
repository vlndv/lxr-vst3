#include "dsp/SequencerEditFacade.h"

#include <cstdint>
#include <cstring>
#include <cstdio>

namespace
{
int checks = 0;
int failures = 0;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

using Facade = lxr::sequencer::SequencerEditFacade;

Facade::Request request(Facade::Operation operation)
{
    Facade::Request result;
    result.operation = operation;
    return result;
}

void testStepEditing()
{
    lxr::sequencer::PatternStateAuthority authority;
    Facade facade(authority);
    Facade::PatternSnapshot before;
    check(facade.readPattern(0u, before) == Facade::Result::Ok,
          "read immutable initial pattern");
    const auto stored = before.pattern.steps[2][lxr::sequencer::PhysicalPosition(4, 3)];

    auto gate = request(Facade::Operation::ToggleMainGate);
    gate.track = 2u; gate.mainStep = 4u;
    auto result = facade.apply(gate, before.generation);
    check(result.status == Facade::Result::Ok, "toggle main gate publishes");
    Facade::PatternSnapshot enabled;
    check(facade.readPattern(0u, enabled) == Facade::Result::Ok,
          "read enabled generation");
    check((enabled.pattern.mainGates[2] & (1u << 4u)) != 0u,
          "main gate enabled");

    auto substep = request(Facade::Operation::SetSubstep);
    substep.track = 2u; substep.mainStep = 4u; substep.substep = 3u;
    substep.active = true;
    result = facade.apply(substep, enabled.generation);
    check(result.status == Facade::Result::Ok, "physical substep publishes");
    Facade::PatternSnapshot withSubstep;
    facade.readPattern(0u, withSubstep);
    check(lxr::sequencer::IsStepActive(withSubstep.pattern.steps[2][35]),
          "physical substep is independently active");

    result = facade.apply(gate, withSubstep.generation);
    check(result.status == Facade::Result::Ok, "main gate disables");
    Facade::PatternSnapshot disabled;
    facade.readPattern(0u, disabled);
    check((disabled.pattern.mainGates[2] & (1u << 4u)) == 0u,
          "main gate is off");
    check(lxr::sequencer::IsStepActive(disabled.pattern.steps[2][35]),
          "main gate off retains stored physical substep");
    check(disabled.pattern.steps[2][35].note == stored.note,
          "gate toggle does not change note");

    for (std::uint8_t value : {std::uint8_t(0), std::uint8_t(63), std::uint8_t(127)})
    {
        auto note = request(Facade::Operation::SetNote);
        note.track = 2u; note.mainStep = 4u; note.substep = 3u; note.value = value;
        result = facade.apply(note, disabled.generation);
        check(result.status == Facade::Result::Ok, "note boundary accepted");
        facade.readPattern(0u, disabled);
        check(disabled.pattern.steps[2][35].note == value, "exact note stored");
    }
    for (std::uint8_t value : {std::uint8_t(0), std::uint8_t(64), std::uint8_t(127)})
    {
        auto velocity = request(Facade::Operation::SetVelocity);
        velocity.track = 2u; velocity.mainStep = 4u; velocity.substep = 3u;
        velocity.value = value;
        result = facade.apply(velocity, disabled.generation);
        check(result.status == Facade::Result::Ok, "velocity boundary accepted");
        facade.readPattern(0u, disabled);
        const auto& step = disabled.pattern.steps[2][35];
        check(lxr::sequencer::StepVelocity(step) == value, "exact velocity stored");
        check(lxr::sequencer::IsStepActive(step), "velocity preserves active bit");
    }
    for (std::uint8_t value : {std::uint8_t(0), std::uint8_t(1), std::uint8_t(127)})
    {
        auto probability = request(Facade::Operation::SetProbability);
        probability.track = 2u; probability.mainStep = 4u;
        probability.substep = 3u; probability.value = value;
        result = facade.apply(probability, disabled.generation);
        check(result.status == Facade::Result::Ok, "probability boundary accepted");
        facade.readPattern(0u, disabled);
        check(disabled.pattern.steps[2][35].probability == value,
              "source probability stored exactly");
    }
}

void testLocksAndValidation()
{
    lxr::sequencer::PatternStateAuthority authority;
    Facade facade(authority);
    Facade::PatternSnapshot state;
    facade.readPattern(0u, state);
    for (std::uint8_t slot = 0u; slot < 2u; ++slot)
    {
        auto target = request(Facade::Operation::SetLockTarget);
        target.track = 3u; target.mainStep = 6u; target.substep = 5u;
        target.slot = slot; target.value = 41u; // lower selector -> patch 40
        auto applied = facade.apply(target, state.generation);
        check(applied.status == Facade::Result::Ok, "safe lock target accepted");
        facade.readPattern(0u, state);
        auto lockValue = request(Facade::Operation::SetLockValue);
        lockValue.track = 3u; lockValue.mainStep = 6u; lockValue.substep = 5u;
        lockValue.slot = slot; lockValue.value = slot == 0u ? 17u : 91u;
        applied = facade.apply(lockValue, state.generation);
        check(applied.status == Facade::Result::Ok, "lock value accepted");
        facade.readPattern(0u, state);
    }
    const auto& step = state.pattern.steps[3][53];
    check(step.parameter1 == 41u && step.value1 == 17u,
          "lock slot one retained");
    check(step.parameter2 == 41u && step.value2 == 91u,
          "duplicate target slot two retained for source precedence");

    auto invalid = request(Facade::Operation::SetLockTarget);
    invalid.track = 3u; invalid.mainStep = 6u; invalid.substep = 5u;
    invalid.slot = 0u; invalid.value = 1u;
    const auto rejected = facade.apply(invalid, state.generation);
    check(rejected.status == Facade::Result::InvalidLockTarget,
          "mod-wheel no-op lock target rejected");
    Facade::PatternSnapshot unchanged;
    facade.readPattern(0u, unchanged);
    check(unchanged.generation == state.generation &&
          unchanged.pattern.steps[3][53].parameter1 == 41u,
          "invalid lock leaves accepted record unchanged");

    auto clear = request(Facade::Operation::ClearLock);
    clear.track = 3u; clear.mainStep = 6u; clear.substep = 5u; clear.slot = 0u;
    check(facade.apply(clear, unchanged.generation).status == Facade::Result::Ok,
          "clear lock slot publishes");
    facade.readPattern(0u, unchanged);
    check(unchanged.pattern.steps[3][53].parameter1 == 0xffu &&
          unchanged.pattern.steps[3][53].value1 == 0u,
          "cleared slot uses firmware sentinel");
}

void testManagementAndConcurrency()
{
    lxr::sequencer::PatternStateAuthority authority;
    Facade facade(authority);
    Facade::PatternSnapshot state;
    facade.readPattern(0u, state);

    auto length = request(Facade::Operation::SetTrackLength);
    length.track = 6u; length.value = 11u;
    check(facade.apply(length, state.generation).status == Facade::Result::Ok,
          "track length publishes"); facade.readPattern(0u, state);
    auto rotation = request(Facade::Operation::SetRotation);
    rotation.track = 6u; rotation.value = 15u;
    check(facade.apply(rotation, state.generation).status == Facade::Result::Ok,
          "rotation publishes"); facade.readPattern(0u, state);
    check(state.pattern.lengthCodes[6] == 11u && state.pattern.rotations[6] == 15u,
          "length and rotation source encodings retained");

    auto chain = request(Facade::Operation::SetPatternChain);
    chain.value = 127u; chain.value2 = 14u;
    check(facade.apply(chain, state.generation).status == Facade::Result::Ok,
          "repeat and random-next code publish"); facade.readPattern(0u, state);
    check(state.pattern.repeat == 127u && state.pattern.nextPattern == 14u,
          "random-next 14 remains metadata not pattern index");
    auto shuffle = request(Facade::Operation::SetShuffle); shuffle.value = 127u;
    check(facade.apply(shuffle, state.generation).status == Facade::Result::Ok,
          "shuffle publishes"); facade.readPattern(0u, state);
    check(state.shuffle == 127u, "shuffle source domain retained");

    auto euclidean = request(Facade::Operation::GenerateEuclidean);
    euclidean.track = 1u; euclidean.value = 16u;
    euclidean.value2 = 5u; euclidean.value3 = 3u;
    const auto stepsBefore = state.pattern.steps[1];
    check(facade.apply(euclidean, state.generation).status == Facade::Result::Ok,
          "existing Euclidean operation publishes"); facade.readPattern(0u, state);
    check(std::memcmp(state.pattern.steps[1].data(), stepsBefore.data(),
                      sizeof(stepsBefore)) == 0,
          "Euclidean preserves all physical step records");

    auto copy = request(Facade::Operation::CopyPattern);
    copy.source = 0u; copy.destination = 1u;
    Facade::PatternSnapshot destinationBefore; facade.readPattern(1u, destinationBefore);
    check(facade.apply(copy, state.generation).status == Facade::Result::Ok,
          "pattern copy publishes");
    Facade::PatternSnapshot destination; facade.readPattern(1u, destination);
    check(destination.pattern.repeat == destinationBefore.pattern.repeat &&
          destination.pattern.nextPattern == destinationBefore.pattern.nextPattern,
          "pattern copy preserves destination chain metadata");
    auto clear = request(Facade::Operation::ClearPattern); clear.pattern = 1u;
    check(facade.apply(clear, destination.generation).status == Facade::Result::Ok,
          "pattern clear publishes"); facade.readPattern(1u, destination);
    check(destination.pattern.repeat == destinationBefore.pattern.repeat &&
          destination.pattern.nextPattern == destinationBefore.pattern.nextPattern,
          "pattern clear preserves chain metadata");

    const auto oldGeneration = destination.generation;
    auto note = request(Facade::Operation::SetNote); note.value = 12u;
    check(facade.apply(note, oldGeneration).status == Facade::Result::Ok,
          "first ordered edit accepted");
    check(facade.apply(note, oldGeneration).status == Facade::Result::Stale,
          "stale generation rejected without overwrite");

    lxr::sequencer::PatternStateAuthority::WriteHandle held;
    check(authority.tryBeginEdit(held) == lxr::sequencer::Status::Ok,
          "hold one writer for Busy test");
    check(facade.apply(note, authority.generation()).status == Facade::Result::Busy,
          "concurrent writer returns bounded Busy");
    held.cancel();

    for (std::uint8_t value = 0u; value < 64u; ++value)
    {
        Facade::PatternSnapshot current; facade.readPattern(0u, current);
        note.value = value;
        const auto applied = facade.apply(note, current.generation);
        check(applied.status == Facade::Result::Ok,
              "rapid sequential edit remains ordered");
    }
    Facade::PatternSnapshot final; facade.readPattern(0u, final);
    check(final.pattern.steps[0][0].note == 63u,
          "rapid edits end at final accepted value");

    lxr::sequencer::PatternStateAuthority otherAuthority;
    Facade other(otherAuthority);
    Facade::PatternSnapshot isolated; other.readPattern(0u, isolated);
    check(isolated.pattern.steps[0][0].note == lxr::sequencer::kDefaultNote,
          "second instance pattern bank remains independent");
}

void testTransientSchedulerControls()
{
    lxr::sequencer::TriggerScheduler scheduler;
    Facade facade(scheduler);
    for (std::uint8_t pattern = 0u; pattern < 8u; ++pattern)
    {
        check(facade.selectPattern(pattern) == Facade::Result::Ok,
              "stopped pattern selection accepted");
        check(facade.activePattern() == pattern &&
              facade.pendingPattern() == pattern,
              "stopped active and pending pattern coherent");
    }
    for (std::uint8_t track = 0u; track < 7u; ++track)
    {
        check(facade.setTrackMuted(track, true) == Facade::Result::Ok &&
              facade.trackMuted(track), "transient logical-track mute set");
        check(facade.setTrackMuted(track, false) == Facade::Result::Ok &&
              !facade.trackMuted(track), "transient logical-track mute released");
        check(facade.setRoll(track, true) == Facade::Result::Ok,
              "transient roll press accepted");
        check(facade.setRoll(track, false) == Facade::Result::Ok,
              "transient roll release accepted");
    }
    check(scheduler.mutedTracksMask() == 0u && scheduler.rollTracksMask() == 0u,
          "transient controls finish released");
    check(facade.setTrackMuted(7u, true) == Facade::Result::InvalidTrack &&
          facade.setRoll(7u, true) == Facade::Result::InvalidTrack,
          "invalid transient track rejected");
}
}

int main()
{
    testStepEditing();
    testLocksAndValidation();
    testManagementAndConcurrency();
    testTransientSchedulerControls();
    if (failures == 0) std::printf("PASS (%d checks)\n", checks);
    return failures == 0 ? 0 : 1;
}
