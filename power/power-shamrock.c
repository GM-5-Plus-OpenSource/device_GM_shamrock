/*
 * YANIK
 * SIKERIM
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_NIDEBUG 0
#define LOG_TAG "AgressivePowerHAL : CALGICI KARISI BINNAZ"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

// -----------------------------------------------------------------------------
// perfd-less boosts (cpu_boost + KGSL)
// Aggressive UI smoothness: boost CPU floors + sched_boost_on_input + GPU max perf
// -----------------------------------------------------------------------------
//
// Device mapping (per your device):
// - Big:    cpu0-3  (max 1516800)
// - Little: cpu4-7  (max 1209600)
// GPU:
// - pwrlevel: 0 fastest, 4 slowest
//
// We do timed boost with restore using generation counter to avoid races.

static const char* kCpuBoostMsPath        = "/sys/module/cpu_boost/parameters/input_boost_ms";
static const char* kCpuBoostFreqPath      = "/sys/module/cpu_boost/parameters/input_boost_freq";
static const char* kSchedBoostOnInputPath = "/sys/module/cpu_boost/parameters/sched_boost_on_input";

// GPU path variants (we auto-pick the one that exists)
static const char* kGpuMinPwrlevelPathA = "/sys/class/kgsl/kgsl-3d0/min_pwrlevel";
static const char* kGpuMinPwrlevelPathB =
    "/sys/devices/soc.0/1c00000.qcom,kgsl-3d0/kgsl/kgsl-3d0/min_pwrlevel";

static const char* gGpuMinPwrlevelPath = NULL;

static pthread_mutex_t g_boost_lock = PTHREAD_MUTEX_INITIALIZER;
static long long g_boost_gen = 0;

// Cached defaults (read once, restored after each timed boost)
static int  g_def_cpu_boost_ms = -1;
static char g_def_cpu_boost_freq[256] = {0};
static int  g_def_sched_boost_on_input = -1;
static int  g_def_gpu_min_pwrlevel = -1;

static int read_int(const char* path, int* out) {
    int fd = open(path, O_RDONLY);
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
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = (int)read(fd, out, out_sz - 1);
    close(fd);
    if (n <= 0) return -1;
    out[n] = '\0';
    return 0;
}

static int write_int(const char* path, int val) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    dprintf(fd, "%d", val);
    close(fd);
    return 0;
}

static int write_str_file(const char* path, const char* s) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    (void)write(fd, s, strlen(s));
    close(fd);
    return 0;
}

static const char* pick_gpu_path_once() {
    // Prefer /sys/class if it exists, else fall back to the SoC path.
    if (access(kGpuMinPwrlevelPathA, F_OK) == 0) return kGpuMinPwrlevelPathA;
    if (access(kGpuMinPwrlevelPathB, F_OK) == 0) return kGpuMinPwrlevelPathB;
    return NULL;
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

    if (g_def_cpu_boost_ms == -1) {
        if (read_int(kCpuBoostMsPath, &g_def_cpu_boost_ms) != 0) {
            ALOGW("Failed to read default input_boost_ms");
        }
    }
    if (g_def_cpu_boost_freq[0] == '\0') {
        if (read_str(kCpuBoostFreqPath, g_def_cpu_boost_freq, sizeof(g_def_cpu_boost_freq)) != 0) {
            ALOGW("Failed to read default input_boost_freq");
        }
    }
    if (g_def_sched_boost_on_input == -1) {
        if (read_int(kSchedBoostOnInputPath, &g_def_sched_boost_on_input) != 0) {
            ALOGW("Failed to read default sched_boost_on_input");
        }
    }
    if (g_def_gpu_min_pwrlevel == -1 && gGpuMinPwrlevelPath) {
        if (read_int(gGpuMinPwrlevelPath, &g_def_gpu_min_pwrlevel) != 0) {
            ALOGW("Failed to read default gpu min_pwrlevel");
        }
    }
}

static void restore_defaults_locked() {
    // Restore CPU boost freq
    if (g_def_cpu_boost_freq[0] != '\0') {
        if (write_str_file(kCpuBoostFreqPath, g_def_cpu_boost_freq) != 0) {
            ALOGW("Failed to restore input_boost_freq");
        }
    }

    // Restore CPU boost ms (restore even if 0)
    if (g_def_cpu_boost_ms >= 0) {
        if (write_int(kCpuBoostMsPath, g_def_cpu_boost_ms) != 0) {
            ALOGW("Failed to restore input_boost_ms");
        }
    }

    // Restore sched_boost_on_input
    if (g_def_sched_boost_on_input >= 0) {
        if (write_int(kSchedBoostOnInputPath, g_def_sched_boost_on_input) != 0) {
            ALOGW("Failed to restore sched_boost_on_input");
        }
    }

    // Restore GPU min_pwrlevel
    if (g_def_gpu_min_pwrlevel >= 0 && gGpuMinPwrlevelPath) {
        if (write_int(gGpuMinPwrlevelPath, g_def_gpu_min_pwrlevel) != 0) {
            ALOGW("Failed to restore GPU min_pwrlevel");
        }
    }
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

// Timed boost apply:
// - cpu_freq_str: written to input_boost_freq
// - cpu_boost_ms: written to input_boost_ms
// - sched_boost_on_input: 1 during boost (aggressive scheduler boost)
// - gpu_min_pwrlevel: 0 = max perf, higher = lower perf
// - duration_ms: after this we restore defaults (if no newer boost)
static void apply_boost_timed(const char* cpu_freq_str,
                             int cpu_boost_ms,
                             int sched_boost_on_input,
                             int gpu_min_pwrlevel,
                             int duration_ms) {
    pthread_mutex_lock(&g_boost_lock);
    cache_defaults_locked();

    g_boost_gen++;
    long long mygen = g_boost_gen;

    // CPU boost knobs
    if (cpu_freq_str && cpu_freq_str[0]) {
        if (write_str_file(kCpuBoostFreqPath, cpu_freq_str) != 0) {
            ALOGW("Failed to write input_boost_freq");
        }
    }
    if (cpu_boost_ms >= 0) {
        if (write_int(kCpuBoostMsPath, cpu_boost_ms) != 0) {
            ALOGW("Failed to write input_boost_ms");
        }
    }

    // Scheduler boost on input (aggressive)
    if (sched_boost_on_input >= 0) {
        if (write_int(kSchedBoostOnInputPath, sched_boost_on_input) != 0) {
            ALOGW("Failed to write sched_boost_on_input");
        }
    }

    // GPU boost (if node exists)
    if (gGpuMinPwrlevelPath && gpu_min_pwrlevel >= 0) {
        if (write_int(gGpuMinPwrlevelPath, gpu_min_pwrlevel) != 0) {
            ALOGW("Failed to write GPU min_pwrlevel");
        }
    }

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
// Existing QCOM PowerHAL logic (profiles + video encode etc.)
// -----------------------------------------------------------------------------

static int video_encode_hint_sent;

const int kMinInteractiveDuration = 500;  /* ms */
const int kMaxInteractiveDuration = 5000; /* ms */
const int kMaxLaunchDuration = 5000;      /* ms */

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

