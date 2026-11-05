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
REPO = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', default=BASELINE)
parser.add_argument('--output', type=Path, default=REPO / 'report/_work/logs')
parser.add_argument('--images', type=Path, default=REPO / 'report/images')
args = parser.parse_args()
logs = args.output.resolve()
logs.mkdir(parents=True, exist_ok=True)
args.images.mkdir(parents=True, exist_ok=True)
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

# These are clearly labeled log excerpts, not terminal screenshots.
from PIL import Image, ImageDraw, ImageFont
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 20)
for name, case in status['cases'].items():
    lines = (logs / ('c3-' + name + '-qemu.log')).read_text().splitlines()
    selected = [line for line in lines if line == '100 ticks' or '[Test 3]' in line
                or 'check_alloc_page() succeeded!' in line or '++ setup timer interrupts' in line]
    qemu = case['steps'][-1]
    text = ['C3 ' + name + ' - actual log excerpt (not a screenshot)',
            'Baseline: ' + baseline, 'Source: c3-' + name + '-qemu.log', ''] + selected + [
            '', 'Recorded process exit: ' + str(qemu['exit_code']),
            'Elapsed seconds: ' + str(qemu['seconds']),
            'Exact "100 ticks" lines: ' + str(case['tick_output_count']),
            'All acceptance checks: ' + ('PASS' if case['passed'] else 'FAIL')]
    width = int(max(font.getlength(line) for line in text)) + 48
    image = Image.new('RGB', (width, len(text) * 30 + 40), '#f7f8fa')
    draw = ImageDraw.Draw(image)
    for i, line in enumerate(text):
        draw.text((24, 20 + 30 * i), line, font=font, fill='#172b4d')
    image.save(args.images / ('c3_' + name + '_log_excerpt.png'))

if not all(case['passed'] for case in status['cases'].values()):
    raise SystemExit('C3 failed; see c3-status.json')
print('C3 acceptance: PASS', flush=True)
