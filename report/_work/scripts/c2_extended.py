#!/usr/bin/env python3
"""Inject extra C2 checks into an already grading-injected /tmp copy."""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
if not root.is_relative_to(Path('/tmp')):
    raise SystemExit('Use an independent copy under /tmp')
p = root / 'kern/test/test_lab3.c'
s = p.read_text()
extra = r'''
static void
test_c2_extended(void)
{
    uint16_t encodings[][2] = {
        {0x0000, 0x0000}, {0x007b, 0x0000},
        {0x9002, 0x0000}, {0x0073, 0x0010}
    };
    const uintptr_t causes[] = {
        CAUSE_ILLEGAL_INSTRUCTION, CAUSE_ILLEGAL_INSTRUCTION,
        CAUSE_BREAKPOINT, CAUSE_BREAKPOINT
    };
    const unsigned int lengths[] = {2, 4, 2, 4};
    struct trapframe tf;
    unsigned int i;

    for (i = 0; i < 4; i++) {
        memset(&tf, 0, sizeof(tf));
        tf.cause = causes[i];
        tf.epc = (uintptr_t)encodings[i];
        exception_handler(&tf);
        if (tf.epc != (uintptr_t)encodings[i] + lengths[i]) {
            cprintf("[C2 extra] EPC case %d: FAIL\n", i);
            return;
        }
        cprintf("[C2 extra] EPC case %d: PASS (+%d)\n", i, lengths[i]);
    }

    memset(&tf, 0, sizeof(tf));
    tf.cause = 20;
    tf.epc = 0x1234;
    exception_handler(&tf);
    if (tf.epc != 0x1234) {
        cprintf("[C2 extra] fallback EPC: FAIL\n");
        return;
    }
    cprintf("[C2 extra] fallback EPC: PASS (unchanged)\n");

    asm volatile(".option push\n.option norvc\n.2byte 0x0000\n.option pop" ::: "memory");
    cprintf("[C2 extra] real illegal16 return: PASS\n");
    asm volatile(".option push\n.option norvc\n.4byte 0x0000007b\n.option pop" ::: "memory");
    cprintf("[C2 extra] real illegal32 return: PASS\n");
    asm volatile(".option push\n.option norvc\n.2byte 0x9002\n.option pop" ::: "memory");
    cprintf("[C2 extra] real breakpoint16 return: PASS\n");
    asm volatile(".option push\n.option norvc\n.4byte 0x00100073\n.option pop" ::: "memory");
    cprintf("[C2 extra] real breakpoint32 return: PASS\n");
    cprintf("[C2 extra] ALL PASS\n");
}

'''
anchor = 'void\ntest_lab3_exception_context(void)'
assert s.count(anchor) == 1
s = s.replace(anchor, extra + anchor)
anchor = '    test_lab3_handler_fallbacks();'
assert s.count(anchor) == 1
p.write_text(s.replace(anchor, anchor + '\n    test_c2_extended();'))
