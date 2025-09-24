Contributing
============

## Coding Conventions

- Use `std::shared_ptr` and `std::unique_ptr` internally, but don't make them be part of the API.
- Use `#pragma once` instead of traditional header guards
- Use Acts::UnitConstants when working with any values that have units. Shortcut for code readability 
  ```c++
  namespace Units = Acts::UnitConstants;
  ```   

## Style

### Formatting

- Indent using 4 spaces, not tabs.

### Naming conventions
- classes and types are `PascalCase`
- Methods are in `camelCase()`;
- Local variables are `camelCase`;
- Member variables are camelCase and prefixed with `m_` e.g. `m_camelCase`.
- JANA2 factories, processors and event sources have parameters, services, inputs and outputs, those names are: 
  - Parameters have `m_cfg_` prefix e.g. `m_cfg_xxxYyy`
  - Inputs have `m_in_` prefix e.g. `m_in_xxxYyy`
  - Outputs have `m_out_` prefix e.g. `m_out_xxxYyy`
  - Services have `m_svc_` prefix: `m_svc_xxxYyy`
