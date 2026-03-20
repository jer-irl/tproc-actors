#!/usr/bin/env bash

set -o errexit
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

TPROC_BUILD="${ROOT_DIR}/threadprocs/buildout"
ACTOR_BUILD="${ROOT_DIR}/buildout"

SERVER="${TPROC_BUILD}/server"
LAUNCHER="${TPROC_BUILD}/launcher"
PING="${ACTOR_BUILD}/examples/ping"
PONG="${ACTOR_BUILD}/examples/pong"

SOCK="${ACTOR_BUILD}/examples/ping_pong.sock"
rm -f "${SOCK}"

cleanup() {
	set +e
	if [[ -n "${pong_pid:-}" ]]; then
		echo ">>> Stopping PONG process (pid ${pong_pid})..."
		kill -TERM "${pong_pid}" 2>/dev/null || true
		wait "${pong_pid}" 2>/dev/null || true
	fi
	if [[ -n "${ping_pid:-}" ]]; then
		echo ">>> Stopping PING process (pid ${ping_pid})..."
		kill -TERM "${ping_pid}" 2>/dev/null || true
		wait "${ping_pid}" 2>/dev/null || true
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

echo ">>> Launching PONG process into tproc server..."
${LAUNCHER} "${SOCK}" "${PONG}" &
pong_pid=$!
sleep 1
echo ">>> PONG process running (pid ${pong_pid})"

echo ">>> Launching PING process into tproc server..."
${LAUNCHER} "${SOCK}" "${PING}"
ping_pid=

echo ">>> PING process exited"
echo ">>> Done"
