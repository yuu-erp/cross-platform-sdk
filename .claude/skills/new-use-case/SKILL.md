---
name: new-use-case
description: Implement a business use case inside a module following Clean Architecture (domain objects, use case class, ports, adapter, facade method, unit tests with fakes). Use when adding a feature such as "get wallet balance" or "call contract method".
---
# New use case

Reference: `src/modules/network/application/connect_to_node.*` and its tests in
`tests/unit/network/application/`.

1. Clarify the intent as `<Verb><Noun>` (e.g. `GetBalance`), inputs, outputs, error cases.
   Do not invent remote API shapes — if the backend/RPC contract is unknown, ask for it.
2. Domain: add/extend value objects & invariants (`domain/`), built on `src/core/domain`
   (`core::StrongType` ids, `core::DomainEvent`, `core::SecureBytes`). Validation lives
   in factories returning `core::Result<T>` (`Address::parse(str)`).
3. Ports: list what the use case needs from outside; reuse core ports first (`core::IClock`,
   `core::ILogger`, `core::IEventBus`), then an existing module port if it fits,
   otherwise add `application/ports/i_<need>.h` (pure virtual, minimal methods).
4. Use case: `application/<verb>_<noun>.{h,cpp}`
   ```cpp
   class GetBalance {
   public:
       GetBalance(IRpcClient& rpc, IClock& clock);
       [[nodiscard]] Result<Balance> execute(const GetBalanceInput& in) const;
   };
   ```
5. Tests first (`tests/unit/<m>/application/get_balance_test.cpp`, GoogleTest) with fakes for
   ports (`tests/support/core/FakeClock`, ...): happy path, each error, boundary values.
6. Adapter in `infrastructure/` implementing new ports; integration test if it does real I/O.
7. Expose through `api/<m>_facade` and, if public, via the `new-c-api` skill.
8. Wire in `composition_root.cpp`. Run `verify`.
