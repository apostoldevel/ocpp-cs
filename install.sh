#!/usr/bin/env bash
#
# OCPP Central System — one-line installer
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/apostoldevel/ocpp-cs/master/install.sh | bash
#
# What it does:
#   1. Checks that Docker is installed
#   2. Pulls the latest image from Docker Hub
#   3. Starts the Central System on port 9220
#   4. Prints connection instructions for your charging station
#

set -euo pipefail

# ── Colors ──────────────────────────────────────────────────────────────────

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

# ── Functions ───────────────────────────────────────────────────────────────

info()  { echo -e "${CYAN}[info]${NC}  $*"; }
ok()    { echo -e "${GREEN}[ok]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[warn]${NC}  $*"; }
fail()  { echo -e "${RED}[error]${NC} $*"; exit 1; }

# ── Pre-flight checks ──────────────────────────────────────────────────────

echo ""
echo -e "${BOLD}OCPP Central System — Installer${NC}"
echo -e "${DIM}https://github.com/apostoldevel/ocpp-cs${NC}"
echo ""

if ! command -v docker &>/dev/null; then
    fail "Docker is not installed. Please install Docker first: https://docs.docker.com/get-docker/"
fi

if ! docker info &>/dev/null 2>&1; then
    fail "Docker daemon is not running. Please start Docker and try again."
fi

# ── Configuration ───────────────────────────────────────────────────────────

IMAGE="apostoldevel/cs"
CONTAINER="cs"
PORT="${CS_PORT:-9220}"
WEBHOOK_URL="${WEBHOOK_URL:-https://api.ocpp-css.com/api/v1/ocpp/parse}"

# ── Stop existing container if running ──────────────────────────────────────

if docker ps -q -f name="^${CONTAINER}$" | grep -q .; then
    warn "Container '${CONTAINER}' is already running. Stopping..."
    docker stop "${CONTAINER}" >/dev/null 2>&1 || true
    docker rm "${CONTAINER}" >/dev/null 2>&1 || true
    ok "Old container removed"
fi

# ── Pull latest image ──────────────────────────────────────────────────────

info "Pulling ${IMAGE}:latest ..."
docker pull "${IMAGE}:latest"
ok "Image pulled"

# ── Start container ─────────────────────────────────────────────────────────

info "Starting Central System on port ${PORT}..."
docker run -d \
    --name "${CONTAINER}" \
    -p "${PORT}:9220" \
    -e "WEBHOOK_URL=${WEBHOOK_URL}" \
    --restart unless-stopped \
    "${IMAGE}:latest" >/dev/null

ok "Central System is running"

# ── Detect host IP ──────────────────────────────────────────────────────────

HOST_IP=""
if command -v hostname &>/dev/null; then
    HOST_IP=$(hostname -I 2>/dev/null | awk '{print $1}') || true
fi
if [ -z "${HOST_IP}" ]; then
    HOST_IP=$(ip -4 route get 1.0.0.0 2>/dev/null | awk '{print $7; exit}') || true
fi
if [ -z "${HOST_IP}" ]; then
    HOST_IP="YOUR_SERVER_IP"
fi

# ── Success message ─────────────────────────────────────────────────────────

echo ""
echo -e "${GREEN}${BOLD}Central System is ready!${NC}"
echo ""
echo -e "  ${BOLD}Web UI:${NC}      http://localhost:${PORT}"
echo -e "  ${BOLD}API Docs:${NC}    http://localhost:${PORT}/docs/"
echo ""
echo -e "${DIM}────────────────────────────────────────────────────────────${NC}"
echo ""
echo -e "${BOLD}Connect your charging station${NC}"
echo ""
echo -e "  Open your station's configuration panel and set the"
echo -e "  Central System URL to one of the following:"
echo ""
echo -e "  ${BOLD}OCPP 1.6 JSON (WebSocket):${NC}"
echo -e "    ${CYAN}ws://${HOST_IP}:${PORT}/ocpp/YOUR_STATION_ID${NC}"
echo ""
echo -e "  ${BOLD}OCPP 1.5 SOAP:${NC}"
echo -e "    ${CYAN}http://${HOST_IP}:${PORT}/Ocpp/CentralSystemService/${NC}"
echo ""
echo -e "  Replace ${YELLOW}YOUR_STATION_ID${NC} with any identifier for your station"
echo -e "  (e.g. ${DIM}MY-CHARGER-001${NC}). The station will appear in the Web UI"
echo -e "  automatically after it connects."
echo ""
echo -e "${DIM}────────────────────────────────────────────────────────────${NC}"
echo ""
echo -e "${BOLD}Common station configuration examples:${NC}"
echo ""
echo -e "  ${DIM}# ABB Terra / EVBox / Webasto / KEBA:${NC}"
echo -e "  Central System URI:  ${CYAN}ws://${HOST_IP}:${PORT}/ocpp${NC}"
echo -e "  Charge Point ID:     ${CYAN}MY-CHARGER-001${NC}"
echo ""
echo -e "  ${DIM}# Stations with full URL format:${NC}"
echo -e "  ${CYAN}ws://${HOST_IP}:${PORT}/ocpp/MY-CHARGER-001${NC}"
echo ""
echo -e "${DIM}────────────────────────────────────────────────────────────${NC}"
echo ""
echo -e "  ${DIM}Manage:  docker stop cs | docker start cs | docker logs cs${NC}"
echo -e "  ${DIM}Remove:  docker rm -f cs && docker rmi ${IMAGE}${NC}"
echo ""
