#include "console.h"
#include <string.h>
static bool number(const char **p, uint16_t *out)
{
    uint32_t n=0;unsigned digits=0;
    while (**p>='0' && **p<='9') { n=n*10U+(unsigned)(*(*p)++-'0');if (++digits>5 || n>65535) return false; }
    if (!digits) return false;
    *out=(uint16_t)n;return true;
}
BenchCommand console_feed(Console *c, char ch)
{
    if (ch=='\r') return CMD_NONE;
    if (ch!='\n') {
        if (c->discard) return CMD_NONE;
        if (c->used==sizeof(c->text)-1) { c->discard=true;return CMD_NONE; }
        c->text[c->used++]=ch;return CMD_NONE;
    }
    c->text[c->used]=0;c->last_length=c->used;c->used=0;
    if (c->discard) { c->discard=false;++c->invalid;return CMD_BAD; }
    const char *t=c->text;
    static const char *const names[]={"STATUS","STATS","CAL","SAVE","LOAD","POT","","PROBE","","EXPORT","RESET","VERSION"};
    for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)
        if (*names[i] && !strcmp(t,names[i])) return (BenchCommand)(i+1);
    if (!strncmp(t,"LIMIT ",6)) {
        const char *p=t+6;
        if (number(&p,&c->x) && *p++==' ' && number(&p,&c->y) && !*p &&
            c->x>=500 && c->x<=5000 && c->y>=500 && c->y<=5000) return CMD_LIMIT;
    }
    if (!strncmp(t,"PERIOD ",7)) {
        const char *p=t+7;
        if (number(&p,&c->x) && !*p && c->x>=2 && c->x<=100) return CMD_PERIOD;
    }
    ++c->invalid;return CMD_BAD;
}
