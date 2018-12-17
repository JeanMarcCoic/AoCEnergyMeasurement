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

#define MAP_MAX_SIZE  35

static char map[MAP_MAX_SIZE][MAP_MAX_SIZE + 1];
static int map_size_x;
static int map_size_y;

static int pathfinding_map[MAP_MAX_SIZE][MAP_MAX_SIZE + 1];

static void print_map(void)
{
    for (int y = 0; y < map_size_y; y++) {
        for (int x = 0; x < map_size_x; x++) {
            printf("%c", map[x][y]);
        }
        printf("\n");
    }
}

typedef struct unit_t {
    char type;
    int hp;
    int x, y;
    int died;
} unit_t;

static unit_t *units[32];
static int nb_units;

static unit_t *unit_new(void)
{
    unit_t *unit = calloc(1, sizeof(unit_t));

    units[nb_units++] = unit;
    return unit;
}

static int unit_compare(const void *unit1_p, const void *unit2_p)
{
    const unit_t *unit1 = *(const unit_t **)unit1_p;
    const unit_t *unit2 = *(const unit_t **)unit2_p;

    if (unit1->died != unit2->died) {
        return unit1->died - unit2->died;
    }

    if (unit1->y != unit2->y) {
        return unit1->y - unit2->y;
    }

    return unit1->x - unit2->x;
}

typedef struct shortest_t {
    int dist;
    int x, y;
} shortest_t;

static void do_path_one(int nth, int x, int y, int current_distance,
                        shortest_t *path)
{
    //printf("do:  %d  %d,%d  d:%d, path:%d %d,%d\n",
    //       nth, x, y, current_distance, path->dist, path->x, path->y);
    if (map[x][y] != '.') {
        //printf("end: %d  %d,%d d:%d -> Wall\n", nth, x, y, current_distance);
        return;
    }

    if (pathfinding_map[x][y] < 0) {
        if (pathfinding_map[x][y] == -nth - 1) {
            if (current_distance < path->dist) {
                path->dist = current_distance;
                path->x = x;
                path->y = y;
            } else if (current_distance == path->dist) {
                if (y < path->y) {
                    path->x = x;
                    path->y = y;
                } else if (y == path->y) {
                    if (x < path->x) {
                        path->x = x;
                        path->y = y;
                    }
                }
            }
        }
        //printf("end: %d  %d,%d d:%d, path:%d %d,%d -> target\n", nth, x, y,
        //       current_distance, path->dist, path->x, path->y);
        return;
    }

    if (pathfinding_map[x][y] > 0 && pathfinding_map[x][y] <= current_distance) {
        //printf("end: %d  %d,%d d:%d -> not optiomal\n", nth, x, y,
        //       current_distance);
        return;
    }
    pathfinding_map[x][y] = current_distance;

    do_path_one(nth, x - 1, y, current_distance + 1, path);
    do_path_one(nth, x + 1, y, current_distance + 1, path);
    do_path_one(nth, x, y - 1, current_distance + 1, path);
    do_path_one(nth, x, y + 1, current_distance + 1, path);
}

static void do_pathfinding(int nth, shortest_t *path)
{
    int nth_x = units[nth]->x;
    int nth_y = units[nth]->y;
    char enemy = units[nth]->type == 'E' ? 'G' : 'E';

    for (int y = 0; y < map_size_y; y++) {
        memset(pathfinding_map[y], 0, map_size_x * sizeof(int));
    }

    if (map[nth_x][nth_y - 1] == enemy) {
        path->dist = 0;
        return;
    }
    //printf("target: %d,%d\n", nth_x, nth_y - 1);
    pathfinding_map[nth_x][nth_y - 1] = -nth - 1;

    if (map[nth_x - 1][nth_y] == enemy) {
        path->dist = 0;
        return;
    }
    //printf("target: %d,%d\n", nth_x - 1, nth_y);
    pathfinding_map[nth_x - 1][nth_y] = -nth - 1;

    if (map[nth_x + 1][nth_y] == enemy) {
        path->dist = 0;
        return;
    }
    //printf("target: %d,%d\n", nth_x + 1, nth_y);
    pathfinding_map[nth_x + 1][nth_y] = -nth - 1;

    if (map[nth_x][nth_y + 1] == enemy) {
        path->dist = 0;
        return;
    }
    //printf("target: %d,%d\n", nth_x, nth_y + 1);
    pathfinding_map[nth_x][nth_y + 1] = -nth - 1;

    path->dist = INT32_MAX;

    for (int i = 0; i < nb_units; i++) {
        if (!units[i]->died && units[i]->type != units[nth]->type) {
            int x = units[i]->x;
            int y = units[i]->y;

            do_path_one(nth, x, y - 1, 1, path);
            do_path_one(nth, x - 1, y, 1, path);
            do_path_one(nth, x + 1, y, 1, path);
            do_path_one(nth, x, y + 1, 1, path);
        }
    }

    if (path->dist == INT32_MAX) {
        /* no possible move */
        path->dist = 0;
    }
}

