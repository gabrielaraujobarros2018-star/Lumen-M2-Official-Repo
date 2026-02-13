/**
 * SweetExperiencesEngine.c - Achievement & Notification Engine for Lumen OS
 * Integrates with Linux kernel hooks and Wayland for user experience enhancement
 * Author: Custom Lumen OS Development
 * Target: /lumen-motonexus6/fw/boot/main/k/sweetexp/
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

// Configuration paths
#define SWEETEXP_INI_PATH "/lumen-motonexus6/fw/boot/main/k/sweetexp/sweetexpengine.ini"
#define SWEETEXP_DATA_PATH "/lumen-motonexus6/fw/boot/main/k/sweetexp/data/sweetexp_enginedata.dat"
#define NOTIFENGINE_SOCK "/tmp/notifengine.sock"

// Engine constants
#define MAX_ACHIEVEMENTS 50
#define MAX_NOTIFICATIONS 100
#define DATA_BUFFER_SIZE 4096
#define INOTIFY_BUFFER_SIZE 4096
#define SOCKET_BACKLOG 5
#define CHECK_INTERVAL_MS 5000

// Achievement structure
typedef struct {
    char id[32];
    char name[64];
    char description[128];
    int progress;
    int target;
    int unlocked;
    time_t unlock_time;
} Achievement;

// Notification structure
typedef struct {
    char message[256];
    char type[32];  // "achievement", "random", "system"
    time_t timestamp;
    int priority;
} Notification;

// Engine state
typedef struct {
    int enabled;
    pthread_mutex_t data_mutex;
    pthread_t achievement_thread;
    pthread_t notification_thread;
    pthread_t wayland_listener;
    pthread_t kernel_hook;
    int data_fd;
    int notif_sock;
    Achievement achievements[MAX_ACHIEVEMENTS];
    int achievement_count;
    Notification notification_queue[MAX_NOTIFICATIONS];
    int notification_count;
    int notification_head;
    int notification_tail;
} SweetEngine;

// Global engine instance
SweetEngine engine = {0};

// Forward declarations
int load_config(void);
int save_engine_data(void);
int load_engine_data(void);
void init_directories(void);
int connect_notif_engine(void);
int send_notification(const char* message, const char* type, int priority);
void generate_random_notification(void);
void check_achievement_progress(void);
void* achievement_monitor_thread(void* arg);
void* notification_dispatcher_thread(void* arg);
void* wayland_event_listener(void* arg);
void* kernel_hook_listener(void* arg);
void signal_handler(int sig);
void unlock_achievement(const char* id);
void log_engine_event(const char* event);

// Parse INI file for SWEETENGINE key
int load_config(void) {
    FILE* fp = fopen(SWEETEXP_INI_PATH, "r");
    if (!fp) {
        fprintf(stderr, "SweetEngine: Config file not found, defaulting to disabled
");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "SWEETENGINE=true")) {
            engine.enabled = 1;
            fclose(fp);
            printf("SweetEngine: Enabled via config
");
            return 1;
        }
    }
    
    fclose(fp);
    printf("SweetEngine: Disabled via config
");
    return 0;
}

// Initialize data directories
void init_directories(void) {
    const char* base_path = "/lumen-motonexus6/fw/boot/main/k/sweetexp";
    const char* data_path = "/lumen-motonexus6/fw/boot/main/k/sweetexp/data";
    
    mkdir(base_path, 0755);
    mkdir(data_path, 0755);
    
    // Create data file if missing
    int fd = open(SWEETEXP_DATA_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd >= 0) close(fd);
}

// Connect to NotifEngine.java UNIX socket
int connect_notif_engine(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, NOTIFENGINE_SOCK, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

// Send notification to NotifEngine.java
int send_notification(const char* message, const char* type, int priority) {
    int sock = connect_notif_engine();
    if (sock < 0) {
        fprintf(stderr, "SweetEngine: Failed to connect to NotifEngine
");
        return -1;
    }
    
    char buffer[512];
    time_t now = time(NULL);
    snprintf(buffer, sizeof(buffer), 
             "{"type":"%s","message":"%s","priority":%d,"timestamp":%ld}
",
             type, message, priority, now);
    
    write(sock, buffer, strlen(buffer));
    close(sock);
    return 0;
}

// Generate random sweet notification
void generate_random_notification(void) {
    const char* random_msgs[] = {
        "You're crushing it today!",
        "Smooth boot sequence detected",
        "System purring like a kitten",
        "Achievement streak active",
        "Lumen OS loves you back",
        "Battery optimization master",
        "Kernel threads dancing happily",
        "Wayland compositor flexing",
        "Memory pressure minimal",
        "You're a system wizard"
    };
    
    int idx = rand() % (sizeof(random_msgs) / sizeof(random_msgs[0]));
    send_notification(random_msgs[idx], "random", 2);
}

// Check achievement progress and unlock
void check_achievement_progress(void) {
    // Example achievements - extend with kernel/Wayland metrics
    static int boot_count = 0;
    static int wayland_events = 0;
    
    // Simulate boot count achievement
    boot_count++;
    for (int i = 0; i < engine.achievement_count; i++) {
        if (strcmp(engine.achievements[i].id, "boot_master") == 0 &&
            boot_count >= engine.achievements[i].target &&
            !engine.achievements[i].unlocked) {
            unlock_achievement("boot_master");
        }
    }
    
    // Wayland event achievement (tracked by listener)
    for (int i = 0; i < engine.achievement_count; i++) {
        if (strcmp(engine.achievements[i].id, "wayland_pro") == 0 &&
            wayland_events >= engine.achievements[i].target &&
            !engine.achievements[i].unlocked) {
            unlock_achievement("wayland_pro");
        }
    }
}

// Unlock achievement and notify
void unlock_achievement(const char* id) {
    for (int i = 0; i < engine.achievement_count; i++) {
        if (strcmp(engine.achievements[i].id, id) == 0 && 
            !engine.achievements[i].unlocked) {
            
            engine.achievements[i].unlocked = 1;
            engine.achievements[i].unlock_time = time(NULL);
            
            char msg[256];
            snprintf(msg, sizeof(msg), 
                    "🏆 Achievement Unlocked: %s!
%s", 
                    engine.achievements[i].name,
                    engine.achievements[i].description);
            
            send_notification(msg, "achievement", 5);
            log_engine_event("Achievement unlocked");
            save_engine_data();
            break;
        }
    }
}

// Achievement monitoring thread
void* achievement_monitor_thread(void* arg) {
    struct timespec ts = {0, CHECK_INTERVAL_MS * 1000000L};
    
    while (engine.enabled) {
        pthread_mutex_lock(&engine.data_mutex);
        check_achievement_progress();
        pthread_mutex_unlock(&engine.data_mutex);
        nanosleep(&ts, NULL);
    }
    return NULL;
}

// Notification dispatcher thread
void* notification_dispatcher_thread(void* arg) {
    struct timespec ts = {0, 2000000000L};  // 2 seconds
    
    while (engine.enabled) {
        // Dispatch queued notifications
        pthread_mutex_lock(&engine.data_mutex);
        if (engine.notification_head != engine.notification_tail) {
            Notification notif = engine.notification_queue[engine.notification_head];
            send_notification(notif.message, notif.type, notif.priority);
            engine.notification_head = (engine.notification_head + 1) % MAX_NOTIFICATIONS;
            engine.notification_count--;
        }
        pthread_mutex_unlock(&engine.data_mutex);
        
        // Random notification chance
        if (rand() % 100 < 5) {  // 5% chance every 2s
            generate_random_notification();
        }
        
        nanosleep(&ts, NULL);
    }
    return NULL;
}

// Wayland event listener (placeholder for compositor integration)
void* wayland_event_listener(void* arg) {
    static int event_count = 0;
    while (engine.enabled) {
        // Monitor Wayland events at /lumen-motonexus6/system/graph/mod/system2Dengine.LUMENGUI/core/wayland
        // Placeholder: increment counter based on Wayland socket activity
        event_count += rand() % 10;
        
        // Queue achievement progress notification
        pthread_mutex_lock(&engine.data_mutex);
        if (event_count % 50 == 0 && engine.notification_count < MAX_NOTIFICATIONS - 1) {
            Notification* notif = &engine.notification_queue[engine.notification_tail];
            snprintf(notif->message, sizeof(notif->message), 
                    "Wayland events: %d processed", event_count);
            strcpy(notif->type, "system");
            notif->priority = 1;
            notif->timestamp = time(NULL);
            engine.notification_tail = (engine.notification_tail + 1) % MAX_NOTIFICATIONS;
            engine.notification_count++;
        }
        pthread_mutex_unlock(&engine.data_mutex);
        
        sleep(5);
    }
    return NULL;
}

// Kernel hook listener (placeholder for /lumen-motonexus6/fw/boot/main/k integration)
void* kernel_hook_listener(void* arg) {
    int fd = inotify_init();
    if (fd < 0) return NULL;
    
    // Watch kernel directories for activity
    int wd = inotify_add_watch(fd, "/proc/stat", IN_MODIFY);
    char buffer[INOTIFY_BUFFER_SIZE];
    
    while (engine.enabled) {
        ssize_t len = read(fd, buffer, INOTIFY_BUFFER_SIZE);
        if (len > 0) {
            // Kernel activity detected - potential achievement trigger
            pthread_mutex_lock(&engine.data_mutex);
            check_achievement_progress();
            pthread_mutex_unlock(&engine.data_mutex);
        }
    }
    
    close(fd);
    return NULL;
}

// Save engine state
int save_engine_data(void) {
    pthread_mutex_lock(&engine.data_mutex);
    
    int fd = open(SWEETEXP_DATA_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&engine.data_mutex);
        return -1;
    }
    
    char buffer[DATA_BUFFER_SIZE];
    int written = 0;
    
    // Write header
    written += snprintf(buffer + written, sizeof(buffer) - written, 
                       "SWEETENGINE_DATA_v1
");
    
    // Write achievements
    for (int i = 0; i < engine.achievement_count; i++) {
        Achievement* ach = &engine.achievements[i];
        written += snprintf(buffer + written, sizeof(buffer) - written,
                           "ACH:%s|%s|%s|%d|%d|%d|%ld
",
                           ach->id, ach->name, ach->description,
                           ach->progress, ach->target, ach->unlocked, ach->unlock_time);
    }
    
    write(fd, buffer, written);
    close(fd);
    pthread_mutex_unlock(&engine.data_mutex);
    return 0;
}

// Load engine state
int load_engine_data(void) {
    int fd = open(SWEETEXP_DATA_PATH, O_RDONLY);
    if (fd < 0) return -1;
    
    char buffer[DATA_BUFFER_SIZE];
    ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    
    if (len <= 0) return -1;
    buffer[len] = '';
    
    // Parse achievements
    char* line = strtok(buffer, "
");
    engine.achievement_count = 0;
    
    while (line && engine.achievement_count < MAX_ACHIEVEMENTS) {
        if (strncmp(line, "ACH:", 4) == 0) {
            Achievement* ach = &engine.achievements[engine.achievement_count];
            sscanf(line + 4, "%31[^|]|%63[^|]|%127[^|]|%d|%d|%d|%ld",
                   ach->id, ach->name, ach->description,
                   &ach->progress, &ach->target, &ach->unlocked, &ach->unlock_time);
            engine.achievement_count++;
        }
        line = strtok(NULL, "
");
    }
    
    // Initialize default achievements if empty
    if (engine.achievement_count == 0) {
        // Boot Master
        strcpy(engine.achievements[0].id, "boot_master");
        strcpy(engine.achievements[0].name, "Boot Master");
        strcpy(engine.achievements[0].description, "Boot 10 times successfully");
        engine.achievements[0].target = 10;
        engine.achievement_count = 1;
        
        // Wayland Pro
        strcpy(engine.achievements[1].id, "wayland_pro");
        strcpy(engine.achievements[1].name, "Wayland Pro");
        strcpy(engine.achievements[1].description, "Process 500 Wayland events");
        engine.achievements[1].target = 500;
        engine.achievement_count = 2;
    }
    
    return 0;
}

// Log engine events
void log_engine_event(const char* event) {
    FILE* log = fopen("/lumen-motonexus6/fw/boot/main/k/sweetexp/engine.log", "a");
    if (log) {
        time_t now = time(NULL);
        char* time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '';  // Remove newline
        fprintf(log, "[%s] %s
", time_str, event);
        fclose(log);
    }
}

// Signal handler for clean shutdown
void signal_handler(int sig) {
    printf("SweetEngine: Received signal %d, shutting down
", sig);
    engine.enabled = 0;
    save_engine_data();
    exit(0);
}

int main(void) {
    printf("SweetExperiencesEngine starting...
");
    
    // Initialize
    pthread_mutex_init(&engine.data_mutex, NULL);
    srand(time(NULL) ^ getpid());
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize filesystem
    init_directories();
    
    // Load configuration
    if (!load_config()) {
        printf("SweetEngine: Disabled by config
");
        return 0;
    }
    
    // Load persistent data
    load_engine_data();
    
    printf("SweetEngine: Initialized with %d achievements
", engine.achievement_count);
    log_engine_event("Engine started");
    
    // Start threads
    pthread_create(&engine.achievement_thread, NULL, achievement_monitor_thread, NULL);
    pthread_create(&engine.notification_thread, NULL, notification_dispatcher_thread, NULL);
    pthread_create(&engine.wayland_listener, NULL, wayland_event_listener, NULL);
    pthread_create(&engine.kernel_hook, NULL, kernel_hook_listener, NULL);
    
    // Main loop - monitor config changes
    int inotify_fd = inotify_init();
    inotify_add_watch(inotify_fd, dirname(SWEETEXP_INI_PATH), IN_MODIFY);
    
    char buffer[INOTIFY_BUFFER_SIZE];
    while (engine.enabled) {
        ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
        if (len > 0) {
            load_config();  // Reload config on change
        }
        usleep(100000);  // 100ms
    }
    
    // Cleanup
    pthread_join(engine.achievement_thread, NULL);
    pthread_join(engine.notification_thread, NULL);
    pthread_join(engine.wayland_listener, NULL);
    pthread_join(engine.kernel_hook, NULL);
    
    pthread_mutex_destroy(&engine.data_mutex);
    log_engine_event("Engine stopped");
    
    printf("SweetEngine: Shutdown complete
");
    return 0;
}