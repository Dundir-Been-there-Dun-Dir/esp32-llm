#!/usr/bin/env bash
# Install the ESP-IDF toolchain and build/flash this project.
#
# Usage:
#   ./setup.sh install            # install system packages + ESP-IDF (one time)
#   ./setup.sh build              # build the firmware
#   ./setup.sh flash [PORT]       # flash firmware + model data (default: auto-detect port)
#   ./setup.sh monitor [PORT]     # open the serial monitor (Ctrl+] to exit)
#   ./setup.sh run [PORT]         # build, flash and monitor
#   ./setup.sh log [PORT] [SECS]  # reset the board and save its output to logs/ (default 60s, Ctrl+C stops early)
#   ./setup.sh talk [PORT]        # type the start of a story and the board continues it (Ctrl+C to quit)
#
# PORT can be left out (auto-detect), given as a short name like usb0 or acm0
# (for /dev/ttyUSB0 or /dev/ttyACM0), or as a full path.
#
# Environment overrides:
#   IDF_VERSION  ESP-IDF branch/tag to install (default: release/v5.3, matching dependencies.lock)
#   IDF_DIR      where to install ESP-IDF     (default: ~/esp/esp-idf)
set -euo pipefail

IDF_VERSION="${IDF_VERSION:-release/v5.3}"
IDF_DIR="${IDF_DIR:-$HOME/esp/esp-idf}"
TARGET="esp32"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

install_system_packages() {
    if ! command -v apt-get >/dev/null; then
        log "Not a Debian/Ubuntu system; install these packages manually:"
        echo "  git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0"
        return
    fi
    log "Installing system packages (needs sudo)"
    sudo apt-get update
    sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
        cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

    if ! id -nG "$USER" | grep -qw dialout; then
        log "Adding $USER to the 'dialout' group for serial port access (log out and back in afterwards)"
        sudo usermod -aG dialout "$USER"
    fi
}

install_idf() {
    if [ -d "$IDF_DIR/.git" ]; then
        log "ESP-IDF already present at $IDF_DIR, updating to $IDF_VERSION"
        git -C "$IDF_DIR" fetch --depth 1 origin "$IDF_VERSION"
        git -C "$IDF_DIR" checkout -q FETCH_HEAD
        git -C "$IDF_DIR" submodule update --init --recursive --depth 1
    else
        log "Cloning ESP-IDF $IDF_VERSION into $IDF_DIR"
        mkdir -p "$(dirname "$IDF_DIR")"
        git clone --depth 1 --branch "$IDF_VERSION" --recursive --shallow-submodules \
            https://github.com/espressif/esp-idf.git "$IDF_DIR"
    fi
    log "Installing ESP-IDF tools for $TARGET"
    "$IDF_DIR/install.sh" "$TARGET"
}

load_idf() {
    [ -f "$IDF_DIR/export.sh" ] || die "ESP-IDF not found at $IDF_DIR. Run: ./setup.sh install"
    # shellcheck disable=SC1091
    . "$IDF_DIR/export.sh" >/dev/null
    cd "$PROJECT_DIR"
}

resolve_port() {
    case "${1:-}" in
        "") ;;
        usb[0-9]*) echo "/dev/ttyUSB${1#usb}" ;;
        acm[0-9]*) echo "/dev/ttyACM${1#acm}" ;;
        *) echo "$1" ;;
    esac
}

port_args() {
    local port
    port="$(resolve_port "${1:-}")"
    if [ -n "$port" ]; then
        echo "-p $port"
    fi
}

find_port() {
    local port
    port="$(resolve_port "${1:-}")"
    if [ -z "$port" ]; then
        port="$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -n 1 || true)"
        [ -n "$port" ] || die "No serial port found. Is the board plugged in with a data cable?"
    fi
    if command -v fuser >/dev/null && fuser "$port" >/dev/null 2>&1; then
        die "$port is in use by another program (an open monitor?). Close it first, e.g.: kill $(fuser "$port" 2>/dev/null)"
    fi
    echo "$port"
}

