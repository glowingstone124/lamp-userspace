#include <regex.h>
#include <stdio.h>
#include <string.h>

static int check(const char *pattern, int cflags, const char *text,
                 int should_match, int so, int eo) {
    regex_t re;
    regmatch_t match[3];
    int status = regcomp(&re, pattern, cflags);
    if (status != 0) {
        char message[128];
        regerror(status, &re, message, sizeof(message));
        printf("FAIL comp %s: %s\n", pattern, message);
        return 1;
    }
    memset(match, 0xff, sizeof(match));
    status = regexec(&re, text, 3, match, 0);
    if (status == REG_NOMATCH && !should_match) {
        regfree(&re);
        return 0;
    }
    if (status == 0 && should_match && match[0].rm_so == so && match[0].rm_eo == eo) {
        regfree(&re);
        return 0;
    }
    printf("FAIL exec /%s/ flags=%d text=\"%s\": status=%d", pattern, cflags, text, status);
    if (status == 0) {
        printf(" match=[%d,%d]", match[0].rm_so, match[0].rm_eo);
    }
    putchar('\n');
    regfree(&re);
    return 1;
}

int main(void) {
    int failures = 0;
    failures += check("abc", 0, "xxabcyy", 1, 2, 5);
    failures += check("a.c", 0, "xxabcyy", 1, 2, 5);
    failures += check("a*", 0, "aaa", 1, 0, 3);
    failures += check("ab+c", 0, "abbbc", 1, 0, 5);
    failures += check("ab?c", 0, "ac", 1, 0, 2);
    failures += check("cat|dog", 0, "hotdog", 1, 3, 6);
    failures += check("\\(ab\\)+", 0, "xxababyy", 1, 2, 6);
    failures += check("(ab)+", REG_EXTENDED, "xxababyy", 1, 2, 6);
    failures += check("[0-9]+", 0, "abc123", 1, 3, 6);
    failures += check("[^0-9]+", 0, "123abc", 1, 3, 6);
    failures += check("^abc", 0, "xxabc", 0, 0, 0);
    failures += check("^abc", 0, "abc", 1, 0, 3);
    failures += check("abc$", 0, "abcxx", 0, 0, 0);
    failures += check("abc$", 0, "xxabc", 1, 2, 5);
    failures += check("ABC", REG_ICASE, "xxabc", 1, 2, 5);
    failures += check("a{2,3}", 0, "xaaaa", 1, 1, 4);
    failures += check("a{2}", 0, "xaaa", 1, 1, 3);
    failures += check("empty", 0, "empty", 1, 0, 5);
    failures += check("", 0, "anything", 1, 0, 0);
    failures += check("\\bword\\b", 0, "a word here", 1, 2, 6);
    failures += check("no-match", 0, "different", 0, 0, 0);
    printf("%s\n", failures ? "regex tests failed" : "regex tests passed");
    return failures != 0;
}
