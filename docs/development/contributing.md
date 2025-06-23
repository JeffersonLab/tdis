Contributing
============

## Coding Conventions

- Use `std::shared_ptr` and `std::unique_ptr` internally, but don't make them be part of the API.
- Use `#pragma once` instead of traditional header guards
- Methods that are part of the API are always in `PascalCase()`; methods that are not part of the contract with the user _may_ be `snake_cased`, though new code should make everything Pascal-cased. Member variables are snake-cased and prefixed with `m_`. Indent using 4 spaces, not tabs.



