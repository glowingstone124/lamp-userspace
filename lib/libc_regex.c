#include <ctype.h>
#include <regex.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    RE_CHAR,
    RE_ANY,
    RE_CLASS,
    RE_SPLIT,
    RE_JMP,
    RE_SAVE,
    RE_BOL,
    RE_EOL,
    RE_WORDB,
    RE_NOT_WORDB,
    RE_MATCH
};

typedef struct {
    unsigned char op;
    unsigned char ch;
    unsigned char set[32];
    int x;
    int y;
} re_inst;

typedef struct {
    re_inst *program;
    size_t length;
    size_t capacity;
    const char *cursor;
    int cflags;
    int extended;
    int groups;
    int error;
} re_compiler;

static int re_emit(re_compiler *c, int op) {
    if (c->length == c->capacity) {
        size_t capacity = c->capacity ? c->capacity * 2 : 32;
        re_inst *program = realloc(c->program, capacity * sizeof(*program));
        if (!program) {
            c->error = REG_ESPACE;
            return -1;
        }
        c->program = program;
        c->capacity = capacity;
    }
    memset(&c->program[c->length], 0, sizeof(c->program[c->length]));
    c->program[c->length].op = (unsigned char)op;
    return (int)c->length++;
}

static int re_insert(re_compiler *c, int index) {
    size_t i;
    if (c->length == c->capacity) {
        size_t capacity = c->capacity ? c->capacity * 2 : 32;
        re_inst *program = realloc(c->program, capacity * sizeof(*program));
        if (!program) {
            c->error = REG_ESPACE;
            return -1;
        }
        c->program = program;
        c->capacity = capacity;
    }
    memmove(&c->program[index + 1], &c->program[index],
            (c->length - (size_t)index) * sizeof(*c->program));
    memset(&c->program[index], 0, sizeof(c->program[index]));
    c->length++;

    for (i = 0; i < c->length; i++) {
        if (c->program[i].x >= index) {
            c->program[i].x++;
        }
        if (c->program[i].y >= index) {
            c->program[i].y++;
        }
    }
    return index;
}

static int re_copy_block(re_compiler *c, int start, int end) {
    size_t count = (size_t)(end - start);
    size_t old_length = c->length;
    size_t i;
    int delta = (int)(old_length - (size_t)start);

    if (count == 0 || c->error) {
        return (int)old_length;
    }
    if (c->length + count > c->capacity) {
        size_t capacity = c->capacity;
        re_inst *program;
        while (capacity < c->length + count) {
            capacity = capacity ? capacity * 2 : 32;
        }
        program = realloc(c->program, capacity * sizeof(*program));
        if (!program) {
            c->error = REG_ESPACE;
            return -1;
        }
        c->program = program;
        c->capacity = capacity;
    }
    memcpy(&c->program[c->length], &c->program[start], count * sizeof(*c->program));
    for (i = c->length; i < c->length + count; i++) {
        if (c->program[i].x >= start && c->program[i].x <= end) {
            c->program[i].x += delta;
        }
        if (c->program[i].y >= start && c->program[i].y <= end) {
            c->program[i].y += delta;
        }
    }
    c->length += count;
    return (int)old_length;
}

static void re_set_char(unsigned char set[32], unsigned char ch) {
    set[ch >> 3] |= (unsigned char)(1 << (ch & 7));
}

static void re_set_range(unsigned char set[32], unsigned char first, unsigned char last) {
    unsigned int ch;
    if (first > last) {
        return;
    }
    for (ch = first; ch <= last; ch++) {
        re_set_char(set, (unsigned char)ch);
    }
}

static int re_parse_escape(const char **cursor, int *error) {
    const char *p = *cursor;
    int ch;
    if (*p != '\\') {
        ch = (unsigned char)*p++;
        *cursor = p;
        return ch;
    }
    p++;
    switch (*p) {
        case 'n': ch = '\n'; break;
        case 't': ch = '\t'; break;
        case 'r': ch = '\r'; break;
        case 'f': ch = '\f'; break;
        case 'v': ch = '\v'; break;
        case 'a': ch = '\a'; break;
        case '0': ch = '\0'; break;
        case '\0': *error = REG_EESCAPE; return -1;
        default: ch = (unsigned char)*p; break;
    }
    if (*p == '\0') {
        *error = REG_EESCAPE;
        return -1;
    }
    p++;
    *cursor = p;
    return ch;
}

