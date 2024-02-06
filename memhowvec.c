/*
 * Copyright (C) 2019, Alex Bennée <alex.bennee@linaro.org>
 *
 * How vectorised is this code?
 *
 * Attempt to measure the amount of vectorisation that has been done
 * on some code by counting classes of instruction.
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

typedef enum {
    COUNT_CLASS,
    COUNT_INDIVIDUAL,
    COUNT_NONE
} CountType;

static int limit = 50;
static bool do_inline;
static bool verbose;

static GMutex lock;
static GHashTable *insns;

typedef enum {
    VM_kern, 
    VM_user, 
    VM_max
} VMMap; 

#define  USERSPACE ((uint64_t)0x0000ffffffffffff)
#define  MEM_CLASSIFY(x)  (((x) > USERSPACE ) ? VM_kern : VM_user)

typedef struct {
    const char *class;
    const char *opt;
    uint32_t mask;
    uint32_t pattern;
    CountType what;
    uint64_t count[VM_max];
} InsnClassExecCount;

typedef struct {
    char *insn;
    uint32_t opcode;
    uint64_t count[VM_max];
    InsnClassExecCount *class;
} InsnExecCount;

/*
 * Matchers for classes of instructions, order is important.
 *
 * Your most precise match must be before looser matches. If no match
 * is found in the table we can create an individual entry.
 *
 * 31..28 27..24 23..20 19..16 15..12 11..8 7..4 3..0
 */
static InsnClassExecCount aarch64_insn_classes[] = {
    /* "Reserved"" */
    { "  UDEF",              "udef",   0xffff0000, 0x00000000, COUNT_NONE, {0,}},
    { "  SVE",               "sve",    0x1e000000, 0x04000000, COUNT_CLASS, {0,}},
    { "Reserved",            "res",    0x1e000000, 0x00000000, COUNT_CLASS, {0,}},
    /* Data Processing Immediate */
    { "  PCrel addr",        "pcrel",  0x1f000000, 0x10000000, COUNT_CLASS, {0,}},
    { "  Add/Sub (imm,tags)","asit",   0x1f800000, 0x11800000, COUNT_CLASS, {0,}},
    { "  Add/Sub (imm)",     "asi",    0x1f000000, 0x11000000, COUNT_CLASS, {0,}},
    { "  Logical (imm)",     "logi",   0x1f800000, 0x12000000, COUNT_CLASS, {0,}},
    { "  Move Wide (imm)",   "movwi",  0x1f800000, 0x12800000, COUNT_CLASS, {0,}},
    { "  Bitfield",          "bitf",   0x1f800000, 0x13000000, COUNT_CLASS, {0,}},
    { "  Extract",           "extr",   0x1f800000, 0x13800000, COUNT_CLASS, {0,}},
    { "Data Proc Imm",       "dpri",   0x1c000000, 0x10000000, COUNT_CLASS, {0,}},
    /* Branches */
    { "  Cond Branch (imm)", "cndb",   0xfe000000, 0x54000000, COUNT_CLASS, {0,}},
    { "  Exception Gen",     "excp",   0xff000000, 0xd4000000, COUNT_CLASS, {0,}},
    { "    NOP",             "nop",    0xffffffff, 0xd503201f, COUNT_NONE, {0,}},
    { "  Hints",             "hint",   0xfffff000, 0xd5032000, COUNT_CLASS, {0,}},
    { "  Barriers",          "barr",   0xfffff000, 0xd5033000, COUNT_CLASS, {0,}},
    { "  PSTATE",            "psta",   0xfff8f000, 0xd5004000, COUNT_CLASS, {0,}},
    { "  System Insn",       "sins",   0xffd80000, 0xd5080000, COUNT_CLASS, {0,}},
    { "  System Reg",        "sreg",   0xffd00000, 0xd5100000, COUNT_CLASS, {0,}},
    { "  Branch (reg)",      "breg",   0xfe000000, 0xd6000000, COUNT_CLASS, {0,}},
    { "  Branch (imm)",      "bimm",   0x7c000000, 0x14000000, COUNT_CLASS, {0,}},
    { "  Cmp & Branch",      "cmpb",   0x7e000000, 0x34000000, COUNT_CLASS, {0,}},
    { "  Tst & Branch",      "tstb",   0x7e000000, 0x36000000, COUNT_CLASS, {0,}},
    { "Branches",            "branch", 0x1c000000, 0x14000000, COUNT_CLASS, {0,}},
    /* Loads and Stores */
    { "  AdvSimd ldstmult",  "advlsm", 0xbfbf0000, 0x0c000000, COUNT_CLASS, {0,}},
    { "  AdvSimd ldstmult++","advlsmp",0xbfb00000, 0x0c800000, COUNT_CLASS, {0,}},
    { "  AdvSimd ldst",      "advlss", 0xbf9f0000, 0x0d000000, COUNT_CLASS, {0,}},
    { "  AdvSimd ldst++",    "advlssp",0xbf800000, 0x0d800000, COUNT_CLASS, {0,}},
    { "  ldst excl",         "ldstx",  0x3f000000, 0x08000000, COUNT_CLASS, {0,}},
    { "    Prefetch",        "prfm",   0xff000000, 0xd8000000, COUNT_CLASS, {0,}},
    { "  Load Reg (lit)",    "ldlit",  0x1b000000, 0x18000000, COUNT_CLASS, {0,}},
    { "  ldst noalloc pair", "ldstnap",0x3b800000, 0x28000000, COUNT_CLASS, {0,}},
    { "  ldst pair",         "ldstp",  0x38000000, 0x28000000, COUNT_CLASS, {0,}},
    { "  ldst reg",          "ldstr",  0x3b200000, 0x38000000, COUNT_CLASS, {0,}},
    { "  Atomic ldst",       "atomic", 0x3b200c00, 0x38200000, COUNT_CLASS, {0,}},
    { "  ldst reg (reg off)","ldstro", 0x3b200b00, 0x38200800, COUNT_CLASS, {0,}},
    { "  ldst reg (pac)",    "ldstpa", 0x3b200200, 0x38200800, COUNT_CLASS, {0,}},
    { "  ldst reg (imm)",    "ldsti",  0x3b000000, 0x39000000, COUNT_CLASS, {0,}},
    { "Loads & Stores",      "ldst",   0x0a000000, 0x08000000, COUNT_CLASS, {0,}},
    /* Data Processing Register */
    { "Data Proc Reg",       "dprr",   0x0e000000, 0x0a000000, COUNT_CLASS, {0,}},
    /* Scalar FP */
    { "Scalar FP ",          "fpsimd", 0x0e000000, 0x0e000000, COUNT_CLASS, {0,}},
    /* Unclassified */
    { "Unclassified",        "unclas", 0x00000000, 0x00000000, COUNT_CLASS, {0,}},
};



