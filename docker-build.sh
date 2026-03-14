#!/usr/bin/env bash
set -euo pipefail

./docker-image-rm.sh

docker compose build

docker image list