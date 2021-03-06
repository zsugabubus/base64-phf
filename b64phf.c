/* Generate magic constants for https://github.com/aqrit/base64. */
/* WTFPL */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (1) Want colorful shittery? */
/* #define PRINT_TBL */

enum {
	DELTA = 1,
	CHECK = 2,
	/* In case of a more sophisticated calc_asso(). */
	BOTH = DELTA | CHECK,
};

/* (2) Generate what? */
int const what = DELTA;

/* (3) Run! */

uint8_t tbl[128];

uint8_t asso_runs[17];
uint8_t asso[16];
int nth_asso = 0;

/* Used both for delta and check. */
uint8_t calc_asso(char c)
{
	return ((c >> 3) + asso[c % 16] + 1) / 2;
}

typedef struct {
	char base;
	int index;
} I;
I b64_pos[10];

int chr_asso(char c)
{
	if ('A' <= c && c <= 'Z')
		return 1;
	else if ('a' <= c && c <= 'z')
		return 2;
	else if ('0' <= c && c <= '9')
		return 3;
	else if ('+' == c)
		return 4;
	/* else if (',' == c)
		return 4; */
	/* else if ('-' == c)
		return 5; */
	else if ('/' == c)
		return 6;
	/* else if ('_' == c)
		return 7; */

	return 0;
}

uint64_t last = 0;
uint64_t cur = 0;
uint64_t tot = 1;

void init_runs() {
	uint64_t skips[17];
	memset(skips, 0, sizeof skips);

	for (uint8_t c = 0; c < 128; ++c)
		skips[c % 16] |= !!chr_asso(c) << (c / 16);

	for (int i = 16; i-- > 0;) {
		asso_runs[i] = 1 + (skips[i] == skips[i + 1] ? asso_runs[i + 1] : 0);
		if (1 == asso_runs[i])
			tot *= 16;
	}

	for (int i = 0; i < 16; ++i)
		fprintf(stderr, "%x", i);
	fputc('\n', stderr);
	for (int i = 0; i < 16; ++i)
		fputc(1 < asso_runs[i] ? '>' : '|', stderr);
	fputc('\n', stderr);
}

void validate()
{
	uint8_t p[16];
	memset(p, UINT8_MAX, sizeof p);

	int ret = 1;

	for (uint8_t c = 0; c < 128;) {
		tbl[c] = calc_asso(c);

		int v = chr_asso(c);

		if (DELTA & what) {
			if (v) {
				if (UINT8_MAX != p[tbl[c]])
					ret &= p[tbl[c]] == v;
				else
					p[tbl[c]] = v;
			}
		}

		if (CHECK & what) {
			if (v) {
				/* Find earliest. */
				if (c <= p[tbl[c]])
					p[tbl[c]] = c;
			} else {
				ret &= c <= p[tbl[c]];
			}
		}

#ifdef PRINT_TBL
		printf("\e[%sm%x\e[m", ret ? (v ? "1;7" : "") : "31",
#if 1
				tbl[c]
#else
				v
#endif
				);
#endif

		++c;
#ifdef PRINT_TBL
		if (!(c % 16))
			fputc('\n', stdout);
#endif
	}

#ifdef PRINT_TBL
	fputc('\n', stdout);
#endif

	if (ret) {
		printf("/**\n");
		printf(" * asso(c) :=\n");
		for (uint8_t y = 0; y < 8; ++y) {
			for (int i = 0; i < 3; ++i) {
				printf(" *  ");
				for (uint8_t x = 0; x < 16; ++x) {
					char c = y * 16 + x;
					switch (i) {
					case 0: printf(" %c", isprint(c) ? c : ' '); break;
					case 1: printf(" %x", tbl[c]); break;
					case 2: printf("  "); break;
					}
				}
				printf("\n");
			}
		}
		printf(" */\n");
		printf("__m128i const %s%sasso_%d = _mm_setr_epi8(",
				DELTA & what ? "delta_" : "",
				CHECK & what ? "check_" : "",
				nth_asso);
		for (int i = 0; i < 16; ++i) {
			if (i)
				printf(", ");
			printf("0x%02x", asso[i]);
		}
		printf(");\n");

		if (DELTA & what) {
			memset(p, UINT8_MAX, sizeof p);
			for (uint8_t c = 0; c < 128; ++c) {
				uint8_t i = chr_asso(c);
				if (i && UINT8_MAX == p[tbl[c]])
					p[tbl[c]] = i;
			}

			printf("__m128i const delta_values_%d = _mm_setr_epi8(",
					nth_asso);
			for (int i = 0; i < 16; ++i) {
				if (i)
					printf(", ");
				if (UINT8_MAX != p[i])
					printf("%d - '%c'", b64_pos[p[i]].index, b64_pos[p[i]].base);
				else
					printf("%d", 0);
			}
			printf(");\n");
		}

		if (CHECK & what) {
			printf("__m128i const check_values_%d = _mm_setr_epi8(",
					nth_asso);
			for (int i = 0; i < 16; ++i) {
				if (i)
					printf(", ");
				if (UINT8_MAX != p[i])
					printf("-'%c'", p[i]);
				else
					printf("INT8_MIN");
			}
			printf(");\n");
		}

		++nth_asso;
	}
}

void generate(uint8_t i)
{
	if (16 <= i) {
		++cur;

		int pct = cur * 100 / tot;
		if (last != pct)
			fprintf(stderr, "/* Progress: %3d%%. */\n", (last = pct));
		validate();
		return;
	}

	for (uint8_t v = 0; v < 16; ++v) {
		uint8_t run_len = asso_runs[i];
		memset(asso + i, v, run_len);
		generate(i + run_len);
	}
}

int main()
{
	b64_pos[chr_asso('A')] = (I){ 'A', 0 };
	b64_pos[chr_asso('a')] = (I){ 'a', 26 };
	b64_pos[chr_asso('0')] = (I){ '0', 26 + 26 };
	b64_pos[chr_asso('+')] = (I){ '+', 26 + 26 + 10 };
	b64_pos[chr_asso('-')] = (I){ '-', 26 + 26 + 10 };
	b64_pos[chr_asso('/')] = (I){ '/', 26 + 26 + 10 + 1 };
	b64_pos[chr_asso('_')] = (I){ '_', 26 + 26 + 10 + 1 };

	init_runs();
	generate(0);
}

/* Thanks for scrolling down here. (I know the code is trash, yes). */
