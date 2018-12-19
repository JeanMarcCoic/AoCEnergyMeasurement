#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

static inline int atoi_p(char *p, char **end)
{
    int res = 0;
    int sign = 1;

    while (*p == ' ') {
        p++;
    }

    if (*p == '-') {
        sign = -1;
        p++;
    }

    while (isdigit(*p)) {
        res = res * 10 + (*p - '0');
        p++;
    }

    *end = p;
    return sign * res;
}

typedef struct op_t {
    int opcode;
    int a;
    int b;
    int c;
} op_t;

#define NB_REGISTERS  6

typedef struct state_t {
    int registers[NB_REGISTERS];
    int instr_pointer;
} state_t;

static inline void state_print(const state_t *s)
{
    printf("state: insrt_pointer: %d [%d %d %d %d %d %d]\n",
           s->instr_pointer, s->registers[0], s->registers[1],
           s->registers[2], s->registers[3], s->registers[4],
           s->registers[5]);
}

enum opcode {
    OP_addr = 0,
    OP_addi,
    OP_mulr,
    OP_muli,
    OP_banr,
    OP_bani,
    OP_borr,
    OP_bori,
    OP_setr,
    OP_seti,
    OP_gtir,
    OP_gtri,
    OP_gtrr,
    OP_eqir,
    OP_eqri,
    OP_eqrr,
    OP_MAX
};


static enum opcode opcode_from_str(const char *str)
{
#define TEST_OP(name)   \
    if (strncmp(str, #name, strlen(#name)) == 0) {  \
        return OP_##name;                      \
    }

    TEST_OP(addr);
    TEST_OP(addi);
    TEST_OP(mulr);
    TEST_OP(muli);
    TEST_OP(banr);
    TEST_OP(bani);
    TEST_OP(borr);
    TEST_OP(bori);
    TEST_OP(setr);
    TEST_OP(seti);
    TEST_OP(gtir);
    TEST_OP(gtri);
    TEST_OP(gtrr);
    TEST_OP(eqir);
    TEST_OP(eqri);
    TEST_OP(eqrr);
    printf("unknown opcode: %.*s", (int)strlen("addr"), str);
    exit(1);
}

static void state_apply(state_t *state, op_t *op)
{
    switch (op->opcode) {
      case OP_addr:
        state->registers[op->c] = state->registers[op->a] + state->registers[op->b];
        break;
      case OP_addi:
        state->registers[op->c] = state->registers[op->a] + op->b;
        break;

      case OP_mulr:
        state->registers[op->c] = state->registers[op->a] * state->registers[op->b];
        break;
      case OP_muli:
        state->registers[op->c] = state->registers[op->a] * op->b;
        break;

      case OP_banr:
        state->registers[op->c] = state->registers[op->a] & state->registers[op->b];
        break;
      case OP_bani:
        state->registers[op->c] = state->registers[op->a] & op->b;
        break;

      case OP_borr:
        state->registers[op->c] = state->registers[op->a] | state->registers[op->b];
        break;
      case OP_bori:
        state->registers[op->c] = state->registers[op->a] | op->b;
        break;

      case OP_setr:
        state->registers[op->c] = state->registers[op->a];
        break;
      case OP_seti:
        state->registers[op->c] = op->a;
        break;

      case OP_gtir:
        state->registers[op->c] = op->a > state->registers[op->b];
        break;
      case OP_gtri:
        state->registers[op->c] = state->registers[op->a] > op->b;
        break;
      case OP_gtrr:
        state->registers[op->c] = state->registers[op->a] > state->registers[op->b];
        break;

      case OP_eqir:
        state->registers[op->c] = op->a == state->registers[op->b];
        break;
      case OP_eqri:
        state->registers[op->c] = state->registers[op->a] == op->b;
        break;
      case OP_eqrr:
        state->registers[op->c] = state->registers[op->a] == state->registers[op->b];
        break;

      default:
        printf("unknown opcode: %d\n", op->opcode);
        exit(1);
    }
}

static state_t state;

static op_t program[128];
static int nb_ops;

int main(int argc, char **argv)
{
    int fd = -1;
    struct stat st;
    char *parser = MAP_FAILED;
    char *parser_start, *parser_end;
    int part1_result = 0;

    if ((fd = open("input", O_RDONLY)) < 0 || fstat(fd, &st) < 0) {
        printf("failed to open input: %m\n");
        return 1;
    }
    if (!st.st_size) {
        printf("input is empty\n");
        return 1;
    }

    parser = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (parser == MAP_FAILED) {
        printf("failed to mmap input: %m\n");
        return 1;
    }
    parser_start = parser;
    parser_end = parser + st.st_size;

    while (parser < parser_end) {
        op_t *op;

        if (*parser == '#') {
            parser += strlen("#ip ");
            state.instr_pointer = atoi_p(parser, &parser);
            parser += strlen("\n");
            continue;
        }

        op = &program[nb_ops++];
        op->opcode = opcode_from_str(parser);

        parser += strlen("addi ");

        op->a = atoi_p(parser, &parser);
        parser += strlen(" ");
        op->b = atoi_p(parser, &parser);
        parser += strlen(" ");
        op->c = atoi_p(parser, &parser);
        parser += strlen("\n");
    }

    printf("nb_opts: %d\n", nb_ops);

    state_print(&state);
    for (;;) {
        int instr = state.registers[state.instr_pointer];

        if (instr < 0 || instr >= nb_ops) {
            break;
        }

        //printf("execute[%d]: %d %d %d %d\n", instr, program[instr].opcode, program[instr].a,
        //       program[instr].b, program[instr].c);
        state_apply(&state, &program[instr]);
        state_print(&state);

        state.registers[state.instr_pointer]++;
    }
    state_print(&state);

    printf("part1_result: %d\n", state.registers[0]);

    memset(state.registers, 0, NB_REGISTERS * sizeof(int));
    state.registers[0] = 1;

    state_print(&state);
    for (;;) {
        int instr = state.registers[state.instr_pointer];

        if (instr < 0 || instr >= nb_ops) {
            break;
        }

        //printf("execute[%d]: %d %d %d %d\n", instr, program[instr].opcode, program[instr].a,
        //       program[instr].b, program[instr].c);
        state_apply(&state, &program[instr]);
        state_print(&state);

        state.registers[state.instr_pointer]++;
    }
    state_print(&state);

    printf("part2_result: %d\n", state.registers[0]);

    return 0;
}
