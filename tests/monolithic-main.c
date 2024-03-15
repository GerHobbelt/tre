
#include "monolithic_examples.h"

#define USAGE_NAME   "eternal"

#include "monolithic_main_internal_defs.h"

MONOLITHIC_CMD_TABLE_START()
	{ "agrep", { .fa = tre_agrep_main } },
	{ "bench", { .fa = tre_bench_test_main } },
	{ "randtest", { .fa = tre_randtest_main } },
	{ "retest", { .fa = tre_retest_main } },
	{ "test_str_source", { .fa = tre_test_str_source_main } },
    { "raw2str", {.fa = tre_rewrite_raw_strings_util_main } },
MONOLITHIC_CMD_TABLE_END();

#include "monolithic_main_tpl.h"