static int re_named_class(const char **cursor, unsigned char set[32]) {
    static const struct {
        const char *name;
        unsigned char first;
        unsigned char last;
    } classes[] = {
        {"alpha:", 'a', 'z'}, {"alpha:", 'A', 'Z'},
        {"digit:", '0', '9'}, {"lower:", 'a', 'z'},
        {"upper:", 'A', 'Z'}, {"space:", '\t', '\r'},
        {"space:", ' ', ' '}, {"blank:", '\t', '\t'},
        {"blank:", ' ', ' '}, {"punct:", '!', '/'},
        {"punct:", ':', '@'}, {"punct:", '[', '`'},
        {"punct:", '{', '~'}, {"cntrl:", '\0', '\037'},
        {"cntrl:", '\177', '\177'}, {"xdigit:", '0', '9'},
        {"xdigit:", 'A', 'F'}, {"xdigit:", 'a', 'f'},
        {"alnum:", '0', '9'}, {"alnum:", 'A', 'Z'},
        {"alnum:", 'a', 'z'}, {"print:", ' ', '~'},
        {"graph:", '!', '~'}
    };
    const char *p = *cursor;
    size_t i;
    if (p[0] != '[' || p[1] != ':') {
        return 0;
    }
    p += 2;
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
        size_t length = strlen(classes[i].name);
        if (strncmp(p, classes[i].name, length) == 0 && p[length] == ']') {
            re_set_range(set, classes[i].first, classes[i].last);
            *cursor = p + length + 1;
            return 1;
        }
    }
    return 0;
}

static int re_compile_class(re_compiler *c) {
    const char *p = c->cursor + 1;
    unsigned char set[32];
    unsigned char negated = 0;
    unsigned int i;
    int index;

    memset(set, 0, sizeof(set));
    if (*p == '^') {
        negated = 1;
        p++;
    }
    if (*p == ']') {
        re_set_char(set, (unsigned char)']');
        p++;
    }
    while (*p != '\0' && *p != ']') {
        const char *saved = p;
        if (re_named_class(&p, set)) {
            continue;
        }
        if (p[0] == '[' && p[1] == ':') {
            c->error = REG_ECTYPE;
            return -1;
        }
        {
            int first = re_parse_escape(&p, &c->error);
            if (first < 0) {
                return -1;
            }
            if (p[0] == '-' && p[1] != ']' && p[1] != '\0') {
                int last;
                p++;
                last = re_parse_escape(&p, &c->error);
                if (last < 0) {
                    return -1;
                }
                if (last < first) {
                    c->error = REG_ERANGE;
                    return -1;
                }
                re_set_range(set, (unsigned char)first, (unsigned char)last);
            } else {
                re_set_char(set, (unsigned char)first);
            }
        }
        (void)saved;
    }
    if (*p != ']') {
        c->error = REG_EBRACK;
        return -1;
    }
    p++;
    if (negated) {
        for (i = 0; i < sizeof(set); i++) {
            set[i] = (unsigned char)~set[i];
        }
    }
    index = re_emit(c, RE_CLASS);
    if (index < 0) {
        return -1;
    }
    memcpy(c->program[index].set, set, sizeof(set));
    c->cursor = p;
    return index;
}

static void re_compile_simple_class(re_compiler *c, char kind, int negate) {
    unsigned char set[32];
    unsigned int i;
    int index;
    memset(set, 0, sizeof(set));
    switch (kind) {
        case 'w': case 'd':
            re_set_range(set, '0', '9');
            if (kind == 'w') {
                re_set_range(set, 'a', 'z');
                re_set_range(set, 'A', 'Z');
                re_set_char(set, '_');
            }
            break;
        case 's':
            re_set_char(set, ' ');
            re_set_char(set, '\t');
            re_set_char(set, '\n');
            re_set_char(set, '\r');
            re_set_char(set, '\f');
            re_set_char(set, '\v');
            break;
        default:
            break;
    }
    if (negate) {
        for (i = 0; i < sizeof(set); i++) {
            set[i] = (unsigned char)~set[i];
        }
    }
    index = re_emit(c, RE_CLASS);
    if (index >= 0) {
        memcpy(c->program[index].set, set, sizeof(set));
    }
}

