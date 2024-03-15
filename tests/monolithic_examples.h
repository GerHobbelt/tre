
#pragma once

#if defined(BUILD_MONOLITHIC)

#ifdef __cplusplus
extern "C" {
#endif

extern int tre_agrep_main(int argc, const char** argv);
extern int tre_bench_test_main(int argc, const char** argv);
extern int tre_randtest_main(int argc, const char** argv);
extern int tre_retest_main(int argc, const char** argv);
extern int tre_test_str_source_main(int argc, const char** argv);
extern int tre_rewrite_raw_strings_util_main(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif
