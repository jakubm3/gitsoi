#include <lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>
#include <sys/wait.h>

#define SETGROUP 78
#define SETBURST 79

int setgroup(int pid_number, int group)
{
    message m;
    m.m1_i1 = pid_number;
    m.m1_i2 = group;
    if (_syscall(MM, SETGROUP, &m) == -1) return 1;
    return 0;
}

int setburst(int pid_number, long burst)
{
    message m;
    m.m1_i1 = pid_number;
    m.m1_i2 = (int) burst;
    if (_syscall(MM, SETBURST, &m) == -1) return 1;
    return 0;
}

long clk_tck(void)
{
    long t;
#ifdef _SC_CLK_TCK
    t = sysconf(_SC_CLK_TCK);
    if (t > 0) return t;
#endif
    return 60;
}

long seconds_to_ticks(long sec)
{
    long t;
    t = clk_tck();
    if (sec <= 0) return 0;
    return sec * t;
}

void burn_ticks(long target)
{
    struct tms t;
    clock_t start;
    volatile unsigned long x;

    times(&t);
    start = t.tms_utime;

    x = 0;
    for (;;) {
        x = x + 1;
        if ((x & 0x3FFFFUL) == 0) {
            times(&t);
            if ((long)(t.tms_utime - start) >= target) break;
        }
    }
}

void delay_ticks(long ticks)
{
    struct tms t;
    clock_t start;

    times(&t);
    start = t.tms_utime;

    while ((long)(t.tms_utime - start) < ticks) {
        times(&t);
    }
}

void child_worker(int group, long burst, long runtime)
{
    int pid;
    struct tms t;
    clock_t u0;
    clock_t u1;

    pid = getpid();
    printf("child pid=%d before setgroup\n", pid);
    fflush(stdout);
    if (group != 0) {
      if (setgroup(pid, group) != 0) exit(2);
    }
    printf("child pid=%d after setgroup\n", pid);
    fflush(stdout);
    if (group == 2) {
	printf("child pid=%d before setburst burst=%1d\n", pid, burst);
	fflush(stdout);
        if (setburst(pid, burst) != 0) exit(3);
	printf("child pid=%d after setburst\n", pid);
	fflush(stdout);
    }

    times(&t);
    u0 = t.tms_utime;
    printf("START pid=%d group=%d burst=%ld\n", pid, group, burst);
    fflush(stdout);

    if (runtime > 0) {
        burn_ticks(runtime);
    } else {
        for (;;) {
        }
    }

    times(&t);
    u1 = t.tms_utime;
    printf("END pid=%d group=%d cpu_ticks=%ld\n", pid, group, (long)(u1 - u0));
    fflush(stdout);
    exit(0);
}

int spawn_worker(int group, long burst, long runtime)
{
    int pid;

    pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        child_worker(group, burst, runtime);
    }
    return pid;
}

int wait_all(int n)
{
    int i;
    int st;

    for (i = 0; i < n; i++) {
        if (wait(&st) < 0) return 1;
    }
    return 0;
}

void usage(void)
{
    printf("uzycie:\n");
    printf("  proces rr N [sekundy]\n");
    printf("  proces aging N [sekundy] [opoznienie_ms]\n");
    printf("  proces sjf [s=sekundy] B1 B2 B3 ...\n");
    printf("  proces mix [sekundy]\n");
    printf("grupy: 0=RR 1=Aging 2=SJF\n");
}

int main(int argc, char *argv[])
{
    int n;
    int i;
    int cnt;
    long sec;
    long runtime;
    long delay_ms;
    long burst;

    if (argc < 2) {
        usage();
        return 1;
    }

    sec = 5;
    delay_ms = 300;

    if (strcmp(argv[1], "onesjf") == 0) {
	int pid;
	pid = getpid();
	printf("onesjf pid=%d\n", pid);
	fflush(stdout);
	setgroup(pid, 2);
	printf("after setgroup\n");
	fflush(stdout);
	setburst(pid, 30);
	printf("after setburst\n");
	fflush(stdout);
	return 0;
    }

    if (strcmp(argv[1], "rr") == 0) {
        if (argc < 3) { usage(); return 1; }
        n = atoi(argv[2]);
        if (argc >= 4) sec = atol(argv[3]);
        runtime = seconds_to_ticks(sec);
        cnt = 0;
        for (i = 0; i < n; i++) {
            if (spawn_worker(0, 0, runtime) < 0) return 2;
            cnt++;
        }
        return wait_all(cnt);
    }

    if (strcmp(argv[1], "aging") == 0) {
        if (argc < 3) { usage(); return 1; }
        n = atoi(argv[2]);
        if (argc >= 4) sec = atol(argv[3]);
        if (argc >= 5) delay_ms = atol(argv[4]);
        runtime = seconds_to_ticks(sec);
        cnt = 0;
        for (i = 0; i < n; i++) {
            if (spawn_worker(1, 0, runtime) < 0) return 2;
            cnt++;
            if (i != n - 1) {
                delay_ticks(seconds_to_ticks(delay_ms) / 1000);
            }
        }
        return wait_all(cnt);
    }

    if (strcmp(argv[1], "sjf") == 0) {
        int argi;
        if (argc < 3) { usage(); return 1; }
        argi = 2;
        if (strncmp(argv[argi], "s=", 2) == 0) {
            sec = atol(argv[argi] + 2);
            argi++;
        }
        runtime = seconds_to_ticks(sec);
        cnt = 0;
        for (i = argi; i < argc; i++) {
            burst = atol(argv[i]);
            if (burst <= 0) continue;
            if (spawn_worker(2, burst, runtime) < 0) return 2;
            cnt++;
        }
        if (cnt == 0) { usage(); return 1; }
        return wait_all(cnt);
    }

    if (strcmp(argv[1], "mix") == 0) {
        if (argc >= 3) sec = atol(argv[2]);
        runtime = seconds_to_ticks(sec);
        cnt = 0;
        if (spawn_worker(0, 0, runtime) < 0) return 2;
        cnt++;
        if (spawn_worker(1, 0, runtime) < 0) return 2;
        cnt++;
        if (spawn_worker(2, 30, runtime) < 0) return 2;
        cnt++;
        if (spawn_worker(2, 90, runtime) < 0) return 2;
        cnt++;
        if (spawn_worker(2, 150, runtime) < 0) return 2;
        cnt++;
        return wait_all(cnt);
    }

    usage();
    return 1;
}
