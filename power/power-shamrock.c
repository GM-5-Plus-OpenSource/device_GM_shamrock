/*
 * AgressivePowerHAL ver. 1
 * yigityanik
 *
 */

#define LOG_NIDEBUG 0
#define LOG_TAG "AgressivePowerHALrelease : "

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>   // snprintf
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <hardware/hardware.h>
#include <hardware/power.h>
#include <log/log.h>

#include "hint-data.h"
#include "metadata-defs.h"
#include "performance.h"
#include "power-common.h"
#include "utils.h"

// --- Bringup fallback for Android 12+ trees (missing legacy power profiles) ---
#ifndef PROFILE_POWER_SAVE
#define PROFILE_POWER_SAVE        0
#endif
#ifndef PROFILE_BALANCED
#define PROFILE_BALANCED          1
#endif
#ifndef PROFILE_HIGH_PERFORMANCE
#define PROFILE_HIGH_PERFORMANCE  2
#endif
#ifndef PROFILE_BIAS_POWER
#define PROFILE_BIAS_POWER        3
#endif
#ifndef PROFILE_BIAS_PERFORMANCE
#define PROFILE_BIAS_PERFORMANCE  4
#endif

#ifndef DEFAULT_PROFILE_HINT_ID
// Old CAF code used a dedicated "profile" hint id. In S trees this may not exist.
// 0 is fine as long as your perform_hint_action/undo_hint_action ignores unknown ids.
#define DEFAULT_PROFILE_HINT_ID   0
#endif
// ---------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// perfd-less boosts (cpu_boost + KGSL)
// Aggressive UI smoothness: boost CPU floors + sched_boost_on_input + GPU max perf
// -----------------------------------------------------------------------------

static const char* kCpuBoostMsPath   = "/sys/module/cpu_boost/parameters/input_boost_ms";
static const char* kCpuBoostFreqPath = "/sys/module/cpu_boost/parameters/input_boost_freq";

// Sched boost: fallback chain
static const char* kSchedBoostPaths[] = {
    "/sys/module/cpu_boost/parameters/sched_boost_on_input",  // often expects "Y"/"N"
    "/proc/sys/kernel/sched_boost",                            // expects "1"/"0"
    NULL,
};
static const char* gSchedBoostPath = NULL;

// GPU path variants (we auto-pick the one that exists)
static const char* kGpuMinPwrlevelPathA = "/sys/class/kgsl/kgsl-3d0/min_pwrlevel";
static const char* kGpuMinPwrlevelPathB =
    "/sys/devices/soc.0/1c00000.qcom,kgsl-3d0/kgsl/kgsl-3d0/min_pwrlevel";

static const char* gGpuMinPwrlevelPath = NULL;

static pthread_mutex_t g_boost_lock = PTHREAD_MUTEX_INITIALIZER;
static long long g_boost_gen = 0;

// Optional self-disable to avoid spam if writes are denied on device
static int g_gpu_boost_disabled = 0;
static int g_cpu_boost_disabled = 0;
static int g_sched_boost_disabled = 0;

// Cached defaults (read once, restored after each timed boost)
static int  g_def_cpu_boost_ms = -1;
static char g_def_cpu_boost_freq[256] = {0};
static int  g_def_sched_boost = -1;     // store 0/1 even if kernel expects Y/N
static int  g_def_gpu_min_pwrlevel = -1;

// Auto-generated input_boost_freq based on cpufreq policy mapping
static char g_auto_boost_freq[256] = {0};
static int  g_auto_boost_ready = 0;

// ---- forward decls (fix C99 implicit-decl errors) ----
static int read_str(const char* path, char* out, size_t out_sz);
static int read_int(const char* path, int* out);

// Helpers
static int is_cpu_boost_sched_param_path(const char* path) {
    return path && (strstr(path, "/sys/module/cpu_boost/parameters/sched_boost_on_input") != NULL);
}

