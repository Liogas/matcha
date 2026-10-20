# Matcha frontend

The frontend is a React and TypeScript application. It uses React Three Fiber
for an optional, decorative Three.js avatar and Motion for page transitions.
Navigation and forms remain standard, responsive React controls.

## Local development

Start the full local stack from the repository root:

```sh
docker compose up -d --build
```

The frontend container runs Vite at `http://localhost:5173`. Authentication
requests go to `http://localhost:18080` by default. Override that API origin
with `VITE_AUTH_API_URL` in a local, untracked `.env` file if needed. For
frontend-only work, run `docker compose up -d --build frontend`.

The backend's default Compose email URLs open the React pages at
`/verify-email` and `/reset-password`. The verification page only consumes its
token after an explicit click. Both pages submit tokens to the existing backend
endpoints; the refresh token remains in the backend's HttpOnly cookie, and the
short-lived access token is kept in memory by the frontend.
The frontend web server must serve the application entry point for these
client-side routes in production as well.

For the browser flow to work, the backend's `AUTH_ALLOWED_ORIGIN` must match
the frontend origin (the local default is `http://localhost:5173`). Mailpit is
available at `http://localhost:8025` when the Compose services are running.

## Checks

```sh
npm run lint
npm run build
```
