# Integration-reference policy

Host-specific systems may be mentioned in clearly isolated documentation or examples when that helps explain how to consume the library. Such references are illustrative only.

The architectural rule remains strict: core source, public headers, configuration structures, and build dependencies must not acquire host-specific types or assumptions merely because an example integration needs them. Dependency direction stays from adapters toward the core.

When an integration motivates a useful new facility, first identify the general scrolling abstraction, give it an independent interface, and test it without requiring the motivating host. This keeps later additions composable and lets consumers use only the portions of the repository they need.

See `INTEGRATION_EXAMPLES.md` for the adapter pattern and a deliberately quarantined libinput example.