static InsnClassExecCount morello_insn_classes[] = {
    /* "Reserved"" */
    { "  UDEF",              "udef",   0xffff0000, 0x00000000, COUNT_NONE, {0,}},
    { "morello add/sub cap", "mor1",   0xff000000, 0x02000000, COUNT_CLASS, {0,}},
    { "morello ld/st misc1", "mor2",   0xff000000, 0x22000000, COUNT_CLASS, {0,}},
    { "morello ld/st misc2", "mor3",   0xff000000, 0x42000000, COUNT_CLASS, {0,}},
    { "morello ld/st misc3", "mor4",   0xff000000, 0x62000000, COUNT_CLASS, {0,}},
    { "morello ldr literal", "mor5",   0xffc00000, 0x82000000, COUNT_CLASS, {0,}},
    { "morello ld/st uovab", "mor6",   0xffc00000, 0x82400000, COUNT_CLASS, {0,}},
    { "morello ld/st rvab",  "mor7",   0xff800000, 0x82800000, COUNT_CLASS, {0,}},
    { "morello ld/st misc4", "mor8",   0xff000000, 0xa2000000, COUNT_CLASS, {0,}},
    { "morello ld/st uo",    "mor9",   0xff800000, 0xc2000000, COUNT_CLASS, {0,}},
    { "morello sysreg",      "mor10",  0xffe00000, 0xc2800000, COUNT_CLASS, {0,}},
    { "morello add extreg",  "mor11",  0xffe00000, 0xc2a00000, COUNT_CLASS, {0,}},
    { "morello misc",        "mor12",  0xffc00000, 0xc2c00000, COUNT_CLASS, {0,}},
    { "morello ld/st uivab", "mor13",  0xff000000, 0xe2000000, COUNT_CLASS, {0,}},
    { "morello NOCLASS",     "morXX",  0x1f000000, 0x02000000, COUNT_CLASS, {0,}},
    { "  SVE",               "sve",    0x1e000000, 0x04000000, COUNT_CLASS, {0,}},
    { "Reserved",            "res",    0x1e000000, 0x00000000, COUNT_CLASS, {0,}},
    /* Data Processing Immediate */
    { "  PCrel addr",        "pcrel",  0x1f000000, 0x10000000, COUNT_CLASS, {0,}},
    { "  Add/Sub (imm,tags)","asit",   0x1f800000, 0x11800000, COUNT_CLASS, {0,}},
    { "  Add/Sub (imm)",     "asi",    0x1f000000, 0x11000000, COUNT_CLASS, {0,}},
    { "  Logical (imm)",     "logi",   0x1f800000, 0x12000000, COUNT_CLASS, {0,}},
    { "  Move Wide (imm)",   "movwi",  0x1f800000, 0x12800000, COUNT_CLASS, {0,}},
    { "  Bitfield",          "bitf",   0x1f800000, 0x13000000, COUNT_CLASS, {0,}},
    { "  Extract",           "extr",   0x1f800000, 0x13800000, COUNT_CLASS, {0,}},
    { "Data Proc Imm",       "dpri",   0x1c000000, 0x10000000, COUNT_CLASS, {0,}},
    /* Branches */
    { "  Cond Branch (imm)", "cndb",   0xfe000000, 0x54000000, COUNT_CLASS, {0,}},
    { "  Exception Gen",     "excp",   0xff000000, 0xd4000000, COUNT_CLASS, {0,}},
    { "    NOP",             "nop",    0xffffffff, 0xd503201f, COUNT_NONE, {0,}},
    { "  Hints",             "hint",   0xfffff000, 0xd5032000, COUNT_CLASS, {0,}},
    { "  Barriers",          "barr",   0xfffff000, 0xd5033000, COUNT_CLASS, {0,}},
    { "  PSTATE",            "psta",   0xfff8f000, 0xd5004000, COUNT_CLASS, {0,}},
    { "  System Insn",       "sins",   0xffd80000, 0xd5080000, COUNT_CLASS, {0,}},
    { "  System Reg",        "sreg",   0xffd00000, 0xd5100000, COUNT_CLASS, {0,}},
    { "  Branch (reg)",      "breg",   0xfe000000, 0xd6000000, COUNT_CLASS, {0,}},
    { "  Branch (imm)",      "bimm",   0x7c000000, 0x14000000, COUNT_CLASS, {0,}},
    { "  Cmp & Branch",      "cmpb",   0x7e000000, 0x34000000, COUNT_CLASS, {0,}},
    { "  Tst & Branch",      "tstb",   0x7e000000, 0x36000000, COUNT_CLASS, {0,}},
    { "Branches",            "branch", 0x1c000000, 0x14000000, COUNT_CLASS, {0,}},
    /* Loads and Stores */
    { "  AdvSimd ldstmult",  "advlsm", 0xbfbf0000, 0x0c000000, COUNT_CLASS, {0,}},
    { "  AdvSimd ldstmult++","advlsmp",0xbfb00000, 0x0c800000, COUNT_CLASS, {0,}},
    { "  AdvSimd ldst",      "advlss", 0xbf9f0000, 0x0d000000, COUNT_CLASS, {0,}},
    { "  AdvSimd ldst++",    "advlssp",0xbf800000, 0x0d800000, COUNT_CLASS, {0,}},
    { "  ldst excl",         "ldstx",  0x3f000000, 0x08000000, COUNT_CLASS, {0,}},
    { "    Prefetch",        "prfm",   0xff000000, 0xd8000000, COUNT_CLASS, {0,}},
    { "  Load Reg (lit)",    "ldlit",  0x1b000000, 0x18000000, COUNT_CLASS, {0,}},
    { "  ldst noalloc pair", "ldstnap",0x3b800000, 0x28000000, COUNT_CLASS, {0,}},
    { "  ldst pair",         "ldstp",  0x38000000, 0x28000000, COUNT_CLASS, {0,}},
    { "  ldst reg",          "ldstr",  0x3b200000, 0x38000000, COUNT_CLASS, {0,}},
    { "  Atomic ldst",       "atomic", 0x3b200c00, 0x38200000, COUNT_CLASS, {0,}},
    { "  ldst reg (reg off)","ldstro", 0x3b200b00, 0x38200800, COUNT_CLASS, {0,}},
    { "  ldst reg (pac)",    "ldstpa", 0x3b200200, 0x38200800, COUNT_CLASS, {0,}},
    { "  ldst reg (imm)",    "ldsti",  0x3b000000, 0x39000000, COUNT_CLASS, {0,}},
    { "Loads & Stores",      "ldst",   0x0a000000, 0x08000000, COUNT_CLASS, {0,}},
    /* Data Processing Register */
    { "Data Proc Reg",       "dprr",   0x0e000000, 0x0a000000, COUNT_CLASS, {0,}},
    /* Scalar FP */
    { "Scalar FP ",          "fpsimd", 0x0e000000, 0x0e000000, COUNT_CLASS, {0,}},
    /* Unclassified */
    { "Unclassified",        "unclas", 0x00000000, 0x00000000, COUNT_CLASS, {0,}},
};

