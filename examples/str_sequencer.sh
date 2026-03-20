#!/usr/bin/env bash

set -o errexit
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

TPROC_BUILD="${ROOT_DIR}/threadprocs/buildout"
ACTOR_BUILD="${ROOT_DIR}/buildout"

SERVER="${TPROC_BUILD}/server"
LAUNCHER="${TPROC_BUILD}/launcher"
STRSEQR="${ACTOR_BUILD}/examples/strseqr"
STROBSERVER="${ACTOR_BUILD}/examples/strobserver"
STRPRODUCER="${ACTOR_BUILD}/examples/strproducer"

SEQR_NAME="StringSequencer"

SOCK="${ACTOR_BUILD}/examples/str_sequencer.sock"
rm -f "${SOCK}"

cleanup() {
	set +e
	if [[ -n "${producer_pid:-}" ]]; then
		echo ">>> Stopping producer (pid ${producer_pid})..."
		kill -TERM "${producer_pid}" 2>/dev/null || true
		wait "${producer_pid}" 2>/dev/null || true
	fi
	if [[ -n "${observer_pid:-}" ]]; then
		echo ">>> Stopping observer (pid ${observer_pid})..."
		kill -TERM "${observer_pid}" 2>/dev/null || true
		wait "${observer_pid}" 2>/dev/null || true
	fi
	if [[ -n "${seqr_pid:-}" ]]; then
		echo ">>> Stopping sequencer (pid ${seqr_pid})..."
		kill -TERM "${seqr_pid}" 2>/dev/null || true
		wait "${seqr_pid}" 2>/dev/null || true
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

echo ">>> Launching sequencer into tproc server..."
${LAUNCHER} "${SOCK}" "${STRSEQR}" "${SEQR_NAME}" &
seqr_pid=$!
sleep 1
echo ">>> Sequencer running (pid ${seqr_pid})"

echo ">>> Launching observer into tproc server..."
${LAUNCHER} "${SOCK}" "${STROBSERVER}" "${SEQR_NAME}" &
observer_pid=$!
sleep 1
echo ">>> Observer running (pid ${observer_pid})"

echo ">>> Launching producer into tproc server..."
${LAUNCHER} "${SOCK}" "${STRPRODUCER}" "${SEQR_NAME}" &
producer_pid=$!
sleep 5
echo ">>> Letting strings flow for a few seconds..."

echo ">>> Done"
