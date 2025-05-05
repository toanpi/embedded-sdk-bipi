#!/bin/bash

# --- Configuration ---
# Path to your ELF file (change if needed)
ELF_FILE="build/sysview_tracing.elf"
# Directory to save trace files (change if needed)
TRACE_DIR="/Users/toanhuynh/Downloads/systemview"
# Program runtime duration to collect trace (seconds)
TRACE_DURATION_S=10
# Command to run OpenOCD (use your board configuration)
# Ensure paths are correct and executable
OPENOCD_CMD=(
    '/Users/toanhuynh/.espressif/tools/openocd-esp32/v0.12.0-esp32-20241016/openocd-esp32/bin/openocd'
    '-s'
    '/Users/toanhuynh/.espressif/tools/openocd-esp32/v0.12.0-esp32-20241016/openocd-esp32/share/openocd/scripts'
    '-f'
    'board/esp32s3-builtin.cfg'
)
# GDB executable name for ESP32-S3 target
GDB_EXECUTABLE='xtensa-esp32s3-elf-gdb'
# Default OpenOCD GDB port
GDB_PORT=3333
# Temporary GDB command file
GDB_SCRIPT_FILE="gdb_commands_temp.txt"
# --- End Configuration ---

# --- PIDs for cleanup ---
OPENOCD_PID=""
GDB_PID=""

# --- Cleanup Function ---
cleanup() {
    echo ">>> Cleaning up..."
    # Stop GDB process if running
    if [[ -n "$GDB_PID" ]] && kill -0 "$GDB_PID" 2>/dev/null; then
        echo "Stopping GDB (PID: $GDB_PID)..."
        kill "$GDB_PID" 2>/dev/null
        sleep 1 # Give it a moment
        if kill -0 "$GDB_PID" 2>/dev/null; then
            echo "Forcing GDB kill..."
            kill -9 "$GDB_PID" 2>/dev/null
        fi
    fi

    # Stop OpenOCD process if running
    if [[ -n "$OPENOCD_PID" ]] && kill -0 "$OPENOCD_PID" 2>/dev/null; then
        echo "Stopping OpenOCD (PID: $OPENOCD_PID)..."
        # Try terminate first, then kill
        kill "$OPENOCD_PID" 2>/dev/null
        sleep 2 # Give it time to terminate
        if kill -0 "$OPENOCD_PID" 2>/dev/null; then
            echo "OpenOCD did not terminate gracefully, forcing kill..."
            kill -9 "$OPENOCD_PID" 2>/dev/null
        fi
        echo "OpenOCD stopped."
    else
        echo "OpenOCD process not found or already stopped."
    fi

    # Remove temporary GDB script
    if [ -f "$GDB_SCRIPT_FILE" ]; then
        echo "Removing temporary GDB script: $GDB_SCRIPT_FILE"
        rm "$GDB_SCRIPT_FILE"
    fi
    echo ">>> Cleanup finished."
}

# --- Trap SIGINT, SIGTERM, ERR, EXIT ---
trap cleanup EXIT INT TERM ERR

# --- Script Start ---
echo "Starting SystemView Trace Script..."

# 1. Check if the ELF file exists
if [ ! -f "$ELF_FILE" ]; then
    echo "Error: ELF file not found at '$ELF_FILE'"
    echo "Please ensure you have built the project and are running the script from the project root directory."
    exit 1
fi
echo "ELF file found: $ELF_FILE"

# 2. Create the trace directory if it doesn't exist
mkdir -p "$TRACE_DIR"
echo "Trace directory ensured: $TRACE_DIR"

# 3. Create timestamp and trace filenames
timestamp=$(date +%Y%m%d_%H%M%S)
trace_file_core0="${TRACE_DIR}/sysview_esp32s3_0_${timestamp}.svdat"
trace_file_core1="${TRACE_DIR}/sysview_esp32s3_1_${timestamp}.svdat"
echo "Trace file Core 0: $trace_file_core0"
echo "Trace file Core 1: $trace_file_core1"

# 4. Start OpenOCD
echo "Starting OpenOCD: ${OPENOCD_CMD[*]}"
"${OPENOCD_CMD[@]}" > openocd.log 2>&1 &
OPENOCD_PID=$!
echo "OpenOCD PID: $OPENOCD_PID. Waiting for startup..."
sleep 3 # Wait for OpenOCD to initialize

# Check if OpenOCD started correctly (basic check)
if ! kill -0 "$OPENOCD_PID" 2>/dev/null; then
    echo "Error: OpenOCD process ($OPENOCD_PID) not found after starting. Check openocd.log."
    exit 1
fi
echo "OpenOCD seems to be running."

# 5. Generate GDB script file
echo "Generating GDB script file: $GDB_SCRIPT_FILE"
cat << EOF > "$GDB_SCRIPT_FILE"
set pagination off
target remote :${GDB_PORT}
mon reset halt
maintenance flush register-cache
b app_main
commands
mon esp sysview start file://${trace_file_core0} file://${trace_file_core1}
c
end
c
EOF

# 6. Start GDB with the script file in the background
echo "Starting GDB with script: $GDB_EXECUTABLE --command=$GDB_SCRIPT_FILE $ELF_FILE"
# Run GDB, redirect its output/error to /dev/null if desired, or to a file
# Running it in the background (&) so the shell script can continue
"$GDB_EXECUTABLE" --command="$GDB_SCRIPT_FILE" "$ELF_FILE" &
GDB_PID=$!
echo "GDB main process PID: $GDB_PID"

# 7. Wait for trace duration
echo "GDB launched. Waiting ${TRACE_DURATION_S} seconds for trace collection..."
sleep "$TRACE_DURATION_S"
echo "Finished waiting ${TRACE_DURATION_S} seconds."

# 8. Stop SystemView tracing using a separate GDB instance
echo "Stopping SystemView tracing..."
# Use --batch-silent to avoid extraneous GDB output here
"$GDB_EXECUTABLE" "$ELF_FILE" \
    -ex "set pagination off" \
    -ex "target remote :${GDB_PORT}" \
    -ex "mon esp sysview stop" \
    -ex "detach" \
    -ex "quit" --batch-silent

echo "SystemView stop command sent."
sleep 1 # Give a moment for the stop command to potentially take effect

# 9. Cleanup is handled by the trap function upon exit

echo ""
echo "--- Finished ---"
echo "Check OpenOCD log file at: openocd.log"
echo "Check GDB log file at: gdb_run.log (if created)"
echo "Trace files saved at:"
echo "  Core 0: $trace_file_core0"
echo "  Core 1: $trace_file_core1"

exit 0