static InsnClassExecCount sparc32_insn_classes[] = {
    { "Call",                "call",   0xc0000000, 0x40000000, COUNT_CLASS, {0,}},
    { "Branch ICond",        "bcc",    0xc1c00000, 0x00800000, COUNT_CLASS, {0,}},
    { "Branch Fcond",        "fbcc",   0xc1c00000, 0x01800000, COUNT_CLASS, {0,}},
    { "SetHi",               "sethi",  0xc1c00000, 0x01000000, COUNT_CLASS, {0,}},
    { "FPU ALU",             "fpu",    0xc1f00000, 0x81a00000, COUNT_CLASS, {0,}},
    { "ALU",                 "alu",    0xc0000000, 0x80000000, COUNT_CLASS, {0,}},
    { "Load/Store",          "ldst",   0xc0000000, 0xc0000000, COUNT_CLASS, {0,}},
    /* Unclassified */
    { "Unclassified",        "unclas", 0x00000000, 0x00000000, COUNT_INDIVIDUAL, {0,}},
};

static InsnClassExecCount sparc64_insn_classes[] = {
    { "SetHi & Branches",     "op0",   0xc0000000, 0x00000000, COUNT_CLASS, {0,}},
    { "Call",                 "op1",   0xc0000000, 0x40000000, COUNT_CLASS, {0,}},
    { "Arith/Logical/Move",   "op2",   0xc0000000, 0x80000000, COUNT_CLASS, {0,}},
    { "Arith/Logical/Move",   "op3",   0xc0000000, 0xc0000000, COUNT_CLASS, {0,}},
    /* Unclassified */
    { "Unclassified",        "unclas", 0x00000000, 0x00000000, COUNT_INDIVIDUAL, {0,}},
};

