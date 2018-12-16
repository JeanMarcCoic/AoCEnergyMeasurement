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

typedef struct state_t {
    int registers[4];
} state_t;

static inline int state_equals(const state_t *s1, const state_t *s2)
{
    return s1->registers[0] == s2->registers[0]
      &&   s1->registers[1] == s2->registers[1]
      &&   s1->registers[2] == s2->registers[2]
      &&   s1->registers[3] == s2->registers[3];
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

typedef struct opcode_test_t {
    int code;

    int nb_remaining;
    uint16_t bits;
} opcode_test_t;

opcode_test_t codes[OP_MAX];

static uint32_t msbDeBruijn32(uint32_t v)
{
    static const int deBruijnBitPosition[32] =
    {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };

    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    return deBruijnBitPosition[(uint32_t)( v * 0x07C4ACDDU) >> 27];
}

static void opcode_is_not(int code, enum opcode op)
{
    opcode_test_t *test = &codes[code];

    if (test->nb_remaining > 1 && test->bits & (1 << op)) {
        test->bits &= ~(1 << op);
        test->nb_remaining--;
        if (test->nb_remaining <= 1) {
            test->code = msbDeBruijn32(test->bits);

            for (int i = 0; i < OP_MAX; i++) {
                if (i != code) {
                    opcode_is_not(i, test->code);
                }
            }
        }
    }
}

static void opcode_test_init(void)
{
    for (int i = 0; i < OP_MAX; i++) {
        codes[i].nb_remaining = 16;
        codes[i].bits = 0xFFFF;
    }
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

int main(int argc, char **argv)
{
    int fd = -1;
    struct stat st;
    char *parser = MAP_FAILED;
    char *parser_start, *parser_end;
    int part1_result = 0;

    opcode_test_init();

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
        state_t before;
        state_t after;
        op_t op;
        int nb_matches = 0;
        int opcode;
#if 0
Before: [1, 0, 1, 3]
9 2 1 0
After:  [2, 0, 1, 3]

#endif
        if (*parser == '\n') {
            break;
        }

        parser += strlen("Before: [");

        before.registers[0] = atoi_p(parser, &parser);
        parser += strlen(", ");
        before.registers[1] = atoi_p(parser, &parser);
        parser += strlen(", ");
        before.registers[2] = atoi_p(parser, &parser);
        parser += strlen(", ");
        before.registers[3] = atoi_p(parser, &parser);
        parser += strlen("]\n");

        opcode = atoi_p(parser, &parser);
        parser += strlen(" ");
        op.a = atoi_p(parser, &parser);
        parser += strlen(" ");
        op.b = atoi_p(parser, &parser);
        parser += strlen(" ");
        op.c = atoi_p(parser, &parser);
        parser += strlen("\nAfter:  [");

        after.registers[0] = atoi_p(parser, &parser);
        parser += strlen(", ");
        after.registers[1] = atoi_p(parser, &parser);
        parser += strlen(", ");
        after.registers[2] = atoi_p(parser, &parser);
        parser += strlen(", ");
        after.registers[3] = atoi_p(parser, &parser);
        parser += strlen("]\n\n");

        for (int i = 0; i < OP_MAX; i++) {
            state_t tmp;

            tmp = before;

            op.opcode = i;
            state_apply(&tmp, &op);

            if (state_equals(&tmp, &after)) {
                nb_matches++;
            } else {
                opcode_is_not(opcode, i);
            }
        }

        if (nb_matches >= 3) {
            part1_result++;
        }
    }

    parser += strlen("\n\n");
    while (parser < parser_end) {
        op_t op;
        //6 0 0 3
        op.opcode = codes[atoi_p(parser, &parser)].code;
        parser += strlen(" ");
        op.a = atoi_p(parser, &parser);
        parser += strlen(" ");
        op.b = atoi_p(parser, &parser);
        parser += strlen(" ");
        op.c = atoi_p(parser, &parser);
        parser += strlen("\n");

        state_apply(&state, &op);
    }

    printf("part1_result: %d\n", part1_result);
    printf("part2_result: %d\n", state.registers[0]);

    return 0;
}