static int set_power_profile(void* data) {
    int profile = data ? *((int*)data) : 0;
    int ret = -EINVAL;
    const char* profile_name = NULL;

    if (profile == current_power_profile) return 0;

    ALOGV("%s: Profile=%d", __func__, profile);

    if (current_power_profile != PROFILE_BALANCED) {
        undo_hint_action(DEFAULT_PROFILE_HINT_ID);
        ALOGV("%s: Hint undone", __func__);
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

static int process_video_encode_hint(void* metadata) {
    char governor[80];
    struct video_encode_metadata_t video_encode_metadata;

    if (!metadata) return HINT_NONE;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");
        return HINT_NONE;
    }

    memset(&video_encode_metadata, 0, sizeof(struct video_encode_metadata_t));
    video_encode_metadata.state = -1;
    video_encode_metadata.hint_id = DEFAULT_VIDEO_ENCODE_HINT_ID;

    if (parse_video_encode_metadata((char*)metadata, &video_encode_metadata) == -1) {
        ALOGE("Error occurred while parsing metadata.");
        return HINT_NONE;
    }

    if (video_encode_metadata.state == 1) {
        if (is_interactive_governor(governor)) {
            int resource_values[] = {
                INT_OP_CLUSTER0_USE_SCHED_LOAD,      0x1,
                INT_OP_CLUSTER1_USE_SCHED_LOAD,      0x1,
                INT_OP_CLUSTER0_USE_MIGRATION_NOTIF, 0x1,
                INT_OP_CLUSTER1_USE_MIGRATION_NOTIF, 0x1,
                INT_OP_CLUSTER0_TIMER_RATE,          BIG_LITTLE_TR_MS_40,
                INT_OP_CLUSTER1_TIMER_RATE,          BIG_LITTLE_TR_MS_40
            };
            perform_hint_action(video_encode_metadata.hint_id, resource_values,
                                ARRAY_SIZE(resource_values));
            return HINT_HANDLED;
        }
    } else if (video_encode_metadata.state == 0) {
        if (is_interactive_governor(governor)) {
            undo_hint_action(video_encode_metadata.hint_id);
            video_encode_hint_sent = 0;
            return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}

// -----------------------------------------------------------------------------
// Aggressive Interaction / Launch overrides (max smoothness)
// -----------------------------------------------------------------------------

static void process_interaction_hint(void* data) {
    (void)data;
    // MAX smoothness preset:
    // - Big (0-3) floor to 1516800
    // - Little (4-7) floor to 1209600
    // - sched_boost_on_input=1 during boost
    // - GPU min_pwrlevel=0 (max perf)
    apply_boost_timed(
        "0:1516800 1:1516800 2:1516800 3:1516800 4:1209600 5:1209600 6:1209600 7:1209600",
        700,  // input_boost_ms
        1,    // sched_boost_on_input
        0,    // GPU min_pwrlevel
        650   // duration
    );
}

static int process_activity_launch_hint(void* data) {
    // Frameworks typically send (int*)1 on launch start and (int*)0 or NULL on end.
    int start = (data && *((int*)data) == 1);
    if (start) {
        apply_boost_timed(
            "0:1516800 1:1516800 2:1516800 3:1516800 4:1209600 5:1209600 6:1209600 7:1209600",
            1600, // input_boost_ms
            1,    // sched_boost_on_input
            0,    // GPU min_pwrlevel
            1300  // duration
        );
    } else {
        restore_now();
    }
    return HINT_HANDLED;
}

// -----------------------------------------------------------------------------
// Hooks used by framework
// -----------------------------------------------------------------------------

int power_hint_override(power_hint_t hint, void* data) {
    int ret_val = HINT_NONE;

    if (hint == POWER_HINT_SET_PROFILE) {
        if (set_power_profile(data) < 0) ALOGE("Setting power profile failed. perfd not started?");
        return HINT_HANDLED;
    }

    // Skip other hints in high/low power modes
    if (current_power_profile == PROFILE_POWER_SAVE ||
        current_power_profile == PROFILE_HIGH_PERFORMANCE) {
        return HINT_HANDLED;
    }

    switch (hint) {
        case POWER_HINT_VIDEO_ENCODE:
            ret_val = process_video_encode_hint(data);
            break;
        case POWER_HINT_INTERACTION:
            process_interaction_hint(data);
            ret_val = HINT_HANDLED;
            break;
        case POWER_HINT_LAUNCH:
            ret_val = process_activity_launch_hint(data);
            break;
        default:
            break;
    }
    return ret_val;
}

int set_interactive_override(int on) {
    // When screen turns off, drop any active boosts and restore defaults.
    if (!on) {
        restore_now();
    }
    return HINT_HANDLED;
}