static void trim_ws(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t'))
        s[--n] = 0;
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static void ensure_newline(char* s, size_t cap) {
    if (!s || cap == 0) return;
    size_t n = strlen(s);
    if (n == 0) return;
    if (s[n-1] != '\n') {
        if (n + 1 < cap) {
            s[n] = '\n';
            s[n+1] = '\0';
        }
    }
}

// Replace '\n' and '\r' for log readability (avoid broken log lines)
static void sanitize_for_log(const char* in, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    if (!in) { out[0] = '\0'; return; }
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        char c = in[i];
        if (c == '\n') {
            if (j + 2 < out_sz) { out[j++]='\\'; out[j++]='n'; }
            else break;
        } else if (c == '\r') {
            if (j + 2 < out_sz) { out[j++]='\\'; out[j++]='r'; }
            else break;
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

// Parse related_cpus formats like: "0 1 2 3" or "0-3 5-7"
static int parse_cpu_list(const char* s, int* cpus, int max_cpus) {
    if (!s || !cpus || max_cpus <= 0) return 0;
    int n = 0;
    const char* p = s;
    while (*p && n < max_cpus) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        int a = -1, b = -1;
        // try range a-b
        if (sscanf(p, "%d-%d", &a, &b) == 2 && a >= 0 && b >= a) {
            for (int v = a; v <= b && n < max_cpus; v++) cpus[n++] = v;
        } else if (sscanf(p, "%d", &a) == 1 && a >= 0) {
            cpus[n++] = a;
        }

        // advance to next token
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    }
    return n;
}

typedef struct {
    int policy_id;
    int cpus[16];
    int cpu_count;
    int max_freq;
} policy_info_t;

static int read_int_from_file(const char* path, int* out) {
    if (!path || !out) return -EINVAL;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -errno;
    char buf[64];
    int n = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -EIO;
    buf[n] = '\0';
    *out = atoi(buf);
    return 0;
}

static int collect_policies(policy_info_t* out, int max_pols) {
    if (!out || max_pols <= 0) return 0;

    DIR* d = opendir("/sys/devices/system/cpu/cpufreq");
    if (!d) return 0;

    int n = 0;
    struct dirent* de;
    while ((de = readdir(d)) != NULL && n < max_pols) {
        if (strncmp(de->d_name, "policy", 6) != 0) continue;

        int pid = atoi(de->d_name + 6);

        char path[256];
        char buf[512];

        // related_cpus
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/%s/related_cpus", de->d_name);
        if (read_str(path, buf, sizeof(buf)) != 0) continue;
        trim_ws(buf);

        int cpus[16];
        int cpu_count = parse_cpu_list(buf, cpus, (int)(sizeof(cpus)/sizeof(cpus[0])));
        if (cpu_count <= 0) continue;

        // cpuinfo_max_freq
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/%s/cpuinfo_max_freq", de->d_name);
        int maxf = -1;
        if (read_int_from_file(path, &maxf) != 0 || maxf <= 0) continue;

        out[n].policy_id = pid;
        out[n].cpu_count = cpu_count;
        out[n].max_freq = maxf;
        for (int i = 0; i < cpu_count; i++) out[n].cpus[i] = cpus[i];
        n++;
    }

    closedir(d);
    return n;
}

static int cmp_policy_by_maxfreq_desc(const void* a, const void* b) {
    const policy_info_t* pa = (const policy_info_t*)a;
    const policy_info_t* pb = (const policy_info_t*)b;
    // big first
    if (pa->max_freq > pb->max_freq) return -1;
    if (pa->max_freq < pb->max_freq) return 1;
    return pa->policy_id - pb->policy_id;
}

// Build input_boost_freq string by detecting which cpufreq policy is "big" or "little"
// We simply use each policy's cpuinfo_max_freq as the boost floor for its CPUs.
static int build_auto_input_boost_freq(char* out, size_t out_sz) {
    if (!out || out_sz == 0) return -EINVAL;
    out[0] = '\0';

    policy_info_t pols[8];
    int np = collect_policies(pols, (int)(sizeof(pols)/sizeof(pols[0])));
    if (np <= 0) return -ENOENT;

    qsort(pols, np, sizeof(pols[0]), cmp_policy_by_maxfreq_desc);

    // Compose "cpu:freq cpu:freq ..." (keep order: big->little for readability)
    size_t used = 0;
    for (int i = 0; i < np; i++) {
        for (int j = 0; j < pols[i].cpu_count; j++) {
            char seg[32];
            int len = snprintf(seg, sizeof(seg), "%d:%d%s",
                               pols[i].cpus[j], pols[i].max_freq,
                               (i == np - 1 && j == pols[i].cpu_count - 1) ? "" : " ");
            if (len <= 0) continue;
            if (used + (size_t)len + 2 >= out_sz) break;
            memcpy(out + used, seg, (size_t)len);
            used += (size_t)len;
            out[used] = '\0';
        }
    }

    // Add newline for kernel sysfs expectations (some nodes expect it)
    if (used + 2 < out_sz) {
        out[used++] = '\n';
        out[used] = '\0';
    }
    return 0;
}

static void ensure_auto_boost_string_locked() {
    if (g_auto_boost_ready) return;
    int rc = build_auto_input_boost_freq(g_auto_boost_freq, sizeof(g_auto_boost_freq));
    if (rc != 0) {
        ALOGW("Auto input_boost_freq build failed rc=%d; falling back to hardcoded mapping", rc);
        g_auto_boost_freq[0] = '\0';
    } else {
        char logbuf[256];
        sanitize_for_log(g_auto_boost_freq, logbuf, sizeof(logbuf));
        ALOGI("Auto input_boost_freq => '%s'", logbuf);
    }
    g_auto_boost_ready = 1;
}

static int write_int(const char* path, int val) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGW("open(%s) failed: %s", path, strerror(errno));
        return -errno;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    if (len < 0) {
        close(fd);
        return -EINVAL;
    }
    if (write(fd, buf, (size_t)len) < 0) {
        ALOGW("write(%s) failed: %s", path, strerror(errno));
        close(fd);
        return -errno;
    }
    close(fd);
    return 0;
}

static int write_str_file(const char* path, const char* s) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGW("open(%s) failed: %s", path, strerror(errno));
        return -errno;
    }
    if (write(fd, s, strlen(s)) < 0) {
        ALOGW("write(%s) failed: %s", path, strerror(errno));
        close(fd);
        return -errno;
    }
    close(fd);
    return 0;
}

