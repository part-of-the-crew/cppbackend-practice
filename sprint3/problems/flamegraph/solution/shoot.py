import argparse
import subprocess
import time
import random
import shlex

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


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    #print (shlex.split(command))
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')



result = "flame.svg"

try:
    #Path(result).unlink()
    if Path(result).is_file():
        Path(result).unlink()
except FileNotFoundError:
    pass




server = run('perf record -g -o perf.data ' + start_server())
time.sleep(0.5)   # let server start
make_shots()
stop(server)
time.sleep(0.5)   # allow perf to flush


# 1. Get the raw script output
p1 = subprocess.Popen(
    ["perf", "script", "-i", "perf.data"],
    stdout=subprocess.PIPE
)

# 2. NEW: Filter out the library noise using grep
# -v: Invert match (exclude)
# -E: Use extended regex for the pipe | symbol
p1_5 = subprocess.Popen(
    ["grep", "-vE", "boost|asio|beast|std::|__libc|pthread"],
    stdin=p1.stdout,
    stdout=subprocess.PIPE
)

# 3. Collapse the remaining "clean" stacks
p2 = subprocess.Popen(
    ["./FlameGraph/stackcollapse-perf.pl"],
    stdin=p1_5.stdout, # Connect to the filter output
    stdout=subprocess.PIPE
)

# 4. Generate the SVG
p3 = subprocess.Popen(
    ["./FlameGraph/flamegraph.pl"],
    stdin=p2.stdout,
    stdout=open(result, "wb")
)

# Clean up pipes to prevent hanging
p1.stdout.close()
p1_5.stdout.close()
p2.stdout.close()

# Wait for all (including the new p1_5)
for p in (p1, p1_5, p2, p3):
    code = p.wait()
    if code not in (0, 141):
        raise RuntimeError(f"{p.args} failed with {code}")

print("Flamegraph written to " + result)        