static void do_move(int nth, int x, int y)
{
    if (map[x][y] != '.') {
        printf("illegal move: %d %d,%d\n", nth, x, y);
        exit(1);
    }

    map[x][y] = units[nth]->type;
    map[units[nth]->x][units[nth]->y] = '.';

    units[nth]->x = x;
    units[nth]->y = y;
}

static int unit_lookup(int x, int y)
{
    for (int i = 0; i < nb_units; i++) {
        if (!units[i]->died && units[i]->x == x && units[i]->y == y) {
            return i;
        }
    }

    printf("unit not found: %d,%d\n", x, y);
    exit(1);
}

static void lookup_target_one(int x, int y, char enemy, int *target, int *min_hp)
{
    if (map[x][y] == enemy) {
        int u = unit_lookup(x, y);
        if (units[u]->hp < *min_hp) {
            *target = u;
            *min_hp = units[u]->hp;
        }
    }
}

static int do_turn(void)
{
    int actions = 0;

    qsort(units, nb_units, sizeof(unit_t *), &unit_compare);

    for (int i = 0; i < nb_units; i++) {
        char enemy = units[i]->type == 'E' ? 'G' : 'E';
        shortest_t path;
        int target = -1;
        int min_hp = 300;
        int x, y;

        if (units[i]->died) {
            continue;
        }
        /* Move */
        do_pathfinding(i, &path);

        if (path.dist > 0) {
            actions++;
            do_move(i, path.x, path.y);
        }

        /* Attack */
        x = units[i]->x;
        y = units[i]->y;
        lookup_target_one(x, y - 1, enemy, &target, &min_hp);
        lookup_target_one(x - 1, y, enemy, &target, &min_hp);
        lookup_target_one(x + 1, y, enemy, &target, &min_hp);
        lookup_target_one(x, y + 1, enemy, &target, &min_hp);

        if (target >= 0) {
            actions++;
            units[target]->hp -= 3;

            if (units[target]->hp <= 0) {
                units[target]->died = 1;
                map[units[target]->x][units[target]->y] = '.';
            }
        }
    }

    return actions;
}

int main(int argc, char **argv)
{
    int fd = -1;
    struct stat st;
    char *parser = MAP_FAILED;
    const char *fname = "input";
    char *parser_start, *parser_end;
    int first_row = 1;
    int nb_rows = 0;
    int x = 0;

    if (argc > 1) {
        fname = argv[1];
    }
    if ((fd = open(fname, O_RDONLY)) < 0 || fstat(fd, &st) < 0) {
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
        unit_t *unit;

        if (*parser == '\n') {
            if (first_row) {
                map_size_x = parser - parser_start;
                if (map_size_x > MAP_MAX_SIZE) {
                    printf("map row too long: %d (max: %d)\n", map_size_x,
                           MAP_MAX_SIZE);
                    exit(1);
                }
                first_row = 0;

            }
            map[x][nb_rows] = '\0';
            nb_rows++;
            x = 0;
            parser++;
            continue;
        }
        map[x][nb_rows] = *parser;

        switch (*parser) {
          case 'E':
          case 'G':
            unit = unit_new();
            unit->type = *parser;
            unit->x = x;
            unit->y = nb_rows;
            unit->hp = 200;
            break;

          default:
            break;
        }
        parser++;
        x++;
    }

    map_size_y = nb_rows;
    if (map_size_y > MAP_MAX_SIZE) {
        printf("map has too many rows: %d (max: %d)\n", map_size_y,
               MAP_MAX_SIZE);
        exit(1);
    }

    print_map();
    for (int round = 0;; round++) {
        printf("ROUND %d\n", round);
        if (!do_turn()) {
            int hp_sum = 0;
            print_map();
            for (int i = 0; i < nb_units; i++) {
                if (!units[i]->died) {
                    hp_sum += units[i]->hp;
                }
            }
            printf("part1: round: %d, hp: %d (answer: %d))\n", round - 1, hp_sum,
                   (round - 1) * hp_sum);
            break;
        }
        print_map();
    }

    return 0;
}
