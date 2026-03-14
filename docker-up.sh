#!/usr/bin/env bash
set -euo pipefail

docker compose up -d --force-recreate
docker compose logs -fn 500
