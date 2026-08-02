#ifndef CPU_USAGE
#define CPU_USAGE

/* cpu_usage.h - v1.0 - public domain cpu usage reader - Hugo Coto Florez 2026

   To use this library, do this in *one* C file:
      #define INCLUDE_CPU_USAGE_IMPLEMENTATION
      #include "cpu_usage.h"

   In other source files, just include the header normally:
      #include "cpu_usage.h"

TABLE OF CONTENTS

  Table of Contents
  License
  Documentation
  Notes
  Credits

LICENSE

  Placed in the public domain.
  See end of file for detailed license information.

DOCUMENTATION

  The library reads CPU usage counters from /proc/stat (per core and the
  system-wide total) and from /proc/<pid>/stat (per process, or self).
  Counters are kept in read-only snapshot structs, Cpu_Perf_Slice for a
  core and Cpu_Proc_Slice for a process.

  The right way to use this lib is:

    1. Call get_cpu_usage / get_proc_cpu_usage to snapshot the usage now
    2. Sleep for a fixed amount of time (i.e., 1s)
    3. Call them again
    4. Compute the difference with calc_cpu_usage / calc_proc_cpu_usage

  All counters are in clock ticks (USER_HZ, typically 100 per second), and
  both /proc/stat and /proc/<pid>/stat use the same unit, so the deltas
  and ratios are directly valid.

  Return values: every get_* function returns 0 on success and -1 on
  failure; every calc_* function returns the usage percentage on success
  and -1.0 when the inputs are invalid.

  Functions:

    get_cpu_usage
      Snapshots the counters of the given Cpu_Id (CPU_ALL for the whole
      machine, CPU_0..CPU_28 for one core; use CPU_N(n) for cores beyond
      the enum). Returns 0 on success, -1 on failure (e.g. no such core).

    get_cpu_count
      Returns the number of CPUs the process may run on, via
      sysconf(_SC_NPROCESSORS_ONLN). Returns -1 on error.

    calc_cpu_usage
      Computes the usage percentage between two snapshots of the same
      core: (total1 - idle1) - (total0 - idle0) over the total delta.
      Returns -1.0 if the slices are invalid or come from different cores.

    get_proc_cpu_usage
      Snapshots the counters of the process with the given pid. A pid of
      0 or less reads /proc/self/stat instead. Returns 0 on success, -1 on
      failure (e.g. the process does not exist).

    get_thread_cpu_usage
      Snapshots the counters of the thread with the given tid, owned by
      the process with the given pid. A pid of 0 or less reads
      /proc/self/task/<tid>/stat instead. The slice's pid field holds the
      tid, so thread snapshots can be fed to the calc_proc_* functions
      with the usual validation.

    calc_proc_cpu_usage
      Computes the percentage of the machine's CPU time used by a process
      between two snapshots: (p1.total - p0.total) /
      (sys1.total - sys0.total). Pass the Cpu_Id whose total is the
      denominator (CPU_ALL for the whole machine, CPU_x for one core).
      Returns -1.0 if the slices are invalid, the process died between
      samples, or the ids do not match.

    calc_proc_cpu_usage_children
      Same as calc_proc_cpu_usage, but also counts the CPU time of the
      process's children (cutime + cstime, fields 16-17).

    calc_proc_cpu_usage_per_core
      Same as calc_proc_cpu_usage, but scaled to per-core percentages
      (top/htop style): one full core reads 100%, the ceiling is
      100 * ncpus %. The factor comes from get_cpu_count() and requires
      id == CPU_ALL; returns -1.0 otherwise.

    calc_proc_cpu_usage_children_per_core
      Same as calc_proc_cpu_usage_children, scaled to per-core
      percentages as described above.

  Usage:

    Cpu_Perf_Slice sys0, sys1;
    Cpu_Proc_Slice p0, p1;

    get_cpu_usage(CPU_ALL, &sys0);
    get_proc_cpu_usage(0, &p0); // 0 -> self
    sleep(1);
    get_cpu_usage(CPU_ALL, &sys1);
    get_proc_cpu_usage(0, &p1);

    double total = calc_cpu_usage(sys0, sys1);
    double proc  = calc_proc_cpu_usage(CPU_ALL, p0, p1, sys0, sys1);

NOTES

  * Percentages are relative to the total CPU time of the sampled period,
    not to a single core. A process pegging one full core reads roughly
    100/ncpus % with CPU_ALL; pass a per-core Cpu_Id for that core's share.
  * Process counters only advance while the process is running (not while
    it sleeps), so an idle process reads ~0%.
  * cutime/cstime may be negative after children are reaped; they are
    clamped to 0 when snapshotted.
  * cutime/cstime only count waited-for children: a live child's CPU time
    appears in the parent's slice only after the child exits and the
    parent waitpid()s it.
  * A process that dies between snapshots makes the calc functions return
    -1.0.
  * The comm field of /proc/<pid>/stat may contain spaces and parentheses;
    parsing always starts at the last ')'.
  * Thread snapshots (get_thread_cpu_usage) store the tid in the slice's
    pid field, so p0.pid != p1.pid validation applies to tids too.
  * Percentages are relative to the machine's total CPU time as seen in
    /proc/stat; inside a container they are host-relative (like top).
  * get_cpu_count() returns the CPUs the process may run on: glibc's
    sysconf(_SC_NPROCESSORS_ONLN) respects cpuset/cgroup restrictions, so
    it can be smaller than the core range present in /proc/stat.
  * Quota-limited containers (--cpus): usage is still reported against the
    host; quota-relative usage would require reading /sys/fs/cgroup/cpu.max.
  * The *_per_core functions scale the machine-wide percentage by
    get_cpu_count(), so one full core reads 100% and the ceiling is
    100 * ncpus %. They require id == CPU_ALL and inherit get_cpu_count()'s
    cpuset caveat.
  * This library is stateless and keeps no globals between calls.

CREDITS

  Hugo Coto Florez -- design and implementation

*/

