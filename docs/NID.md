# NID Support

The HLE layer supports forward NID hashing and a small built-in name database.

## Forward hashing

Nid::Compute converts a public symbol name into the 11-character NID used by
qualified imports. The implementation is covered by known public test vectors.

## Name database

NidNameDatabase stores symbol-name to NID mappings and supports reverse NID
lookup for diagnostics and service registration tooling. Entries are verified
through the same forward hashing implementation when added.

The built-in database is intentionally small and contains only names already
used by the project's public test vectors. It is infrastructure, not a claim of
complete platform coverage. More names can be added from lawful public sources
without changing the lookup API.
