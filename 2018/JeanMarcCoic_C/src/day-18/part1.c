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

#define MAP_SIZE  50

typedef struct map_t {
    char cells[MAP_SIZE + 2][MAP_SIZE + 2];
} map_t;

static map_t map1;
static map_t map2;
static map_t *cur = &map1, *next = &map2;

static void print_map(map_t *map)
{
    for (int y = 1; y <= MAP_SIZE; y++) {
        for (int x = 1; x <= MAP_SIZE; x++) {
            printf("%c", map->cells[x][y]);
        }
        printf("\n");
    }
}

static inline void compute_one(char c, int *nb_open, int *nb_tree,
                               int *nb_lumber)
{
    switch (c) {
      case '.':
        *nb_open += 1;
        break;
      case '|':
        *nb_tree += 1;
        break;
      case '#':
        *nb_lumber += 1;
        break;
    }
}

static inline void compute_neighbors(map_t *map, int x, int y, int *nb_open,
                                     int *nb_tree, int *nb_lumber)
{
    compute_one(map->cells[x - 1][y - 1], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x    ][y - 1], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x + 1][y - 1], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x - 1][y    ], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x + 1][y    ], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x - 1][y + 1], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x    ][y + 1], nb_open, nb_tree, nb_lumber);
    compute_one(map->cells[x + 1][y + 1], nb_open, nb_tree, nb_lumber);
}

static inline void count_answer(map_t *map, int *nb_tree, int *nb_lumber)
{
    for (int y = 1; y <= MAP_SIZE; y++) {
        for (int x = 1; x <= MAP_SIZE; x++) {
            switch (map->cells[x][y]) {
              case '|':
                *nb_tree += 1;
                break;

              case '#':
                *nb_lumber += 1;
                break;
            }
        }
    }
}

static inline void next_minute(void)
{
    for (int y = 1; y <= MAP_SIZE; y++) {
        for (int x = 1; x <= MAP_SIZE; x++) {
            int nb_open = 0, nb_tree = 0, nb_lumber = 0;

            compute_neighbors(cur, x, y, &nb_open, &nb_tree, &nb_lumber);

            next->cells[x][y] = cur->cells[x][y];
            switch (cur->cells[x][y]) {
              case '.':
                if (nb_tree >= 3) {
                    next->cells[x][y] = '|';
                }
                break;

              case '|':
                if (nb_lumber >= 3) {
                    next->cells[x][y] = '#';
                }
                break;

              case '#':
                if (!(nb_lumber >= 1 && nb_tree >= 1)) {
                    next->cells[x][y] = '.';
                }
                break;
            }
        }
    }

    if (cur == &map1) {
        cur = &map2;
        next = &map1;
    } else {
        cur = &map1;
        next = &map2;
    }
}

int main(int argc, char **argv)
{
    int fd = -1;
    struct stat st;
    char *parser = MAP_FAILED;
    char *parser_start, *parser_end;
    int x = 0;
    int y = 0;
    int part1_tree = 0, part1_lumber = 0;
    int part2_tree = 0, part2_lumber = 0;
    int part2_max = 0;
    int part2_max_minute = 0;

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
        if (*parser == '\n') {
            y++;
            x = 0;
        } else {
            cur->cells[x + 1][y + 1] = *parser;
            x++;
        }
        parser++;
    }

    //print_map(cur);

    for (int minute = 1; minute <= 10; minute++) {
        next_minute();
        //printf("\nafter minute %d:\n", minute);
        //print_map(cur);
    }

    count_answer(cur, &part1_tree, &part1_lumber);

    printf("part1: trees = %d, lumberyards = %d, result = %d\n", part1_tree,
           part1_lumber, part1_tree * part1_lumber);

    for (int minute = 11; minute <= 1000000000; minute++) {
        part2_tree = part2_lumber = 0;
        next_minute();
        //printf("\nafter minute %d:\n", minute);
        //print_map(cur);
        count_answer(cur, &part2_tree, &part2_lumber);

        if (minute > 500 && minute < 1000000) {
            int score = part2_tree * part2_lumber;
            if (part2_max < score) {
                part2_max = score;
                part2_max_minute = minute;
            } else if (part2_max == score) {
                int cycle = minute - part2_max_minute;

                printf("cycle: %d, remaining: %d, after skip: %d\n",
                       cycle, 1000000000 - minute, minute + ((1000000000 -
                                                              minute) / cycle)
                       * cycle);
                minute += ((1000000000 - minute) / cycle) * cycle;
            }
        }

        printf("part2: minute = %d, trees = %d, lumberyards = %d, result = %d, max = %d\n", minute, part2_tree,
               part2_lumber, part2_tree * part2_lumber, part2_max);
    }

    return 0;
}