typedef enum Cpu_Id {
        // clang-format off
        CPU_ALL = -1, CPU_0 = 0, 
        CPU_1, CPU_2, CPU_3, CPU_4, CPU_5, CPU_6, CPU_7, CPU_8, CPU_9, CPU_10,
        CPU_11, CPU_12, CPU_13, CPU_14, CPU_15, CPU_16, CPU_17, CPU_18, CPU_19,
        CPU_20, CPU_21, CPU_22, CPU_23, CPU_24, CPU_25, CPU_26, CPU_27, CPU_28,
        // clang-format on
} Cpu_Id;

#define CPU_N(n) ((Cpu_Id) (n)) // any core by number, not just CPU_0..CPU_28

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
 * read-only struct
 */
typedef struct Cpu_Proc_Slice {
        // lower level
        unsigned long long utime, stime;   // fields 14, 15 (ticks)
        unsigned long long cutime, cstime; // fields 16, 17 (children, clamped)
        // higher level
        unsigned long long total;          // utime + stime
        unsigned long long total_children; // total + cutime + cstime
        long pid;
        unsigned int valid : 1; // sanity check
} Cpu_Proc_Slice;

int get_cpu_usage(Cpu_Id id, Cpu_Perf_Slice *slice);                                                                            // Get the cpu usage since a fixed moment in the past
double calc_cpu_usage(Cpu_Perf_Slice t0, Cpu_Perf_Slice t1);                                                                    // Get the difference between two usage-slices
int get_proc_cpu_usage(long pid, Cpu_Proc_Slice *slice);                                                                        // Snapshot the counters of a process (pid <= 0 -> self)
int get_thread_cpu_usage(long pid, long tid, Cpu_Proc_Slice *slice);                                                            // Snapshot the counters of a thread (pid <= 0 -> self)
int get_cpu_count(void);                                                                                                        // Number of cores the process may run on (-1 on error)
double calc_proc_cpu_usage(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1, Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1);          // Process usage % against the given Cpu_Id total
double calc_proc_cpu_usage_children(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1, Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1); // Same, including children (cutime + cstime)
double calc_proc_cpu_usage_per_core(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1, Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1); // Top-style %, one core = 100%, up to 100*ncpus
double calc_proc_cpu_usage_children_per_core(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1, Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1); // Same, including children (per core)