static int re_is_alt_delimiter(const re_compiler *c, int in_group) {
    if (c->extended) {
        return c->cursor[0] == '|';
    }
    (void)in_group;
    return c->cursor[0] == '\\' && c->cursor[1] == '|';
}

static int re_is_group_close(const re_compiler *c, int in_group) {
    if (!in_group) {
        return 0;
    }
    if (c->extended) {
        return c->cursor[0] == ')';
    }
    return c->cursor[0] == '\\' && c->cursor[1] == ')';
}

static void re_compile_alternation(re_compiler *c, int in_group);

static int re_compile_atom(re_compiler *c, int in_group) {
    const char *p = c->cursor;
    int ch;
    int index;

    if (p[0] == '\0' || re_is_alt_delimiter(c, in_group) || re_is_group_close(c, in_group)) {
        return 0;
    }

    if (p[0] == '^') {
        c->cursor = p + 1;
        return re_emit(c, RE_BOL);
    }
    if (p[0] == '$') {
        c->cursor = p + 1;
        return re_emit(c, RE_EOL);
    }
    if (p[0] == '.') {
        c->cursor = p + 1;
        return re_emit(c, RE_ANY);
    }
    if (p[0] == '[') {
        return re_compile_class(c);
    }

    if (p[0] == '(' && c->extended) {
        int capture = 1;
        int save;
        p++;
        if (p[0] == '?' && p[1] == ':') {
            capture = 0;
            p += 2;
        }
        c->cursor = p;
        if (capture) {
            c->groups++;
            save = re_emit(c, RE_SAVE);
            if (save < 0) {
                return -1;
            }
            c->program[save].x = c->groups * 2;
        }
        re_compile_alternation(c, 1);
        if (c->error) {
            return -1;
        }
        if (c->cursor[0] != ')') {
            c->error = REG_EPAREN;
            return -1;
        }
        c->cursor++;
        if (capture) {
            save = re_emit(c, RE_SAVE);
            if (save < 0) {
                return -1;
            }
            c->program[save].x = c->groups * 2 + 1;
        }
        return 1;
    }

    if (p[0] == '\\' && p[1] == '(' && !c->extended) {
        int save;
        c->cursor = p + 2;
        c->groups++;
        save = re_emit(c, RE_SAVE);
        if (save < 0) {
            return -1;
        }
        c->program[save].x = c->groups * 2;
        re_compile_alternation(c, 1);
        if (c->error) {
            return -1;
        }
        if (c->cursor[0] != '\\' || c->cursor[1] != ')') {
            c->error = REG_EPAREN;
            return -1;
        }
        c->cursor += 2;
        save = re_emit(c, RE_SAVE);
        if (save < 0) {
            return -1;
        }
        c->program[save].x = c->groups * 2 + 1;
        return 1;
    }

    if (p[0] == '\\' && p[1] != '\0') {
        char kind = p[1];
        if (kind == 'w' || kind == 'W' || kind == 's' ||
            kind == 'S' || kind == 'd' || kind == 'D') {
            c->cursor = p + 2;
            re_compile_simple_class(c, kind, kind >= 'A' && kind <= 'Z');
            return 1;
        }
        if (kind == 'b' || kind == 'B') {
            c->cursor = p + 2;
            return re_emit(c, kind == 'b' ? RE_WORDB : RE_NOT_WORDB);
        }
    }

    ch = re_parse_escape(&c->cursor, &c->error);
    if (ch < 0) {
        return -1;
    }
    index = re_emit(c, RE_CHAR);
    if (index < 0) {
        return -1;
    }
    c->program[index].ch = (unsigned char)ch;
    return 1;
}