/* Default matcher for currently unclassified architectures */
static InsnClassExecCount default_insn_classes[] = {
    { "Unclassified",        "unclas", 0x00000000, 0x00000000, COUNT_INDIVIDUAL, {0,}},
};


typedef struct {
    const char *qemu_target;
    InsnClassExecCount *table;
    int table_sz;
} ClassSelector;

static ClassSelector class_tables[] =
{
    { "aarch64", aarch64_insn_classes, ARRAY_SIZE(aarch64_insn_classes) },
    { "morello", morello_insn_classes, ARRAY_SIZE(morello_insn_classes) },    
    { "sparc",   sparc32_insn_classes, ARRAY_SIZE(sparc32_insn_classes) },
    { "sparc64", sparc64_insn_classes, ARRAY_SIZE(sparc64_insn_classes) },
    { NULL, default_insn_classes, ARRAY_SIZE(default_insn_classes) },
};

static InsnClassExecCount *class_table;
static int class_table_sz;

static gint cmp_exec_count(gconstpointer a, gconstpointer b)
{
    InsnExecCount *ea = (InsnExecCount *) a;
    InsnExecCount *eb = (InsnExecCount *) b;
    //return ea->count > eb->count ? -1 : 1;   @dejice.jacob original
    return ea->count[VM_user] > eb->count[VM_user] ? -1 : 1;
}

static void free_record(gpointer data)
{
    InsnExecCount *rec = (InsnExecCount *) data;
    g_free(rec->insn);
    g_free(rec);
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_autoptr(GString) report = g_string_new("Instruction Classes:\n");
    int i;
    GList *counts;
    InsnClassExecCount *class = NULL;

    for (i = 0; i < class_table_sz; i++) {
        class = &class_table[i];
        switch (class->what) {
        case COUNT_CLASS:
            // if (class->count || verbose) {   @dejice.jacob - original
            if (class->count[VM_user] || class->count[VM_kern] || verbose) {
                g_string_append_printf(report, "Class: %-24s\t(hits: user - %ld, kern - %ld)\n",
                                       class->class,
                                       class->count[VM_user], 
                                       class->count[VM_kern]);
            }
            break;
        case COUNT_INDIVIDUAL:
            g_string_append_printf(report, "Class: %-24s\tcounted individually\n",
                                   class->class);
            break;
        case COUNT_NONE:
            g_string_append_printf(report, "Class: %-24s\tnot counted\n",
                                   class->class);
            break;
        default:
            break;
        }
    }

    counts = g_hash_table_get_values(insns);
    if (counts && g_list_next(counts)) {
        g_string_append_printf(report,"Individual Instructions:\n");
        counts = g_list_sort(counts, cmp_exec_count);

        for (i = 0; i < limit && g_list_next(counts);
             i++, counts = g_list_next(counts)) {
            InsnExecCount *rec = (InsnExecCount *) counts->data;
            g_string_append_printf(report,
                                   "Instr: %-24s\t(hits: user - %ld, kern -%ld)\t(op=%#08x/%s)\n",
                                   rec->insn,
                                   rec->count[VM_user], rec->count[VM_kern],
                                   rec->opcode,
                                   rec->class ?
                                   rec->class->class : "un-categorised");
        }
        g_list_free(counts);
    }

    g_hash_table_destroy(insns);

    qemu_plugin_outs(report->str);
}