talk_session() {
    local port
    port="$(find_port "${1:-}")"
    # the script goes in via -c so stdin stays connected to the keyboard for input()
    local script
    script="$(cat <<'PY'
import re, sys, time, serial

MARKER = b"Prompt> "
# ESP-IDF log lines (optionally colored), e.g. "I (397) LLM: ..."
LOG_LINE = re.compile(rb"^(\x1b\[[0-9;]*m)?[IWED] \(\d+\)")

def wait_for(s, marker, timeout):
    """Read until marker appears and return whatever came after it."""
    buf, start = b"", time.time()
    while marker not in buf:
        if time.time() - start > timeout:
            sys.exit("\nNo response from the board. Is the talk-enabled firmware flashed? (./setup.sh build && ./setup.sh flash)")
        buf += s.read(256)
    return buf.split(marker, 1)[1]

def read_until_marker(s, buf=b""):
    """Stream the board's text, minus log lines, until it asks for the next line."""
    shown = 0
    while True:
        # print finished lines that aren't log output, and the unfinished tail as it streams in
        while True:
            line_start = buf.rfind(b"\n", 0, shown) + 1
            nl = buf.find(b"\n", shown)
            line = buf[line_start:] if nl == -1 else buf[line_start:nl + 1]
            is_log = LOG_LINE.match(line) or line.startswith(b"\x1b") or (
                nl == -1 and (MARKER.startswith(line.strip(b"\r")) or re.match(rb"^[IWED] ?\(?\d*$", line)))
            if nl == -1:
                if not is_log and len(buf) > shown:
                    sys.stdout.write(buf[shown:].decode(errors="replace").replace("\r", ""))
                    shown = len(buf)
                break
            if not is_log:
                text = buf[shown:nl + 1].decode(errors="replace").replace("\r", "")
                if text.startswith("achieved tok/s"):
                    text = "\033[2m(" + text.strip() + ")\033[0m\n"
                sys.stdout.write(text)
            shown = nl + 1
        sys.stdout.flush()
        if MARKER in buf:
            return
        buf += s.read(256)

with serial.Serial(port := sys.argv[1], 115200, timeout=0.1) as s:
    print(f"Connecting to the board on {port}...\n")
    s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False  # reset so we start clean
    read_until_marker(s, wait_for(s, b"### READY", timeout=20).lstrip(b"\r\n"))
    print("\033[2m(Type your reply and press Enter. Story model: an empty line gives a random story;"
          " officer model: an empty line starts a new interrogation. Ctrl+C to quit.)\033[0m")
    try:
        while True:
            line = input("\n\033[1mYou:\033[0m ")
            s.write(line.encode() + b"\r")
            read_until_marker(s)
    except (KeyboardInterrupt, EOFError):
        print()
PY
)"
    python -c "$script" "$port"
}

capture_log() {
    local port secs logfile
    port="$(find_port "${1:-}")"
    secs="${2:-60}"
    mkdir -p "$PROJECT_DIR/logs"
    logfile="$PROJECT_DIR/logs/serial-$(date +%Y%m%d-%H%M%S).log"
    log "Capturing $port for ${secs}s to $logfile"
    python - "$port" "$secs" "$logfile" <<'PY'
import sys, time, serial
port, secs, path = sys.argv[1], float(sys.argv[2]), sys.argv[3]
with serial.Serial(port, 115200, timeout=0.2) as s, open(path, "wb") as f:
    # pulse EN via RTS so the log starts from boot
    s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
    end = time.time() + secs
    try:
        while time.time() < end:
            data = s.read(4096)
            if data:
                f.write(data); f.flush()
                sys.stdout.write(data.decode(errors="replace")); sys.stdout.flush()
    except KeyboardInterrupt:
        pass
PY
    log "Saved log to $logfile"
}

cmd="${1:-}"
shift || true
case "$cmd" in
    install)
        install_system_packages
        install_idf
        log "Done. Next: ./setup.sh build"
        ;;
    build)
        load_idf
        # On a fresh checkout the component manager reports the git-sourced u8g2
        # component as "corrupted" (hash mismatch with dependencies.lock); the
        # second run uses the downloaded copy and succeeds.
        idf.py build || { log "Retrying build once"; idf.py build; }
        ;;
    flash)
        load_idf
        # shellcheck disable=SC2046
        idf.py $(port_args "${1:-}") flash
        ;;
    monitor)
        load_idf
        # shellcheck disable=SC2046
        idf.py $(port_args "${1:-}") monitor
        ;;
    talk)
        load_idf
        talk_session "${1:-}"
        ;;
    log)
        load_idf
        capture_log "${1:-}" "${2:-}"
        ;;
    run)
        load_idf
        # shellcheck disable=SC2046
        idf.py $(port_args "${1:-}") build flash monitor
        ;;
    *)
        sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
        exit 1
        ;;
esac