static int re_parse_interval(re_compiler *c, int *min, int *max) {
    const char *p = c->cursor;
    int value = 0;
    int have_digit = 0;

    while (isdigit((unsigned char)*p)) {
        value = value * 10 + (*p - '0');
        if (value > 10000) {
            c->error = REG_BADBR;
            return 0;
        }
        have_digit = 1;
        p++;
    }
    *min = have_digit ? value : 0;
    value = 0;
    if (*p == ',') {
        p++;
        if (isdigit((unsigned char)*p)) {
            have_digit = 1;
            while (isdigit((unsigned char)*p)) {
                value = value * 10 + (*p - '0');
                if (value > 10000) {
                    c->error = REG_BADBR;
                    return 0;
                }
                p++;
            }
            *max = value;
        } else {
            *max = -1;
        }
    } else {
        *max = *min;
    }
    if (*p != '}') {
        c->error = REG_BADBR;
        return 0;
    }
    c->cursor = p + 1;
    return 1;
}

static int re_wrap_optional(re_compiler *c, int start) {
    int split = re_insert(c, start);
    if (split < 0) {
        return -1;
    }
    c->program[split].op = RE_SPLIT;
    c->program[split].x = start + 1;
    c->program[split].y = (int)c->length;
    return 0;
}

static int re_wrap_star(re_compiler *c, int start) {
    int split = re_insert(c, start);
    int jump;
    if (split < 0) {
        return -1;
    }
    c->program[split].op = RE_SPLIT;
    c->program[split].x = start + 1;
    c->program[split].y = (int)c->length;
    jump = re_emit(c, RE_JMP);
    if (jump < 0) {
        return -1;
    }
    c->program[jump].x = split;
    return 0;
}

static int re_compile_quantifier(re_compiler *c, int atom_start) {
    const char *p = c->cursor;
    int plus = 0;
    int question = 0;
    int interval = 0;
    int min = 0;
    int max = 0;
    int atom_end;

    if (p[0] == '*') {
        c->cursor++;
        return re_wrap_star(c, atom_start);
    }
    if (p[0] == '+' && c->extended) {
        plus = 1;
    } else if (p[0] == '?' && c->extended) {
        question = 1;
    } else if (p[0] == '\\' && !c->extended &&
               (p[1] == '+' || p[1] == '?')) {
        plus = p[1] == '+';
        question = p[1] == '?';
    } else if ((p[0] == '{' && c->extended) ||
               (p[0] == '\\' && p[1] == '{' && !c->extended)) {
        interval = 1;
    } else {
        return 0;
    }

    if (plus) {
        int split;
        c->cursor += c->extended ? 1 : 2;
        atom_end = (int)c->length;
        split = re_emit(c, RE_SPLIT);
        if (split < 0) {
            return -1;
        }
        c->program[split].x = atom_start;
        c->program[split].y = (int)c->length;
        return 1;
    }
    if (question) {
        c->cursor += c->extended ? 1 : 2;
        return re_wrap_optional(c, atom_start) == 0 ? 1 : -1;
    }

    c->cursor += c->extended ? 1 : 2;
    if (!re_parse_interval(c, &min, &max)) {
        return -1;
    }
    if (max != -1 && max < min) {
        c->error = REG_BADBR;
        return -1;
    }

    atom_end = (int)c->length;
    if (min == 0) {
        if (max == -1) {
            return re_wrap_star(c, atom_start) == 0 ? 1 : -1;
        }
        if (re_wrap_optional(c, atom_start) < 0) {
            return -1;
        }
        if (max > 1) {
            int count;
            for (count = 1; count < max; count++) {
                int split = re_emit(c, RE_SPLIT);
                int body;
                if (split < 0) {
                    return -1;
                }
                body = re_copy_block(c, atom_start, atom_end);
                if (body < 0) {
                    return -1;
                }
                c->program[split].x = body;
                c->program[split].y = (int)c->length;
            }
        }
        return 1;
    }

    {
        int count;
        for (count = 1; count < min; count++) {
            if (re_copy_block(c, atom_start, atom_end) < 0) {
                return -1;
            }
        }
        if (max == -1) {
            int split = re_emit(c, RE_SPLIT);
            int body;
            int jump;
            if (split < 0) {
                return -1;
            }
            body = re_copy_block(c, atom_start, atom_end);
            if (body < 0) {
                return -1;
            }
            c->program[split].x = body;
            c->program[split].y = (int)c->length;
            jump = re_emit(c, RE_JMP);
            if (jump < 0) {
                return -1;
            }
            c->program[jump].x = split;
        } else {
            for (count = min; count < max; count++) {
                int split = re_emit(c, RE_SPLIT);
                int body;
                if (split < 0) {
                    return -1;
                }
                body = re_copy_block(c, atom_start, atom_end);
                if (body < 0) {
                    return -1;
                }
                c->program[split].x = body;
                c->program[split].y = (int)c->length;
            }
        }
    }
    return 1;
}

