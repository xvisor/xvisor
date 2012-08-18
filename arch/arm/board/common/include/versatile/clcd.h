#ifndef VERSATILE_CLCD_H
#define VERSATILE_CLCD_H

struct clcd_panel *versatile_clcd_get_panel(const char *);
int versatile_clcd_setup(struct clcd_fb *, unsigned long);
void versatile_clcd_remove(struct clcd_fb *);

#endif