#endif // !CPU_USAGE
#if defined(INCLUDE_CPU_USAGE_IMPLEMENTATION)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int
get_cpu_usage(Cpu_Id id, Cpu_Perf_Slice *slice)
{
        FILE *fp;
        char *fmt = NULL;

        if (slice == NULL) goto err;
        memset(slice, 0, sizeof(Cpu_Perf_Slice));

        if (id != CPU_ALL && id < 0) {
                fprintf(stderr, "Invalid cpu id: %d\n", id);
                return -1;
        }

        if ((fp = fopen("/proc/stat", "r")) == NULL) {
                perror("Failed to open /proc/stat");
                return -1;
        }

        char buf[1024] = { 0 };
        for (int i = 0; i <= id; i++) {
                if (fgets(buf, sizeof buf - 1, fp) == NULL) {
                        if (ferror(fp)) {
                                perror("Failed to read /proc/stat");
                        } else {
                                fprintf(stderr, "No such cpu: %d\n", id);
                        }
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

static int
read_proc_stat(const char *path, long id, Cpu_Proc_Slice *slice)
{
        FILE *fp;
        char buf[2048] = { 0 };
        char *p;
        char state;
        int ppid, pgrp, session, tty_nr, tpgid;
        unsigned int flags;
        unsigned long minflt, cminflt, majflt, cmajflt;
        unsigned long utime, stime; // kernel prints %lu
        long cutime, cstime;        // kernel prints %ld, may be negative

        if (slice == NULL) return -1;
        memset(slice, 0, sizeof(Cpu_Proc_Slice));
        slice->pid = id;

        if ((fp = fopen(path, "r")) == NULL) {
                perror("Failed to open proc stat");
                return -1;
        }

        if (fgets(buf, sizeof buf - 1, fp) == NULL) {
                perror("Failed to read line");
                fclose(fp);
                return -1;
        }
        fclose(fp);

        // comm (field 2) may contain spaces and parens: parse from last ')'
        if ((p = strrchr(buf, ')')) == NULL) return -1;

        // fields 3..17: state ppid pgrp session tty_nr tpgid flags
        //               minflt cminflt majflt cmajflt utime stime cutime cstime
        if (sscanf(p + 1,
                   " %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld",
                   &state, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,
                   &minflt, &cminflt, &majflt, &cmajflt,
                   &utime, &stime, &cutime, &cstime) != 15) {
                fprintf(stderr, "Failed to parse proc stat\n");
                return -1;
        }

        slice->utime  = utime;
        slice->stime  = stime;
        slice->cutime = cutime < 0 ? 0 : (unsigned long long) cutime;
        slice->cstime = cstime < 0 ? 0 : (unsigned long long) cstime;

        slice->total          = slice->utime + slice->stime;
        slice->total_children = slice->total + slice->cutime + slice->cstime;
        slice->valid          = 1u;
        return 0;
}

int
get_proc_cpu_usage(long pid, Cpu_Proc_Slice *slice)
{
        char path[64];

        if (pid <= 0) {
                return read_proc_stat("/proc/self/stat", pid, slice);
        }
        snprintf(path, sizeof path, "/proc/%ld/stat", pid);
        return read_proc_stat(path, pid, slice);
}

int
get_thread_cpu_usage(long pid, long tid, Cpu_Proc_Slice *slice)
{
        char path[64];

        if (pid <= 0) {
                snprintf(path, sizeof path, "/proc/self/task/%ld/stat", tid);
        } else {
                snprintf(path, sizeof path, "/proc/%ld/task/%ld/stat", pid, tid);
        }
        return read_proc_stat(path, tid, slice);
}

int
get_cpu_count(void)
{
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n < 0) {
                perror("sysconf(_SC_NPROCESSORS_ONLN)");
                return -1;
        }
        return (int) n;
}

static double
calc_proc_impl(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1,
               Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1,
               unsigned long long t0, unsigned long long t1)
{
        if (!p0.valid || !p1.valid || !sys0.valid || !sys1.valid) return -1.0;
        if (p0.pid != p1.pid || sys0.id != id || sys1.id != id) return -1.0;
        if (t1 < t0 || sys1.total <= sys0.total) return -1.0; // process died/reset

        return ((double) (t1 - t0) / (double) (sys1.total - sys0.total)) * 100.0;
}

double
calc_proc_cpu_usage(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1,
                    Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1)
{
        return calc_proc_impl(id, p0, p1, sys0, sys1, p0.total, p1.total);
}

double
calc_proc_cpu_usage_children(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1,
                             Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1)
{
        return calc_proc_impl(id, p0, p1, sys0, sys1,
                              p0.total_children, p1.total_children);
}

static double
calc_proc_impl_per_core(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1,
                        Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1,
                        unsigned long long t0, unsigned long long t1)
{
        double pct;
        int n;

        if (id != CPU_ALL) return -1.0; // per-core basis needs the machine total
        pct = calc_proc_impl(id, p0, p1, sys0, sys1, t0, t1);
        if (pct < 0.0) return pct;
        n = get_cpu_count();
        if (n <= 0) return -1.0;
        return pct * n;
}

double
calc_proc_cpu_usage_per_core(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1,
                             Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1)
{
        return calc_proc_impl_per_core(id, p0, p1, sys0, sys1, p0.total, p1.total);
}

double
calc_proc_cpu_usage_children_per_core(Cpu_Id id, Cpu_Proc_Slice p0, Cpu_Proc_Slice p1,
                                      Cpu_Perf_Slice sys0, Cpu_Perf_Slice sys1)
{
        return calc_proc_impl_per_core(id, p0, p1, sys0, sys1,
                                       p0.total_children, p1.total_children);
}

double
calc_cpu_usage(Cpu_Perf_Slice t0, Cpu_Perf_Slice t1)
{
        unsigned long long diff_total, diff_idle;

        if (!t0.valid || !t1.valid || t0.id != t1.id) return -1.0;
        if (t1.total <= t0.total) return -1.0; // counter reset / cpu hotplug

        diff_total = t1.total - t0.total;
        diff_idle  = t1.idle > t0.idle ? t1.idle - t0.idle : 0;

        return ((double) (diff_total - diff_idle) / diff_total) * 100.0;
}

#endif

#if defined(MAIN_TEST)

// EXAMPLE

#include <assert.h>
#include <signal.h>
#include <unistd.h>

static pid_t busy_pid;

static void
on_signal(int sig)
{
        (void) sig;
        if (busy_pid > 0) kill(busy_pid, SIGKILL);
        _exit(0);
}

int
main()
{
        Cpu_Perf_Slice prev;
        Cpu_Perf_Slice curr;
        Cpu_Proc_Slice s_prev;
        Cpu_Proc_Slice s_curr;
        Cpu_Proc_Slice c_prev;
        Cpu_Proc_Slice c_curr;
        Cpu_Proc_Slice t_prev;
        Cpu_Proc_Slice t_curr;
        Cpu_Id id = CPU_ALL;
        double usage, self_usage, child_usage, thread_usage, child_per_core;

        busy_pid = fork();
        assert(busy_pid >= 0);
        if (busy_pid == 0) {
                // child: burn one core forever
                for (volatile unsigned long long i = 1; i != 0; i++)
                        ;
        }

        signal(SIGINT, on_signal);
        signal(SIGTERM, on_signal);

        assert(get_cpu_usage(id, &prev) >= 0);
        assert(get_proc_cpu_usage(0, &s_prev) >= 0);        // self: sleeping parent
        assert(get_proc_cpu_usage(busy_pid, &c_prev) >= 0); // busy child
        // single-threaded child, so its main thread is tid == pid
        assert(get_thread_cpu_usage(busy_pid, busy_pid, &t_prev) >= 0);

        while (1) {
                sleep(1);
                assert(get_cpu_usage(id, &curr) >= 0);
                assert(get_proc_cpu_usage(0, &s_curr) >= 0);
                assert(get_proc_cpu_usage(busy_pid, &c_curr) >= 0);
                assert(get_thread_cpu_usage(busy_pid, busy_pid, &t_curr) >= 0);
                usage         = calc_cpu_usage(prev, curr);
                self_usage    = calc_proc_cpu_usage(id, s_prev, s_curr, prev, curr);
                child_usage   = calc_proc_cpu_usage(id, c_prev, c_curr, prev, curr);
                thread_usage  = calc_proc_cpu_usage(id, t_prev, t_curr, prev, curr);
                child_per_core = calc_proc_cpu_usage_per_core(id, c_prev, c_curr, prev, curr);
                printf("Total: %.2f%% | self: %.2f%% | child: %.2f%% | child thread: %.2f%% | child per core: %.2f%%\n",
                       usage, self_usage, child_usage, thread_usage, child_per_core);
                prev   = curr;
                s_prev = s_curr;
                c_prev = c_curr;
                t_prev = t_curr;
        }
        return 0;
}
#endif


/*
-----------------------------------------------------------------------------
This software is in the public domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute
this software, either in source code form or as a compiled binary, for any
purpose, commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of
this software dedicate any and all copyright interest in the software to
the public domain. We make this dedication for the benefit of the public
at large and to the detriment of our heirs and successors. We intend this
dedication to be an overt act of relinquishment in perpetuity of all
present and future rights to this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
-----------------------------------------------------------------------------
*/
