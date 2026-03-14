# Ship of Harkinian — Web Builds

This branch is auto-managed by CI. Do not commit to it manually.

## Structure

- `/` — Production build from `feature/emscripten-web-port`
- `/{branch-name}/` — Preview builds from feature branches

Each push to a feature branch deploys its build to a subdirectory,
accessible at `https://zalo.github.io/Shipwright/{branch-name}/`.
