#ifndef CPU_USAGE
#define CPU_USAGE

/*
 * Tiny library to read cpu usage per core in Linux
 *
 * by Hugo Coto (https://github.com/hugoocoto),
 * placed in the public domain.
 */

typedef enum Cpu_Id {
        // clang-format off
        CPU_ALL = -1, CPU_0 = 0, 
        CPU_1, CPU_2, CPU_3, CPU_4, CPU_5, CPU_6, CPU_7, CPU_8, CPU_9, CPU_10,
        CPU_11, CPU_12, CPU_13, CPU_14, CPU_15, CPU_16, CPU_17, CPU_18, CPU_19,
        CPU_20, CPU_21, CPU_22, CPU_23, CPU_24, CPU_25, CPU_26, CPU_27, CPU_28,
        // clang-format on
} Cpu_Id;

/*
 * read-only struct
 */
typedef struct Cpu_Perf_Slice {
        // lower level
        unsigned long long use, nic, sys, idl, iow, irq, softirq, steal;
        // higher level
        unsigned long long idle;
        unsigned long long non_idle;
        unsigned long long total;

        Cpu_Id id;              // cpu number or ALL
        unsigned int valid : 1; // sanity check
} Cpu_Perf_Slice;

/*
 * The right way to use this lib is:
 *
 * 1. Call get_cpu_usage to get the usage in this moment
 * 2. Sleep for a fixed amount of time (i.e., 1s)
 * 3. Call get_cpu_usage again
 * 4. Calc the difference to get the usage in a second with calc_cpu_usage.
 *
 * Don't forget to define INCLUDE_CPU_USAGE_IMPLEMENTATION before including the
 * header once and just once.
 */

int get_cpu_usage(Cpu_Id id, Cpu_Perf_Slice *slice);         // Get the cpu usage since a fixed moment in the past
double calc_cpu_usage(Cpu_Perf_Slice t0, Cpu_Perf_Slice t1); // Get the difference between two usage-slices

#endif // !CPU_USAGE
#if defined(INCLUDE_CPU_USAGE_IMPLEMENTATION)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
get_cpu_usage(Cpu_Id id, Cpu_Perf_Slice *slice)
{
        FILE *fp;
        char *fmt = NULL;

        if (slice == NULL) goto err;
        memset(slice, 0, sizeof(Cpu_Perf_Slice));

        if ((fp = fopen("/proc/stat", "r")) == NULL) {
                perror("Failed to open /proc/stat");
                return -1;
        }

        char buf[1024] = { 0 };
        for (int i = 0; i <= id; i++) {
                if (fgets(buf, sizeof buf - 1, fp) == NULL) {
                        perror("Failed to read line");
                        goto err;
                }

                // just the first N bytes of the line was read
                if (buf[strlen(buf) - 1] != '\n') --i;
        }

        if (id == CPU_ALL) {
                if (asprintf(&fmt, "cpu %%llu %%llu %%llu %%llu %%llu %%llu %%llu %%llu") < 0) {
                        perror("Failed to build parsing format");
                        goto err;
                }
        } else {
                if (asprintf(&fmt, "cpu%d %%llu %%llu %%llu %%llu %%llu %%llu %%llu %%llu", id) < 0) {
                        perror("Failed to build parsing format");
                        goto err;
                }
        }

        if (fmt == NULL) {
                perror("Parsing format not stored");
                goto err;
        }

        if (fscanf(fp, fmt, &slice->use, &slice->nic, &slice->sys, &slice->idl,
                   &slice->iow, &slice->irq, &slice->softirq, &slice->steal) != 8) {
                perror("Failed to parse cpu stat");
                goto err;
        }

        slice->idle     = slice->idl + slice->iow;
        slice->non_idle = slice->use + slice->nic + slice->sys + slice->irq + slice->softirq + slice->steal;
        slice->total    = slice->idle + slice->non_idle;
        slice->valid    = 1u;
        slice->id       = id;

        if (fmt) free(fmt);
        return 0;
err:
        if (fmt) free(fmt);
        return -1;
}

double
calc_cpu_usage(Cpu_Perf_Slice t0, Cpu_Perf_Slice t1)
{
        unsigned long long diff_total = t1.total - t0.total;
        unsigned long long diff_idle  = t1.idle - t0.idle;

        if (!t0.valid || !t1.valid || diff_total <= 0 || t0.id != t1.id) {
                return -1.0;
        }

        return ((double) (diff_total - diff_idle) / diff_total) * 100.0;
}

#endif

#if defined(MAIN_TEST)

// EXAMPLE

#include <assert.h>
#include <unistd.h>

int
main()
{
        Cpu_Perf_Slice prev;
        Cpu_Perf_Slice curr;
        Cpu_Id id = CPU_ALL;
        double usage;

        assert(get_cpu_usage(id, &prev) >= 0);

        while (1) {
                sleep(1);
                assert(get_cpu_usage(id, &curr) >= 0);
                usage = calc_cpu_usage(prev, curr);
                printf("Cpu usage: %lf\n", usage);
                prev = curr;
        }
        return 0;
}
#endif


/*
 * LICENSE - Public Domain (www.unlicense.org)
 * This is free and unencumbered software released into the public domain.
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 * software, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 * In jurisdictions that recognize copyright laws, the author or authors of this
 * software dedicate any and all copyright interest in the software to the public
 * domain. We make this dedication for the benefit of the public at large and to
 * the detriment of our heirs and successors. We intend this dedication to be an
 * overt act of relinquishment in perpetuity of all present and future rights to
 * this software under copyright law.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