static void re_compile_sequence(re_compiler *c, int in_group) {
    while (!c->error && c->cursor[0] != '\0' &&
           !re_is_alt_delimiter(c, in_group) &&
           !re_is_group_close(c, in_group)) {
        int atom_start = (int)c->length;
        int atom = re_compile_atom(c, in_group);
        if (atom < 0) {
            return;
        }
        if (!atom) {
            break;
        }
        if (re_compile_quantifier(c, atom_start) < 0) {
            return;
        }
    }
}

static void re_compile_alternation(re_compiler *c, int in_group) {
    int first_start = (int)c->length;
    int first_split = -1;
    int last_split = -1;
    int jumps[128];
    int jump_count = 0;

    re_compile_sequence(c, in_group);
    if (c->error) {
        return;
    }
    if (!re_is_alt_delimiter(c, in_group)) {
        return;
    }

    first_split = re_insert(c, first_start);
    if (first_split < 0) {
        return;
    }
    c->program[first_split].op = RE_SPLIT;
    c->program[first_split].x = first_start + 1;
    c->program[first_split].y = (int)c->length;
    last_split = first_split;
    {
        int jump = re_emit(c, RE_JMP);
        if (jump < 0) {
            return;
        }
        jumps[jump_count++] = jump;
    }

    while (re_is_alt_delimiter(c, in_group)) {
        int split;
        int body;
        int jump;
        c->cursor += c->extended ? 1 : 2;
        split = re_emit(c, RE_SPLIT);
        if (split < 0) {
            return;
        }
        c->program[last_split].y = split;
        body = (int)c->length;
        re_compile_sequence(c, in_group);
        if (c->error) {
            return;
        }
        c->program[split].x = body;
        c->program[split].y = (int)c->length;
        last_split = split;
        jump = re_emit(c, RE_JMP);
        if (jump < 0) {
            return;
        }
        if (jump_count >= (int)(sizeof(jumps) / sizeof(jumps[0]))) {
            c->error = REG_BADPAT;
            return;
        }
        jumps[jump_count++] = jump;
        if (!re_is_alt_delimiter(c, in_group)) {
            break;
        }
    }

    {
        int i;
        int end = (int)c->length;
        c->program[last_split].y = end;
        for (i = 0; i < jump_count; i++) {
            c->program[jumps[i]].x = end;
        }
    }
}