static const char* pick_gpu_path_once() {
    if (access(kGpuMinPwrlevelPathA, F_OK) == 0) return kGpuMinPwrlevelPathA;
    if (access(kGpuMinPwrlevelPathB, F_OK) == 0) return kGpuMinPwrlevelPathB;
    return NULL;
}

static const char* pick_sched_boost_path_once() {
    for (int i = 0; kSchedBoostPaths[i]; i++) {
        if (access(kSchedBoostPaths[i], F_OK) == 0) return kSchedBoostPaths[i];
    }
    return NULL;
}

static int write_sched_boost_value(const char* path, int on) {
    if (!path) return -EINVAL;
    if (is_cpu_boost_sched_param_path(path)) {
        return write_str_file(path, on ? "Y" : "N");
    }
    return write_int(path, on ? 1 : 0);
}

static void dump_state(const char* where) {
    int ms = -1, sched = -1, gpu = -1;
    char freq[256] = {0};

    (void)read_int(kCpuBoostMsPath, &ms);
    (void)read_str(kCpuBoostFreqPath, freq, sizeof(freq));
    trim_ws(freq);

    if (gSchedBoostPath) {
        if (is_cpu_boost_sched_param_path(gSchedBoostPath)) {
            char tmp[16] = {0};
            if (read_str(gSchedBoostPath, tmp, sizeof(tmp)) == 0) {
                trim_ws(tmp);
                sched = (tmp[0] == 'Y' || tmp[0] == 'y' || tmp[0] == '1') ? 1 : 0;
            }
        } else {
            (void)read_int(gSchedBoostPath, &sched);
        }
    }

    if (gGpuMinPwrlevelPath) (void)read_int(gGpuMinPwrlevelPath, &gpu);

    ALOGI("STATE[%s]: boost_ms=%d boost_freq='%s' sched_boost=%d gpu_min_pwrlevel=%d",
          where, ms, freq, sched, gpu);
}

