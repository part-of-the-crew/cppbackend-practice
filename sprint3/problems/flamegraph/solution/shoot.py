import argparse
import subprocess
import time
import random
import shlex
import os
from pathlib import Path

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server

def stop(process, wait=False):
    if process.poll() is None:
        process.terminate() # Sends SIGTERM
        if wait:
            process.wait()

def shoot(ammo):
    # Use a list for command to avoid shell injection issues
    hit = subprocess.Popen(['curl', '-s', ammo], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)

def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')

def del_file(file_path):
    f = Path(file_path)
    if f.exists():
        f.unlink()

def run_proc(args, stdout=None, stdin=None, stderr=subprocess.DEVNULL):
    return subprocess.Popen(args, stdout=stdout, stdin=stdin, stderr=stderr)

# --- EXECUTION ---

RESULT_FILE = "graph.svg"
PERF_DATA = "perf.data"

del_file(RESULT_FILE)
del_file(PERF_DATA)

# Fix: Flatten the command list correctly
server_cmd = shlex.split(start_server())
perf_cmd = ["perf", "record", "-g", "-o", PERF_DATA] + server_cmd

print(f"Starting server with: {' '.join(perf_cmd)}")
server = run_proc(perf_cmd)

time.sleep(1.0) # Give server time to bind to port
make_shots()
stop(server, wait=True)
time.sleep(0.5)

if not os.path.exists(PERF_DATA):
    raise RuntimeError("perf.data was not created. Check if 'perf' is installed and has permissions.")

# 1. Get raw script output
p1 = run_proc(["perf", "script", "-i", PERF_DATA], stdout=subprocess.PIPE)

# 2. Filter library noise
p1_5 = run_proc(["grep", "-vE", "boost|asio|beast|std::|__libc|pthread"], 
                stdin=p1.stdout, stdout=subprocess.PIPE)

# 3. Collapse stacks
# Ensure the path to the perl script is correct relative to where you run this
p2 = run_proc(["./FlameGraph/stackcollapse-perf.pl"], 
                stdin=p1_5.stdout, stdout=subprocess.PIPE)

# 4. Generate SVG
with open(RESULT_FILE, "wb") as out_file:
    p3 = run_proc(["./FlameGraph/flamegraph.pl"], 
                    stdin=p2.stdout, stdout=out_file)

    # Close pipe handles to avoid deadlocks
    p1.stdout.close()
    p1_5.stdout.close()
    p2.stdout.close()

    # Wait for completion and check codes
    for p in (p1, p1_5, p2, p3):
        code = p.wait()
        if code not in (0, 141): # 141 is SIGPIPE, common in grep/head chains
            print(f"Warning: {p.args} exited with code {code}")

if os.path.exists(RESULT_FILE):
    print(f"Success! Flamegraph written to {RESULT_FILE}")
else:
    print("Failed to generate Flamegraph.")