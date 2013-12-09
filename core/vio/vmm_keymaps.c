/**
 * Copyright (c) 2013 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_keymaps.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief keysym to keycode conversion using keyboad mappings implementation
 *
 * The source has been largely adapted from QEMU sources:
 * ui/keymaps.c
 *
 * QEMU keysym to keycode conversion using rdesktop keymaps
 *
 * Copyright (c) 2004 Johannes Schindelin
 *
 * The original source is licensed under GPL.
 */

#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vio/vmm_keymaps.h>
#include <libs/stringlib.h>

struct vmm_keymap_file {
	const char name[32];
	const char *start;
	const unsigned long *size;
};

#define DECLARE_KEYMAP_FILE(__kf__) \
extern const char core_vio_keymaps_##__kf__##_data_start[]; \
extern const unsigned long core_vio_keymaps_##__kf__##_data_size;

#define KEYMAP_FILE(__kf__, __kf_name__) \
{ \
.name = __kf_name__, \
.start = core_vio_keymaps_##__kf__##_data_start, \
.size = &core_vio_keymaps_##__kf__##_data_size, \
}

DECLARE_KEYMAP_FILE(modifiers);
DECLARE_KEYMAP_FILE(common);
#ifdef CONFIG_VINPUT_KEYMAP_AR
DECLARE_KEYMAP_FILE(ar);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_BEPO
DECLARE_KEYMAP_FILE(bepo);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_CZ
DECLARE_KEYMAP_FILE(cz);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_DA
DECLARE_KEYMAP_FILE(da);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_DE_CH
DECLARE_KEYMAP_FILE(de_ch);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_DE
DECLARE_KEYMAP_FILE(de);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_EN_GB
DECLARE_KEYMAP_FILE(en_gb);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_EN_US
DECLARE_KEYMAP_FILE(en_us);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_ES
DECLARE_KEYMAP_FILE(es);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_ET
DECLARE_KEYMAP_FILE(et);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FI
DECLARE_KEYMAP_FILE(fi);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FO
DECLARE_KEYMAP_FILE(fo);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR_BE
DECLARE_KEYMAP_FILE(fr_be);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR_CA
DECLARE_KEYMAP_FILE(fr_ca);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR_CH
DECLARE_KEYMAP_FILE(fr_ch);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR
DECLARE_KEYMAP_FILE(fr);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_HR
DECLARE_KEYMAP_FILE(hr);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_HU
DECLARE_KEYMAP_FILE(hu);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_IS
DECLARE_KEYMAP_FILE(is);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_IT
DECLARE_KEYMAP_FILE(it);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_JA
DECLARE_KEYMAP_FILE(ja);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_LT
DECLARE_KEYMAP_FILE(lt);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_LV
DECLARE_KEYMAP_FILE(lv);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_MK
DECLARE_KEYMAP_FILE(mk);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_NL_BE
DECLARE_KEYMAP_FILE(nl_be);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_NL
DECLARE_KEYMAP_FILE(nl);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_NO
DECLARE_KEYMAP_FILE(no);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_PL
DECLARE_KEYMAP_FILE(pl);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_PT_BR
DECLARE_KEYMAP_FILE(pt_br);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_PT
DECLARE_KEYMAP_FILE(pt);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_RU
DECLARE_KEYMAP_FILE(ru);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_SL
DECLARE_KEYMAP_FILE(sl);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_SV
DECLARE_KEYMAP_FILE(sv);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_TH
DECLARE_KEYMAP_FILE(th);
#endif
#ifdef CONFIG_VINPUT_KEYMAP_TR
DECLARE_KEYMAP_FILE(tr);
#endif

