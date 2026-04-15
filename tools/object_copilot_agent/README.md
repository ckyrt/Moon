# Object Copilot Agent

This folder contains the first Python service scaffold for the standalone Object Copilot prototype.

## Endpoints

- `GET /health`
- `POST /agent/patch`

## Run

```bash
python -m uvicorn app:app --host 127.0.0.1 --port 8008
```

The current implementation is intentionally minimal. It fixes the service contract early so the C++ editor can evolve independently of the final agent loop.