static void cache_defaults_locked() {
    if (!gGpuMinPwrlevelPath) {
        gGpuMinPwrlevelPath = pick_gpu_path_once();
        if (!gGpuMinPwrlevelPath) {
            ALOGW("GPU min_pwrlevel path not found (no KGSL node?)");
        } else {
            ALOGI("Using GPU min_pwrlevel path: %s", gGpuMinPwrlevelPath);
        }
    }

    if (!gSchedBoostPath) {
        gSchedBoostPath = pick_sched_boost_path_once();
        if (!gSchedBoostPath) {
            ALOGW("No sched boost path found (cpu_boost/proc missing?)");
        } else {
            ALOGI("Using sched boost path: %s", gSchedBoostPath);
        }
    }

    if (g_def_cpu_boost_ms == -1) {
        if (read_int(kCpuBoostMsPath, &g_def_cpu_boost_ms) != 0) {
            ALOGW("Failed to read default input_boost_ms");
        }
    }
    if (g_def_cpu_boost_freq[0] == '\0') {
        if (read_str(kCpuBoostFreqPath, g_def_cpu_boost_freq, sizeof(g_def_cpu_boost_freq)) != 0) {
            ALOGW("Failed to read default input_boost_freq");
        } else {
            trim_ws(g_def_cpu_boost_freq);
            ensure_newline(g_def_cpu_boost_freq, sizeof(g_def_cpu_boost_freq));
        }
    }
    if (g_def_sched_boost == -1 && gSchedBoostPath) {
        if (is_cpu_boost_sched_param_path(gSchedBoostPath)) {
            char tmp[16] = {0};
            if (read_str(gSchedBoostPath, tmp, sizeof(tmp)) == 0) {
                trim_ws(tmp);
                g_def_sched_boost = (tmp[0] == 'Y' || tmp[0] == 'y' || tmp[0] == '1') ? 1 : 0;
            } else {
                ALOGW("Failed to read default sched boost");
            }
        } else {
            if (read_int(gSchedBoostPath, &g_def_sched_boost) != 0) {
                ALOGW("Failed to read default sched boost");
            }
        }
    }
    if (g_def_gpu_min_pwrlevel == -1 && gGpuMinPwrlevelPath) {
        if (read_int(gGpuMinPwrlevelPath, &g_def_gpu_min_pwrlevel) != 0) {
            ALOGW("Failed to read default gpu min_pwrlevel");
        }
    }

    ensure_auto_boost_string_locked();
    dump_state("cache_defaults");
}

static int should_disable_for_rc(int rc) {
    switch (-rc) {
        case ENOENT:
        case ENODEV:
        case EROFS:
            return 1;
        default:
            return 0;
    }
}

static void restore_defaults_locked() {
    dump_state("before_restore");

    if (!g_cpu_boost_disabled && g_def_cpu_boost_freq[0] != '\0') {
        int rc = write_str_file(kCpuBoostFreqPath, g_def_cpu_boost_freq);
        if (rc != 0) {
            ALOGW("Failed to restore input_boost_freq rc=%d", rc);
            if (should_disable_for_rc(rc)) g_cpu_boost_disabled = 1;
        }
    }

    if (!g_cpu_boost_disabled && g_def_cpu_boost_ms >= 0) {
        int rc = write_int(kCpuBoostMsPath, g_def_cpu_boost_ms);
        if (rc != 0) {
            ALOGW("Failed to restore input_boost_ms rc=%d", rc);
            if (should_disable_for_rc(rc)) g_cpu_boost_disabled = 1;
        }
    }

    if (!g_sched_boost_disabled && g_def_sched_boost >= 0 && gSchedBoostPath) {
        int rc = write_sched_boost_value(gSchedBoostPath, g_def_sched_boost);
        if (rc != 0) {
            ALOGW("Failed to restore sched boost rc=%d", rc);
            if (should_disable_for_rc(rc)) g_sched_boost_disabled = 1;
        }
    }

    if (!g_gpu_boost_disabled && g_def_gpu_min_pwrlevel >= 0 && gGpuMinPwrlevelPath) {
        int rc = write_int(gGpuMinPwrlevelPath, g_def_gpu_min_pwrlevel);
        if (rc != 0) {
            ALOGW("Failed to restore GPU min_pwrlevel rc=%d", rc);
            if (should_disable_for_rc(rc)) g_gpu_boost_disabled = 1;
        }
    }

    dump_state("after_restore");
}

struct restore_args {
    int delay_ms;
    long long gen;
};

static void* restore_thread(void* arg) {
    struct restore_args* a = (struct restore_args*)arg;
    usleep((useconds_t)a->delay_ms * 1000);

    pthread_mutex_lock(&g_boost_lock);
    if (a->gen == g_boost_gen) {
        cache_defaults_locked();
        restore_defaults_locked();
    }
    pthread_mutex_unlock(&g_boost_lock);

    free(a);
    return NULL;
}