static struct vmm_keymap_file keymap_files[] = {
	KEYMAP_FILE(modifiers, "modifiers"),
	KEYMAP_FILE(common, "common"),
#ifdef CONFIG_VINPUT_KEYMAP_AR
	KEYMAP_FILE(ar, "ar"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_BEPO
	KEYMAP_FILE(bepo, "bepo"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_CZ
	KEYMAP_FILE(cz, "cz"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_DA
	KEYMAP_FILE(da, "da"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_DE_CH
	KEYMAP_FILE(de_ch, "de-ch"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_DE
	KEYMAP_FILE(de, "de"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_EN_GB
	KEYMAP_FILE(en_gb, "en-gb"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_EN_US
	KEYMAP_FILE(en_us, "en-us"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_ES
	KEYMAP_FILE(es, "es"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_ET
	KEYMAP_FILE(et, "et"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FI
	KEYMAP_FILE(fi, "fi"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FO
	KEYMAP_FILE(fo, "fo"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR_BE
	KEYMAP_FILE(fr_be, "fr-be"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR_CA
	KEYMAP_FILE(fr_ca, "fr-ca"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR_CH
	KEYMAP_FILE(fr_ch, "fr-ch"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_FR
	KEYMAP_FILE(fr, "fr"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_HR
	KEYMAP_FILE(hr, "hr"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_HU
	KEYMAP_FILE(hu, "hu"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_IS
	KEYMAP_FILE(is, "is"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_IT
	KEYMAP_FILE(it, "it"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_JA
	KEYMAP_FILE(ja, "ja"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_LT
	KEYMAP_FILE(lt, "lt"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_LV
	KEYMAP_FILE(lv, "lv"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_MK
	KEYMAP_FILE(mk, "mk"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_NL_BE
	KEYMAP_FILE(nl_be, "nl-be"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_NL
	KEYMAP_FILE(nl, "nl"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_NO
	KEYMAP_FILE(no, "no"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_PL
	KEYMAP_FILE(pl, "pl"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_PT_BR
	KEYMAP_FILE(pt_br, "pt-br"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_PT
	KEYMAP_FILE(pt, "pt"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_RU
	KEYMAP_FILE(ru, "ru"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_SL
	KEYMAP_FILE(sl, "sl"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_SV
	KEYMAP_FILE(sv, "sv"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_TH
	KEYMAP_FILE(th, "th"),
#endif
#ifdef CONFIG_VINPUT_KEYMAP_TR
	KEYMAP_FILE(tr, "tr"),
#endif
};

static struct vmm_keymap_file *keymap_file_find(const char *name)
{
	int i;

	if (!name) {
		return NULL;
	}

	for (i = 0; i < array_size(keymap_files); i++) {
		if (!strcmp(keymap_files[i].name, name)) {
			return &keymap_files[i];
		}
	}

	return NULL;
}

static const char *keymap_file_first_pos(struct vmm_keymap_file *kf)
{
	return (kf) ? kf->start : NULL;
}

static bool keymap_file_end_of_line(const char c)
{
	switch (c) {
	case '\0':
	case '\r':
	case '\n':
		return TRUE;
	default:
		break;
	};

	return FALSE;
}

static const char *keymap_file_gets(struct vmm_keymap_file *kf,
				    const char *kf_pos, char *buf, u32 bufsz)
{
	u32 i;
	bool done;

	/* Sanity checks */
	if (!kf || !kf_pos || !buf) {
		return NULL;
	}
	if ((kf_pos < kf->start) || ((kf->start + *kf->size) <= kf_pos)) {
		return NULL;
	}

	/* Fill the buffer */
	i = 0;
	done = FALSE;
	while (!done && (kf_pos < (kf->start + *kf->size)) && (i < (bufsz - 1))) {
		if (keymap_file_end_of_line(*kf_pos)) {
			done = TRUE;
		} else {
			buf[i] = *kf_pos;
			i++;
		};
		kf_pos++;
	}
	buf[i] = '\0';

	/* If buffer was full before we hit end-of-line
	 * then ignore rest of the line
	 */
	if (!done && (i == (bufsz - 1))) {
		while (kf_pos < (kf->start + *kf->size)) {
			if (keymap_file_end_of_line(*kf_pos)) {
				break;
			}
			kf_pos++;
		}
	}

	return (kf_pos == (kf->start + *kf->size)) ? NULL : kf_pos;
}

static int get_keysym(const struct vmm_name2keysym *table,
		      const char *name)
{
	const struct vmm_name2keysym *p;

	for(p = table; p->name != NULL; p++) {
		if (!strcmp(p->name, name))
			return p->keysym;
	}

	if (name[0] == 'U' && strlen(name) == 5) { /* try unicode Uxxxx */
		char *end;
		int ret = (int)strtoul(name + 1, &end, 16);
		if (*end == '\0' && ret > 0)
			return ret;
	}

	return 0;
}

static void add_to_key_range(struct vmm_key_range **krp, int code)
{
	struct vmm_key_range *kr;

	for (kr = *krp; kr; kr = kr->next) {
		if (code >= kr->start && code <= kr->end)
			break;
		if (code == kr->start - 1) {
			kr->start--;
			break;
		}
		if (code == kr->end + 1) {
			kr->end++;
			break;
		}
	}

	if (kr == NULL) {
		kr = vmm_zalloc(sizeof(*kr));
		if (!kr) {
			vmm_printf("Warning: Could allocate key range"
				   " for code %d\n", code);
			return;
		}
		kr->start = kr->end = code;
		kr->next = *krp;
		*krp = kr;
	}
}

static void add_keysym(char *line, int keysym, int keycode,
			struct vmm_keymap_layout *k)
{
	if (keysym < VMM_MAX_NORMAL_KEYCODE) {
		k->keysym2keycode[keysym] = keycode;
	} else {
		if (k->extra_count >= VMM_MAX_EXTRA_COUNT) {
			vmm_printf("Warning: Could not assign keysym %s (0x%x)"
				   " because of memory constraints.\n",
				   line, keysym);
		} else {
			k->keysym2keycode_extra[k->extra_count].keysym = keysym;
			k->keysym2keycode_extra[k->extra_count].keycode = keycode;
			k->extra_count++;
		}
	}
}

static struct vmm_keymap_layout *parse_keyboard_layout(
					const struct vmm_name2keysym *table,
					const char *lang,
					struct vmm_keymap_layout *k)
{
	struct vmm_keymap_file *kf;
	const char *kf_pos;
	char line[1024];
	int len;

	kf = keymap_file_find(lang);
	if (!kf) {
		vmm_printf("Error: Could not read keymap file: '%s'\n", lang);
		return NULL;
	}
	kf_pos = keymap_file_first_pos(kf);

	if (!k) {
		k = vmm_zalloc(sizeof(struct vmm_keymap_layout));
		if (!k) {
			return NULL;
		}
	}

	for(;;) {
		kf_pos = keymap_file_gets(kf, kf_pos, line, sizeof(line));
		if (kf_pos == NULL)
			break;
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';
		if (line[0] == '#')
			continue;
		if (!strncmp(line, "map ", 4))
			continue;
		if (!strncmp(line, "include ", 8)) {
			parse_keyboard_layout(table, line + 8, k);
		} else {
			int keysym, keycode;
			const char *rest;
			char *end_of_keysym = line;
			while (*end_of_keysym != 0 && *end_of_keysym != ' ') {
				end_of_keysym++;
			}
			if (*end_of_keysym == 0) {
				continue;
			}
			*end_of_keysym = 0;
			keysym = get_keysym(table, line);
			if (keysym == 0) {
				continue;
			}
			rest = end_of_keysym + 1;
			keycode = strtol(rest, NULL, 0);
			if (strstr(rest, "numlock")) {
				add_to_key_range(&k->keypad_range, keycode);
				add_to_key_range(&k->numlock_range, keysym);
			}
			if (strstr(rest, "shift")) {
				keycode |= SCANCODE_SHIFT;
			}
			if (strstr(rest, "altgr")) {
				keycode |= SCANCODE_ALTGR;
			}
			if (strstr(rest, "ctrl")) {
				keycode |= SCANCODE_CTRL;
			}

			add_keysym(line, keysym, keycode, k);

			if (strstr(rest, "addupper")) {
				str2upper(line);
				keysym = get_keysym(table, line);
				if (keysym) {
					add_keysym(line, keysym,
						keycode | SCANCODE_SHIFT, k);
				}
			}
		}
	}

	return k;
}

struct vmm_keymap_layout *vmm_keymap_alloc_layout(
					const struct vmm_name2keysym *table,
					const char *lang)
{
	return parse_keyboard_layout(table, lang, NULL);
}
VMM_EXPORT_SYMBOL(vmm_keymap_alloc_layout);

void vmm_keymap_free_layout(struct vmm_keymap_layout *layout)
{
	struct vmm_keymap_layout *k = layout;
	struct vmm_key_range *kr, *next_kr;

	kr = k->keypad_range;
	while (kr) {
		next_kr = kr->next;
		vmm_free(kr);
		kr = next_kr;
	}

	kr = k->numlock_range;
	while (kr) {
		next_kr = kr->next;
		vmm_free(kr);
		kr = next_kr;
	}

	vmm_free(layout);
}
VMM_EXPORT_SYMBOL(vmm_keymap_free_layout);

int vmm_keysym2scancode(struct vmm_keymap_layout *layout, int keysym)
{
	struct vmm_keymap_layout *k = layout;

	if (keysym < VMM_MAX_NORMAL_KEYCODE) {
		if (k->keysym2keycode[keysym] == 0) {
			vmm_printf("Warning: no scancode found"
				   " for keysym %d\n", keysym);
		}
		return k->keysym2keycode[keysym];
	} else {
		int i;
#ifdef XK_ISO_Left_Tab
		if (keysym == XK_ISO_Left_Tab) {
			keysym = XK_Tab;
		}
#endif
		for (i = 0; i < k->extra_count; i++) {
			if (k->keysym2keycode_extra[i].keysym == keysym) {
				return k->keysym2keycode_extra[i].keycode;
			}
		}
	}

	return 0;
}
VMM_EXPORT_SYMBOL(vmm_keysym2scancode);

bool vmm_keycode_is_keypad(struct vmm_keymap_layout *layout, int keycode)
{
	struct vmm_keymap_layout *k = layout;
	struct vmm_key_range *kr;

	for (kr = k->keypad_range; kr; kr = kr->next) {
		if (keycode >= kr->start && keycode <= kr->end) {
			return TRUE;
		}
	}

	return FALSE;
}
VMM_EXPORT_SYMBOL(vmm_keycode_is_keypad);

bool vmm_keysym_is_numlock(struct vmm_keymap_layout *layout, int keysym)
{
	struct vmm_keymap_layout *k = layout;
	struct vmm_key_range *kr;

	for (kr = k->numlock_range; kr; kr = kr->next) {
		if (keysym >= kr->start && keysym <= kr->end) {
			return TRUE;
		}
	}

	return FALSE;
}
VMM_EXPORT_SYMBOL(vmm_keycode_is_numlock);

static const int vmm_vkey_defs[] = {
	[VMM_VKEY_SHIFT] = 0x2a,
	[VMM_VKEY_SHIFT_R] = 0x36,

	[VMM_VKEY_ALT] = 0x38,
	[VMM_VKEY_ALT_R] = 0xb8,
	[VMM_VKEY_ALTGR] = 0x64,
	[VMM_VKEY_ALTGR_R] = 0xe4,
	[VMM_VKEY_CTRL] = 0x1d,
	[VMM_VKEY_CTRL_R] = 0x9d,

	[VMM_VKEY_MENU] = 0xdd,

	[VMM_VKEY_ESC] = 0x01,

	[VMM_VKEY_1] = 0x02,
	[VMM_VKEY_2] = 0x03,
	[VMM_VKEY_3] = 0x04,
	[VMM_VKEY_4] = 0x05,
	[VMM_VKEY_5] = 0x06,
	[VMM_VKEY_6] = 0x07,
	[VMM_VKEY_7] = 0x08,
	[VMM_VKEY_8] = 0x09,
	[VMM_VKEY_9] = 0x0a,
	[VMM_VKEY_0] = 0x0b,
	[VMM_VKEY_MINUS] = 0x0c,
	[VMM_VKEY_EQUAL] = 0x0d,
	[VMM_VKEY_BACKSPACE] = 0x0e,

	[VMM_VKEY_TAB] = 0x0f,
	[VMM_VKEY_Q] = 0x10,
	[VMM_VKEY_W] = 0x11,
	[VMM_VKEY_E] = 0x12,
	[VMM_VKEY_R] = 0x13,
	[VMM_VKEY_T] = 0x14,
	[VMM_VKEY_Y] = 0x15,
	[VMM_VKEY_U] = 0x16,
	[VMM_VKEY_I] = 0x17,
	[VMM_VKEY_O] = 0x18,
	[VMM_VKEY_P] = 0x19,
	[VMM_VKEY_BRACKET_LEFT] = 0x1a,
	[VMM_VKEY_BRACKET_RIGHT] = 0x1b,
	[VMM_VKEY_RET] = 0x1c,

	[VMM_VKEY_A] = 0x1e,
	[VMM_VKEY_S] = 0x1f,
	[VMM_VKEY_D] = 0x20,
	[VMM_VKEY_F] = 0x21,
	[VMM_VKEY_G] = 0x22,
	[VMM_VKEY_H] = 0x23,
	[VMM_VKEY_J] = 0x24,
	[VMM_VKEY_K] = 0x25,
	[VMM_VKEY_L] = 0x26,
	[VMM_VKEY_SEMICOLON] = 0x27,
	[VMM_VKEY_APOSTROPHE] = 0x28,
	[VMM_VKEY_GRAVE_ACCENT] = 0x29,

	[VMM_VKEY_BACKSLASH] = 0x2b,
	[VMM_VKEY_Z] = 0x2c,
	[VMM_VKEY_X] = 0x2d,
	[VMM_VKEY_C] = 0x2e,
	[VMM_VKEY_V] = 0x2f,
	[VMM_VKEY_B] = 0x30,
	[VMM_VKEY_N] = 0x31,
	[VMM_VKEY_M] = 0x32,
	[VMM_VKEY_COMMA] = 0x33,
	[VMM_VKEY_DOT] = 0x34,
	[VMM_VKEY_SLASH] = 0x35,

	[VMM_VKEY_ASTERISK] = 0x37,

	[VMM_VKEY_SPC] = 0x39,
	[VMM_VKEY_CAPS_LOCK] = 0x3a,
	[VMM_VKEY_F1] = 0x3b,
	[VMM_VKEY_F2] = 0x3c,
	[VMM_VKEY_F3] = 0x3d,
	[VMM_VKEY_F4] = 0x3e,
	[VMM_VKEY_F5] = 0x3f,
	[VMM_VKEY_F6] = 0x40,
	[VMM_VKEY_F7] = 0x41,
	[VMM_VKEY_F8] = 0x42,
	[VMM_VKEY_F9] = 0x43,
	[VMM_VKEY_F10] = 0x44,
	[VMM_VKEY_NUM_LOCK] = 0x45,
	[VMM_VKEY_SCROLL_LOCK] = 0x46,

	[VMM_VKEY_KP_DIVIDE] = 0xb5,
	[VMM_VKEY_KP_MULTIPLY] = 0x37,
	[VMM_VKEY_KP_SUBTRACT] = 0x4a,
	[VMM_VKEY_KP_ADD] = 0x4e,
	[VMM_VKEY_KP_ENTER] = 0x9c,
	[VMM_VKEY_KP_DECIMAL] = 0x53,
	[VMM_VKEY_SYSRQ] = 0x54,

	[VMM_VKEY_KP_0] = 0x52,
	[VMM_VKEY_KP_1] = 0x4f,
	[VMM_VKEY_KP_2] = 0x50,
	[VMM_VKEY_KP_3] = 0x51,
	[VMM_VKEY_KP_4] = 0x4b,
	[VMM_VKEY_KP_5] = 0x4c,
	[VMM_VKEY_KP_6] = 0x4d,
	[VMM_VKEY_KP_7] = 0x47,
	[VMM_VKEY_KP_8] = 0x48,
	[VMM_VKEY_KP_9] = 0x49,

	[VMM_VKEY_LESS] = 0x56,

	[VMM_VKEY_F11] = 0x57,
	[VMM_VKEY_F12] = 0x58,

	[VMM_VKEY_PRINT] = 0xb7,

	[VMM_VKEY_HOME] = 0xc7,
	[VMM_VKEY_PGUP] = 0xc9,
	[VMM_VKEY_PGDN] = 0xd1,
	[VMM_VKEY_END] = 0xcf,

	[VMM_VKEY_LEFT] = 0xcb,
	[VMM_VKEY_UP] = 0xc8,
	[VMM_VKEY_DOWN] = 0xd0,
	[VMM_VKEY_RIGHT] = 0xcd,

	[VMM_VKEY_INSERT] = 0xd2,
	[VMM_VKEY_DELETE] = 0xd3,
	[VMM_VKEY_MAX] = 0,
};

static const char *vmm_vkey_lookup[] = {
	"shift",
	"shift_r",
	"alt",
	"alt_r",
	"altgr",
	"altgr_r",
	"ctrl",
	"ctrl_r",
	"menu",
	"esc",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"0",
	"minus",
	"equal",
	"backspace",
	"tab",
	"q",
	"w",
	"e",
	"r",
	"t",
	"y",
	"u",
	"i",
	"o",
	"p",
	"bracket_left",
	"bracket_right",
	"ret",
	"a",
	"s",
	"d",
	"f",
	"g",
	"h",
	"j",
	"k",
	"l",
	"semicolon",
	"apostrophe",
	"grave_accent",
	"backslash",
	"z",
	"x",
	"c",
	"v",
	"b",
	"n",
	"m",
	"comma",
	"dot",
	"slash",
	"asterisk",
	"spc",
	"caps_lock",
	"f1",
	"f2",
	"f3",
	"f4",
	"f5",
	"f6",
	"f7",
	"f8",
	"f9",
	"f10",
	"num_lock",
	"scroll_lock",
	"kp_divide",
	"kp_multiply",
	"kp_subtract",
	"kp_add",
	"kp_enter",
	"kp_decimal",
	"sysrq",
	"kp_0",
	"kp_1",
	"kp_2",
	"kp_3",
	"kp_4",
	"kp_5",
	"kp_6",
	"kp_7",
	"kp_8",
	"kp_9",
	"less",
	"f11",
	"f12",
	"print",
	"home",
	"pgup",
	"pgdn",
	"end",
	"left",
	"up",
	"down",
	"right",
	"insert",
	"delete",
	"stop",
	"again",
	"props",
	"undo",
	"front",
	"copy",
	"open",
	"paste",
	"find",
	"cut",
	"lf",
	"help",
	"meta_l",
	"meta_r",
	"compose",
	NULL,
};

int vmm_keyname2vkey(const char *key)
{
	int i;

	for (i = 0; vmm_vkey_lookup[i] != NULL; i++) {
		if (!strcmp(key, vmm_vkey_lookup[i])) {
			break;
		}
	}

	/* Return VMM_VKEY_MAX if the key is invalid */
	return i;
}
VMM_EXPORT_SYMBOL(vmm_keyname2vkey);

int vmm_keycode2vkey(int keycode)
{
	int i;

	for (i = 0; i < VMM_VKEY_MAX; i++) {
		if (vmm_vkey_defs[i] == keycode) {
			break;
		}
	}

	/* Return VMM_VKEY_MAX if the code is invalid */
	return i;
}
VMM_EXPORT_SYMBOL(vmm_keycode2vkey);

int vmm_vkey2keycode(int vkey)
{
	if ((-1 < vkey) && (vkey < VMM_VKEY_MAX)) {
		return vmm_vkey_defs[vkey];
	}
	return 0;
}
VMM_EXPORT_SYMBOL(vmm_vkey2keycode);
