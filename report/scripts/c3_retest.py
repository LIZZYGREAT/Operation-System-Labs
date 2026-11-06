#!/usr/bin/env python3
"""Rebuild a fixed Git baseline in /tmp and independently verify B's timer."""
import argparse
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import time

BASELINE = '222162ce44b2cd84a9fa854a192a006c618ee7c1'
REPO = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', default=BASELINE)
parser.add_argument('--output', type=Path, default=REPO / 'report/logs')
args = parser.parse_args()
logs = args.output.resolve()
logs.mkdir(parents=True, exist_ok=True)
baseline = subprocess.check_output(['git', 'rev-parse', args.baseline + '^{commit}'], cwd=REPO, text=True).strip()
workspace = Path(tempfile.mkdtemp(prefix='lab3-c3-'))
archive = subprocess.check_output(['git', 'archive', baseline, 'code'], cwd=REPO)
versions = {}
for program in ('make', 'riscv64-unknown-elf-gcc', 'qemu-system-riscv64', 'python3'):
    versions[program] = subprocess.check_output([program, '--version'], text=True).splitlines()[0]
status = {'baseline': baseline, 'workspace': str(workspace), 'versions': versions, 'cases': {}}


def save_status():
    (logs / 'c3-status.json').write_text(json.dumps(status, indent=2) + '\n')


def run(command, cwd, log):
    start = time.monotonic()
    with log.open('w') as out:
        result = subprocess.run(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT)
    return {'command': command, 'cwd': str(cwd), 'exit_code': result.returncode,
            'seconds': round(time.monotonic() - start, 3), 'log': log.name}


for name in ('timer', 'normal'):
    case_dir = workspace / name
    case_dir.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        source.extractall(case_dir, filter='data')
    code = case_dir / 'code'
    case = status['cases'][name] = {'steps': []}
    commands = []
    if name == 'timer':
        commands.append(('inject', ['python3', 'tools/inject.py', str(code)]))
    commands.extend([
        ('clean', ['make', 'clean']),
        ('build', ['make', 'V=', 'DEFS=-Dsbadaddr=stval' + (' -DGRADE_LAB3_TIMER' if name == 'timer' else '')]),
    ])
    for label, command in commands:
        result = run(command, code, logs / ('c3-' + name + '-' + label + '.log'))
        case['steps'].append(result)
        save_status()
        if result['exit_code']:
            raise SystemExit(name + ' ' + label + ' failed; see logs')
    serial = logs / ('c3-' + name + '-qemu.log')
    command = ['timeout', '20s', 'qemu-system-riscv64', '-machine', 'virt', '-nographic',
               '-bios', 'default', '-device', 'loader,file=bin/ucore.img,addr=0x80200000',
               '-serial', 'file:' + str(serial), '-monitor', 'none', '-no-reboot']
    result = run(command, code, logs / ('c3-' + name + '-qemu-stderr.log'))
    case['steps'].append(result)
    # Preserve all text, normalizing only serial CRLF line endings for Git.
    serial.write_bytes(serial.read_bytes().replace(b'\r\n', b'\n'))
    output = serial.read_text()
    lines = output.splitlines()
    marker = '[Test 3] Sustained supervisor timer: PASS (40/40, ticks=200)'
    case['tick_output_count'] = lines.count('100 ticks')
    case['timer_pass_count'] = lines.count(marker)
    case['checks'] = {
        'normal_exit_before_timeout': result['exit_code'] == 0 and result['seconds'] < 20,
        'exactly_ten_tick_lines': case['tick_output_count'] == 10,
        'no_fail_or_panic': re.search(r'FAIL|panic', output, re.IGNORECASE) is None,
        'memory_self_check': 'check_alloc_page() succeeded!' in output,
        'timer_initialized': '++ setup timer interrupts' in output,
        'expected_timer_marker': case['timer_pass_count'] == (1 if name == 'timer' else 0),
    }
    case['passed'] = all(case['checks'].values())
    save_status()
    print(name + ': ' + ('PASS' if case['passed'] else 'FAIL'), flush=True)

if not all(case['passed'] for case in status['cases'].values()):
    raise SystemExit('C3 failed; see c3-status.json')
print('C3 acceptance: PASS', flush=True)