static void apply_boost_timed(const char* cpu_freq_str,
                             int cpu_boost_ms,
                             int sched_boost_on_input,
                             int gpu_min_pwrlevel,
                             int duration_ms) {
    pthread_mutex_lock(&g_boost_lock);
    cache_defaults_locked();

    g_boost_gen++;
    long long mygen = g_boost_gen;

    dump_state("before_apply");

    if (!g_cpu_boost_disabled) {
        if (cpu_freq_str && cpu_freq_str[0]) {
            char logbuf[256];
            sanitize_for_log(cpu_freq_str, logbuf, sizeof(logbuf));
            ALOGI("WRITE input_boost_freq => '%s'", logbuf);
            int rc = write_str_file(kCpuBoostFreqPath, cpu_freq_str);
            if (rc != 0) {
                ALOGW("Failed to write input_boost_freq rc=%d", rc);
                if (should_disable_for_rc(rc)) g_cpu_boost_disabled = 1;
            }
        }
        if (!g_cpu_boost_disabled && cpu_boost_ms >= 0) {
            int rc = write_int(kCpuBoostMsPath, cpu_boost_ms);
            if (rc != 0) {
                ALOGW("Failed to write input_boost_ms rc=%d", rc);
                if (should_disable_for_rc(rc)) g_cpu_boost_disabled = 1;
            }
        }
    }

    if (!g_sched_boost_disabled && gSchedBoostPath && sched_boost_on_input >= 0) {
        int rc = write_sched_boost_value(gSchedBoostPath, sched_boost_on_input);
        if (rc != 0) {
            ALOGW("Failed to write sched boost rc=%d", rc);
            if (should_disable_for_rc(rc)) g_sched_boost_disabled = 1;
        }
    }

    if (!g_gpu_boost_disabled && gGpuMinPwrlevelPath && gpu_min_pwrlevel >= 0) {
        int rc = write_int(gGpuMinPwrlevelPath, gpu_min_pwrlevel);
        if (rc != 0) {
            ALOGW("Failed to write GPU min_pwrlevel rc=%d", rc);
            if (should_disable_for_rc(rc)) g_gpu_boost_disabled = 1;
        }
    }

    dump_state("after_apply");
    pthread_mutex_unlock(&g_boost_lock);

    struct restore_args* a = (struct restore_args*)calloc(1, sizeof(*a));
    if (!a) return;
    a->delay_ms = duration_ms;
    a->gen = mygen;

    pthread_t t;
    if (pthread_create(&t, NULL, restore_thread, a) == 0) {
        pthread_detach(t);
    } else {
        free(a);
    }
}

static void restore_now() {
    pthread_mutex_lock(&g_boost_lock);
    cache_defaults_locked();
    g_boost_gen++;
    restore_defaults_locked();
    pthread_mutex_unlock(&g_boost_lock);
}

// -----------------------------------------------------------------------------
// Existing QCOM PowerHAL logic (kept minimal; your tree likely uses power_hint_override hooks)
// -----------------------------------------------------------------------------

const int kMinInteractiveDuration = 500;
const int kMaxInteractiveDuration = 5000;
const int kMaxLaunchDuration = 5000;

static int current_power_profile = PROFILE_BALANCED;

// clang-format off
static int profile_high_performance[] = {
    SCHED_BOOST_ON_V3, 0x1,
    ALL_CPUS_PWR_CLPS_DIS_V3, 0x1,
    CPUS_ONLINE_MIN_BIG, 0x4,
    MIN_FREQ_BIG_CORE_0, 0xFFF,
    MIN_FREQ_LITTLE_CORE_0, 0xFFF,
    GPU_MIN_POWER_LEVEL, 0x1,
    SCHED_PREFER_IDLE_DIS_V3, 0x1,
    SCHED_SMALL_TASK, 0x1,
    SCHED_MOSTLY_IDLE_NR_RUN, 0x1,
    SCHED_MOSTLY_IDLE_LOAD, 0x1,
};

static int profile_power_save[] = {
    CPUS_ONLINE_MAX_BIG, 0x1,
    MAX_FREQ_BIG_CORE_0, 0x3bf,
    MAX_FREQ_LITTLE_CORE_0, 0x300,
};

static int profile_bias_power[] = {
    MAX_FREQ_BIG_CORE_0, 0x4B0,
    MAX_FREQ_LITTLE_CORE_0, 0x300,
};

static int profile_bias_performance[] = {
    CPUS_ONLINE_MAX_BIG, 0x4,
    MIN_FREQ_BIG_CORE_0, 0x540,
};
// clang-format on

#ifdef INTERACTION_BOOST
int get_number_of_profiles() {
    return 5;
}
#endif

