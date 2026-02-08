# MVP Layering

This MVP is intentionally split into two layers.

## 1) Generic infra layer

These files are reusable for any app:

- `src/infra/app/host.hpp`: process host lifecycle (`build/start`, signal stop, `stop/join`, setup/teardown hooks).
- `src/infra/topology/spec.hpp`: graph model (`Topology`, `NodeSpec`, `EdgeSpec`).
- `src/infra/topology/ports.hpp`: generic templated `Inbox<T>` / `Outbox<T>`.
- `src/infra/topology/engine.hpp`: queue allocation, port binding, worker launch.
- `src/infra/topology/wire.hpp`: generic binding helpers.
- `src/infra/memory/arena.hpp`: arena-backed allocator used by queue storage.

This layer does not know about `md`, `strat`, `or`, or any app contracts.

## 2) MVP app assembly layer

These files are MVP-specific:

- `apps/mvp/contracts.hpp`: concrete envelope contracts (`TickEnvelope`, `OrderReqEnvelope`, `OrderAckEnvelope`).
- `apps/mvp/topology.hpp`: concrete node list, edge list, queue depths, core mapping.
- `apps/mvp/nodes.hpp` and `apps/mvp/nodes.cpp`: concrete node logic and role-specific port bundles.
- `apps/mvp/wiring.hpp` (`namespace magus2::mvp::assembly`): app assembly that maps:
  - concrete node port members -> generic binder helpers,
  - concrete node constructors -> generic worker runtime.

This is the first place concrete roles are named and coupled.

## Practical rule

- If you add a new trading app role/contract, change `apps/mvp/*`.
- If you improve queue/runtime/binding machinery for all apps, change `src/infra/topology/*` and `src/infra/memory/*`.