int regcomp(regex_t *preg, const char *regex, int cflags) {
    re_compiler compiler;
    int match;
    int save;

    if (!preg || !regex) {
        return REG_BADPAT;
    }
    memset(&compiler, 0, sizeof(compiler));
    compiler.cursor = regex;
    compiler.cflags = cflags;
    compiler.extended = (cflags & REG_EXTENDED) != 0;

    save = re_emit(&compiler, RE_SAVE);
    if (save < 0) {
        free(compiler.program);
        preg->re_program = 0;
        preg->re_nsub = 0;
        preg->re_cflags = 0;
        return REG_ESPACE;
    }
    compiler.program[save].x = 0;

    re_compile_alternation(&compiler, 0);
    if (!compiler.error && compiler.cursor[0] != '\0') {
        compiler.error = REG_BADPAT;
    }
    if (compiler.error) {
        free(compiler.program);
        preg->re_program = 0;
        preg->re_nsub = 0;
        preg->re_cflags = 0;
        return compiler.error;
    }
    save = re_emit(&compiler, RE_SAVE);
    if (save < 0) {
        free(compiler.program);
        preg->re_program = 0;
        preg->re_nsub = 0;
        preg->re_cflags = 0;
        return REG_ESPACE;
    }
    compiler.program[save].x = 1;

    match = re_emit(&compiler, RE_MATCH);
    if (match < 0) {
        free(compiler.program);
        preg->re_program = 0;
        preg->re_nsub = 0;
        preg->re_cflags = 0;
        return REG_ESPACE;
    }
    preg->re_program = compiler.program;
    preg->re_nsub = (size_t)compiler.groups;
    preg->re_cflags = cflags;
    return 0;
}

static int re_class_match(const re_inst *inst, unsigned char ch, int cflags) {
    if (inst->set[ch >> 3] & (1 << (ch & 7))) {
        return 1;
    }
    if (cflags & REG_ICASE) {
        unsigned char lower = (unsigned char)tolower(ch);
        unsigned char upper = (unsigned char)toupper(ch);
        if (inst->set[lower >> 3] & (1 << (lower & 7))) {
            return 1;
        }
        if (inst->set[upper >> 3] & (1 << (upper & 7))) {
            return 1;
        }
    }
    return 0;
}

static int re_is_word(unsigned char ch) {
    return isalnum(ch) || ch == '_';
}

static int re_run(const re_inst *program, size_t program_length, int pc,
                  const char *text, size_t length, size_t pos,
                  int *captures, unsigned int *generations,
                  unsigned int generation, int cflags, int eflags) {
    size_t state;

    if (pc < 0 || (size_t)pc >= program_length) {
        return 0;
    }
    state = (size_t)pc * (length + 1) + pos;
    if (generations[state] == generation) {
        return 0;
    }
    generations[state] = generation;

    switch (program[pc].op) {
        case RE_CHAR:
            if (pos >= length) {
                return 0;
            }
            if (cflags & REG_ICASE) {
                if (tolower((unsigned char)text[pos]) !=
                    tolower(program[pc].ch)) {
                    return 0;
                }
            } else if ((unsigned char)text[pos] != program[pc].ch) {
                return 0;
            }
            return re_run(program, program_length, pc + 1, text, length,
                          pos + 1, captures, generations, generation,
                          cflags, eflags);
        case RE_ANY:
            if (pos >= length || ((cflags & REG_NEWLINE) && text[pos] == '\n')) {
                return 0;
            }
            return re_run(program, program_length, pc + 1, text, length,
                          pos + 1, captures, generations, generation,
                          cflags, eflags);
        case RE_CLASS:
            if (pos >= length || !re_class_match(&program[pc],
                                                  (unsigned char)text[pos], cflags)) {
                return 0;
            }
            return re_run(program, program_length, pc + 1, text, length,
                          pos + 1, captures, generations, generation,
                          cflags, eflags);
        case RE_SPLIT:
            if (re_run(program, program_length, program[pc].x, text, length,
                       pos, captures, generations, generation, cflags, eflags)) {
                return 1;
            }
            return re_run(program, program_length, program[pc].y, text, length,
                          pos, captures, generations, generation, cflags, eflags);
        case RE_JMP:
            return re_run(program, program_length, program[pc].x, text, length,
                          pos, captures, generations, generation, cflags, eflags);
        case RE_SAVE: {
            int old = captures[program[pc].x];
            captures[program[pc].x] = (int)pos;
            if (re_run(program, program_length, pc + 1, text, length, pos,
                       captures, generations, generation, cflags, eflags)) {
                return 1;
            }
            captures[program[pc].x] = old;
            return 0;
        }
        case RE_BOL:
            if (pos == 0) {
                if (eflags & REG_NOTBOL) {
                    return 0;
                }
            } else if (!(cflags & REG_NEWLINE) || text[pos - 1] != '\n') {
                return 0;
            }
            return re_run(program, program_length, pc + 1, text, length,
                          pos, captures, generations, generation, cflags, eflags);
        case RE_EOL:
            if (pos == length) {
                if (eflags & REG_NOTEOL) {
                    return 0;
                }
            } else if (!(cflags & REG_NEWLINE) || text[pos] != '\n') {
                return 0;
            }
            return re_run(program, program_length, pc + 1, text, length,
                          pos, captures, generations, generation, cflags, eflags);
        case RE_WORDB:
        case RE_NOT_WORDB: {
            int before = pos > 0 && re_is_word((unsigned char)text[pos - 1]);
            int after = pos < length && re_is_word((unsigned char)text[pos]);
            int boundary = before != after;
            if ((program[pc].op == RE_WORDB) != boundary) {
                return 0;
            }
            return re_run(program, program_length, pc + 1, text, length,
                          pos, captures, generations, generation, cflags, eflags);
        }
        case RE_MATCH:
            return 1;
        default:
            return 0;
    }
}

