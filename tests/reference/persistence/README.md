# Proposed persistence contract — test only

P15-M2F does not install LXRS v2 or persistent routes. `StateContractModel.h`
is an executable reference model in `lxr::m2f_test`, never production code.
Its vectors and test allocations are not an audio-thread implementation.
The declarative proposal is `interfaces/P15M2FPersistenceContract.h`.

The fixture CSV is a byte-identical copy of the accepted M2A original-C target
audit. Original source/table semantics are Copyright 2013 Julian Schmidt.
All derived metadata, fixtures and code remain subject to `LICENSE-LXR.txt`
(non-commercial use; modified redistribution includes full source and notices).

The Python fixture generator uses explicit little-endian `struct` fields and
standard-library `zlib.crc32`, independently of the C++ encoder and decoder.
`fixtures/receipt.json` records the original source hash, CSV hash and every
binary hash. The live tests also compile unchanged original AVR and STM32 C
metadata fixtures. The proposed physical key is the minimum original PAR in a
storage-alias class, checked against original C pointer equality, never a C++
enum ordinal or process pointer.

Run from the repository root with documented pinned dependencies present:

```powershell
python tests/reference/persistence/run_contract_tests.py --output out/p15-m2f/new-contract-run
python tests/reference/persistence/make_independent_fixtures.py --output out/p15-m2f/new-independent-fixtures
```

Both commands refuse existing destinations. The runner records exact compiler
commands, input hashes/copies, executable hashes, logs and O0/O2 results. Native
test/model translation units use C++17, exceptions/RTTI disabled and
`-ffp-contract=off`. The unchanged third-party `IPlugParameter.cpp` is separately
compiled with RTTI enabled, as its `GetShapeID/GetShapeValue` use dynamic_cast;
this is the documented framework exception, not a DSP flag change. No plugin is
built or loaded. WDL's Windows `fopen` macro is undef'd only in the test source
after its includes to allow `std::fopen` to read fixtures.

Tests cover the proposed wire format, legacy migrations, all original identities,
the exact 107-row runtime profile, 65,536 host token conversions through real
IParam, corruption, future optional/mandatory tags, and a sequential publication
ordering model. This is not a concurrent implementation or a live VST3 host test.
