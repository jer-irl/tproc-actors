#!/usr/bin/env bash

set -o errexit
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

TPROC_BUILD="${ROOT_DIR}/threadprocs/buildout"
ACTOR_BUILD="${ROOT_DIR}/buildout"

SERVER="${TPROC_BUILD}/server"
LAUNCHER="${TPROC_BUILD}/launcher"
TIMING_SERVICE="${ACTOR_BUILD}/examples/timing_service"
TIMING_REQUESTER="${ACTOR_BUILD}/examples/timing_requester"

SERVICE_NAME="TimingService"

SOCK="${ACTOR_BUILD}/examples/timing_requester.sock"
rm -f "${SOCK}"

cleanup() {
	set +e
	if [[ -n "${requester_pid:-}" ]]; then
		echo ">>> Stopping timing requester (pid ${requester_pid})..."
		kill -TERM "${requester_pid}" 2>/dev/null || true
		wait "${requester_pid}" 2>/dev/null || true
	fi
	if [[ -n "${service_pid:-}" ]]; then
		echo ">>> Stopping timing service (pid ${service_pid})..."
		kill -TERM "${service_pid}" 2>/dev/null || true
		wait "${service_pid}" 2>/dev/null || true
	fi
	if [[ -n "${server_pid:-}" ]]; then
		echo ">>> Stopping tproc server (pid ${server_pid})..."
		kill -TERM "${server_pid}" 2>/dev/null || true
		wait "${server_pid}" 2>/dev/null || true
	fi
	rm -f "${SOCK}"
}
trap cleanup EXIT

echo ">>> Launching tproc server on ${SOCK}..."
${SERVER} "${SOCK}" &
server_pid=$!
sleep 1
echo ">>> tproc server running (pid ${server_pid})"

echo ">>> Launching timing service into tproc server..."
${LAUNCHER} "${SOCK}" "${TIMING_SERVICE}" "${SERVICE_NAME}" &
service_pid=$!
sleep 1
echo ">>> Timing service running (pid ${service_pid})"

echo ">>> Launching timing requester into tproc server..."
${LAUNCHER} "${SOCK}" "${TIMING_REQUESTER}" "${SERVICE_NAME}" &
requester_pid=$!
sleep 5
echo ">>> Letting callbacks fire for a few seconds..."

echo ">>> Done"