int regexec(const regex_t *preg, const char *string, unsigned long nmatch,
            regmatch_t pmatch[], int eflags) {
    re_inst *program;
    size_t program_length;
    size_t text_length;
    size_t capture_count;
    size_t state_count;
    int *captures = 0;
    unsigned int *generations = 0;
    unsigned int generation = 0;
    size_t start;
    int result = REG_NOMATCH;
    size_t i;

    if (!preg || !preg->re_program || !string) {
        return REG_BADPAT;
    }
    program = preg->re_program;
    program_length = 0;
    while (program[program_length].op != RE_MATCH) {
        program_length++;
    }
    program_length++;
    text_length = strlen(string);
    capture_count = (preg->re_nsub + 1) * 2;
    state_count = program_length * (text_length + 1);

    captures = malloc(capture_count * sizeof(*captures));
    generations = malloc(state_count * sizeof(*generations));
    if (!captures || !generations) {
        free(captures);
        free(generations);
        return REG_ESPACE;
    }

    for (start = 0; start <= text_length; start++) {
        generation++;
        if (generation == 0) {
            memset(generations, 0, state_count * sizeof(*generations));
            generation = 1;
        }
        for (i = 0; i < capture_count; i++) {
            captures[i] = -1;
        }
        captures[0] = (int)start;
        if (re_run(program, program_length, 0, string, text_length, start,
                   captures, generations, generation, preg->re_cflags, eflags)) {
            result = 0;
            break;
        }
    }

    if (result == 0 && !(preg->re_cflags & REG_NOSUB) && pmatch) {
        size_t count = (size_t)nmatch;
        if (count > preg->re_nsub + 1) {
            count = preg->re_nsub + 1;
        }
        for (i = 0; i < count; i++) {
            pmatch[i].rm_so = captures[i * 2];
            pmatch[i].rm_eo = captures[i * 2 + 1];
        }
    }

    free(captures);
    free(generations);
    return result;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    static const char *const messages[] = {
        "No error",
        "No match",
        "Invalid regular expression",
        "Invalid collating element",
        "Invalid character class",
        "Trailing backslash",
        "Invalid back reference",
        "Unmatched bracket",
        "Unmatched parenthesis",
        "Unmatched brace",
        "Invalid repeat range",
        "Invalid character range",
        "Out of memory"
    };
    const char *message;
    size_t length;

    (void)preg;
    if (errcode < 0 || (size_t)errcode >= sizeof(messages) / sizeof(messages[0])) {
        message = "Unknown regular expression error";
    } else {
        message = messages[errcode];
    }
    length = strlen(message) + 1;
    if (errbuf && errbuf_size > 0) {
        strncpy(errbuf, message, errbuf_size - 1);
        errbuf[errbuf_size - 1] = '\0';
    }
    return length;
}

void regfree(regex_t *preg) {
    if (!preg) {
        return;
    }
    free(preg->re_program);
    preg->re_program = 0;
    preg->re_nsub = 0;
    preg->re_cflags = 0;
}