static __attribute__((unused)) int set_power_profile(void* data) {
    int profile = data ? *((int*)data) : 0;
    int ret = -EINVAL;
    const char* profile_name = NULL;

    if (profile == current_power_profile) return 0;

    if (current_power_profile != PROFILE_BALANCED) {
        undo_hint_action(DEFAULT_PROFILE_HINT_ID);
        current_power_profile = PROFILE_BALANCED;
    }

    if (profile == PROFILE_POWER_SAVE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_power_save,
                                  ARRAY_SIZE(profile_power_save));
        profile_name = "powersave";
    } else if (profile == PROFILE_HIGH_PERFORMANCE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_high_performance,
                                  ARRAY_SIZE(profile_high_performance));
        profile_name = "performance";
    } else if (profile == PROFILE_BIAS_POWER) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_bias_power,
                                  ARRAY_SIZE(profile_bias_power));
        profile_name = "bias power";
    } else if (profile == PROFILE_BIAS_PERFORMANCE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_bias_performance,
                                  ARRAY_SIZE(profile_bias_performance));
        profile_name = "bias perf";
    } else if (profile == PROFILE_BALANCED) {
        ret = 0;
        profile_name = "balanced";
    }

    if (ret == 0) {
        current_power_profile = profile;
        ALOGD("%s: Set %s mode", __func__, profile_name);
    }
    return ret;
}

static void process_interaction_hint(void* data) {
    (void)data;
    const char* boost = (g_auto_boost_freq[0] != '\0') ? g_auto_boost_freq :
        "0:1516800 1:1516800 2:1516800 3:1516800 4:1209600 5:1209600 6:1209600 7:1209600\n";
    apply_boost_timed(
        boost,
        700,
        1,
        0,
        650
    );
}

static int process_activity_launch_hint(void* data) {
    int start = (data && *((int*)data) == 1);
    if (start) {
        const char* boost = (g_auto_boost_freq[0] != '\0') ? g_auto_boost_freq :
            "0:1516800 1:1516800 2:1516800 3:1516800 4:1209600 5:1209600 6:1209600 7:1209600\n";
        apply_boost_timed(
            boost,
            1600,
            1,
            0,
            1300
        );
    }
    return HINT_HANDLED;
}

static inline int hint_is_interaction(power_hint_t hint) {
    int v = (int)hint;
    return (hint == POWER_HINT_INTERACTION) || (v == 1) || (v == 2);
}

static inline int hint_is_launch(power_hint_t hint) {
    int v = (int)hint;
    return (hint == POWER_HINT_LAUNCH) || (v == 7) || (v == 8);
}

int power_hint_override(power_hint_t hint, void* data) {
    int ret_val = HINT_NONE;

#ifdef POWER_HINT_SET_PROFILE
    if (hint == POWER_HINT_SET_PROFILE) {
        if (set_power_profile(data) < 0) ALOGE("Setting power profile failed. perfd not started?");
        return HINT_HANDLED;
    }
#endif

    if (current_power_profile == PROFILE_POWER_SAVE ||
        current_power_profile == PROFILE_HIGH_PERFORMANCE) {
        return HINT_HANDLED;
    }

    ALOGI("HINT_IN: hint=%d (0x%x) data=%p data_i32=%d",
          (int)hint, (unsigned int)hint, data, data ? *((int*)data) : -1);

    if (hint_is_interaction(hint)) {
        process_interaction_hint(data);
        ret_val = HINT_HANDLED;
    } else if (hint_is_launch(hint)) {
        ret_val = process_activity_launch_hint(data);
    }

    return ret_val;
}

int set_interactive_override(int on) {
    if (!on) {
        restore_now();
    }
    return HINT_HANDLED;
}

// -----------------------------------------------------------------------------
// low-level IO helpers (definitions; prototypes above)
// -----------------------------------------------------------------------------

static int read_int(const char* path, int* out) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    char buf[64];
    int n = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    *out = atoi(buf);
    return 0;
}

static int read_str(const char* path, char* out, size_t out_sz) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    int n = (int)read(fd, out, out_sz - 1);
    close(fd);
    if (n <= 0) return -1;
    out[n] = '\0';
    return 0;
}

void init_platform(void) {
    pthread_mutex_lock(&g_boost_lock);
    cache_defaults_locked();   // gerçek default'u servis açılışında yakala
    pthread_mutex_unlock(&g_boost_lock);
}