static void plugin_init(void)
{
    insns = g_hash_table_new_full(NULL, g_direct_equal, NULL, &free_record);
}

static void vcpu_insn_exec_before(unsigned int cpu_index, void *udata)
{
    uint64_t *count = (uint64_t *) udata;
    (*count)++;
}

static uint64_t * find_counter(struct qemu_plugin_insn *insn)
{
    int i;
    uint64_t *cnt = NULL;
    uint32_t opcode;
    InsnClassExecCount *class = NULL;

    /*
     * We only match the first 32 bits of the instruction which is
     * fine for most RISCs but a bit limiting for CISC architectures.
     * They would probably benefit from a more tailored plugin.
     * However we can fall back to individual instruction counting.
     */
    opcode = *((uint32_t *)qemu_plugin_insn_data(insn));

    for (i = 0; !cnt && i < class_table_sz; i++) {
        class = &class_table[i];
        uint32_t masked_bits = opcode & class->mask;
        if (masked_bits == class->pattern) {
            break;
        }
    }

    g_assert(class);

    switch (class->what) {
    case COUNT_NONE:
        return NULL;
    case COUNT_CLASS:
        //return &class->count;  @dejice.jacob commented out
        return class->count;
    case COUNT_INDIVIDUAL:
    {
        InsnExecCount *icount;

        g_mutex_lock(&lock);
        icount = (InsnExecCount *) g_hash_table_lookup(insns,
                                                       GUINT_TO_POINTER(opcode));

        if (!icount) {
            icount = g_new0(InsnExecCount, 1);
            icount->opcode = opcode;
            icount->insn = qemu_plugin_insn_disas(insn);
            icount->class = class;

            g_hash_table_insert(insns, GUINT_TO_POINTER(opcode),
                                (gpointer) icount);
        }
        g_mutex_unlock(&lock);

        //return &icount->count;  @dejice.jacob commented out
        return icount->count;
    }
    default:
        g_assert_not_reached();
    }

    return NULL;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++) {
        uint64_t *cnt;
        uint64_t vaddr;   // @dejice.jacob added 
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        cnt = find_counter(insn);
        vaddr = qemu_plugin_insn_vaddr(insn);  // @dejice.jacob added 

        if (cnt) {
            if (do_inline) {
                qemu_plugin_register_vcpu_insn_exec_inline(
                    //insn, QEMU_PLUGIN_INLINE_ADD_U64, cnt, 1);   @dejice.jacob  commented orig
                    insn, QEMU_PLUGIN_INLINE_ADD_U64, &cnt[MEM_CLASSIFY(vaddr)], 1);
            } else {
                qemu_plugin_register_vcpu_insn_exec_cb(
                    //insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS, cnt);
                    insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS, &cnt[MEM_CLASSIFY(vaddr)]);
            }
        }
    }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    int i;

    /* Select a class table appropriate to the guest architecture */
    for (i = 0; i < ARRAY_SIZE(class_tables); i++) {
        ClassSelector *entry = &class_tables[i];
        if (!entry->qemu_target ||
            strcmp(entry->qemu_target, info->target_name) == 0) {
            class_table = entry->table;
            class_table_sz = entry->table_sz;
            break;
        }
    }

    for (i = 0; i < argc; i++) {
        char *p = argv[i];
        if (strcmp(p, "inline") == 0) {
            do_inline = true;
        } else if (strcmp(p, "verbose") == 0) {
            verbose = true;
        } else {
            int j;
            CountType type = COUNT_INDIVIDUAL;
            if (*p == '!') {
                type = COUNT_NONE;
                p++;
            }
            for (j = 0; j < class_table_sz; j++) {
                if (strcmp(p, class_table[j].opt) == 0) {
                    class_table[j].what = type;
                    break;
                }
            }
        }
    }

    plugin_init();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
