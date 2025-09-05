// https://github.com/m6vrm/nob.h
#ifndef NOB_H
#define NOB_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else  // _WIN32
#include <sys/types.h>
#endif  // _WIN32

#ifdef __cplusplus
#define DECLTYPE(expr) (decltype(expr))
#else  // __cplusplus
#define DECLTYPE(expr)
#endif  // __cplusplus

#define RETURN_DEFER(value) \
    do {                    \
        result = (value);   \
        goto defer;         \
    } while (false)

#define ARRAY(Type, ...) ((Type[]){__VA_ARGS__})
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(*(arr)))
#define ARRAY_FOREACH(arr, Type) ARRAY_FOREACH_AS(arr, it, Type)
#define ARRAY_FOREACH_AS(arr, item, Type) \
    for (Type * (item) = (arr); (item) < (arr) + ARRAY_COUNT(arr); ++(item))

#define arr_reserve(arr, new_capacity)                                              \
    do {                                                                            \
        if ((new_capacity) > (arr)->capacity) {                                     \
            while ((new_capacity) > (arr)->capacity) {                              \
                (arr)->capacity = (arr)->capacity == 0 ? 128 : (arr)->capacity * 2; \
            }                                                                       \
            (arr)->items = DECLTYPE((arr)->items)                                   \
                realloc((arr)->items, (arr)->capacity * sizeof(*(arr)->items));     \
            assert((arr)->items != NULL);                                           \
        }                                                                           \
    } while (false)
#define arr_append(arr, item)               \
    do {                                    \
        arr_reserve(arr, (arr)->count + 1); \
        (arr)->items[(arr)->count] = item;  \
        (arr)->count += 1;                  \
    } while (false)
#define arr_append_many(arr, new_items, new_items_count)                                           \
    do {                                                                                           \
        arr_reserve(arr, (arr)->count + (new_items_count));                                        \
        memcpy((arr)->items + (arr)->count, new_items, (new_items_count) * sizeof(*(arr)->items)); \
        (arr)->count += new_items_count;                                                           \
    } while (false)
#define arr_foreach(arr, Type) arr_foreach_as(arr, it, Type)
#define arr_foreach_as(arr, item, Type) \
    for (Type * (item) = (arr).items; (item) < (arr).items + (arr).count; ++(item))

typedef struct {
    char* items;
    size_t count;
    size_t capacity;
} Str;
void str_append_cstr(Str* str, const char* cstr);

#ifdef _WIN32
typedef HANDLE Proc;
#define PROC_INVALID INVALID_HANDLE_VALUE
#else  // _WIN32
typedef pid_t Proc;
#define PROC_INVALID (-1)
#endif  // _WIN32
typedef struct {
    Proc* items;
    size_t count;
    size_t capacity;
} Procs;
#define procs_wait(...) \
    procs_wait_many(ARRAY(Proc, __VA_ARGS__), ARRAY_COUNT(ARRAY(Proc, __VA_ARGS__)))
bool proc_wait(Proc proc);
bool procs_wait_many(const Proc* procs, size_t procs_count);

typedef struct {
    const char** items;
    size_t count;
    size_t capacity;
} Cmd;
#define cmd_append(cmd, ...)                              \
    arr_append_many(cmd, ARRAY(const char*, __VA_ARGS__), \
                    ARRAY_COUNT(ARRAY(const char*, __VA_ARGS__)))
void cmd_str(Cmd cmd, Str* str);
Proc cmd_exec_async(Cmd cmd);

#define fs_outdated(dst, ...)                              \
    fs_outdated_many(dst, ARRAY(const char*, __VA_ARGS__), \
                     ARRAY_COUNT(ARRAY(const char*, __VA_ARGS__)))
int fs_outdated_many(const char* dst, const char** srcs, size_t srcs_count);
bool fs_mkdir(const char* path);
bool fs_mkdir_recursive(const char* path);
bool fs_write(const char* path, const char* data, size_t size);
bool fs_current_dir(char* dst, size_t size);

#define rebuild_myself(argc, argv) rebuild_myself_with(argc, argv, )
#define rebuild_myself_with(argc, argv, ...)                                   \
    rebuild_myself_many(argc, argv, ARRAY(const char*, __FILE__, __VA_ARGS__), \
                        ARRAY_COUNT(ARRAY(const char*, __FILE__, __VA_ARGS__)))
void rebuild_myself_many(int argc, char* argv[], const char** srcs, size_t srcs_count);

void* tmp_alloc(size_t size);
void tmp_reset(void);
char* tmp_sprintf(const char* fmt, ...);

static Str compile_db;
bool compile_db_append(Cmd cmd);
bool compile_db_write(const char* path);

void log_print(const char* fmt, ...);

#define args_shift(argc, argv) (assert((argc) > 0), (argc)--, *(argv)++)

#endif  // NOB_H

#ifdef NOB_IMPLEMENTATION

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif  // _WIN32

#ifndef PATH_MAX
#ifdef _WIN32
#define PATH_MAX MAX_PATH
#else  // _WIN32
#define PATH_MAX 256
#endif  // _WIN32
#endif  // PATH_MAX

#ifndef REBUILD_CMD
#ifdef _WIN32
#if defined(__GNUC__)  // _WIN32
#define REBUILD_CMD(dst, src) "gcc", "-Wall", "-Wextra", "-Wpedantic", "-o", dst, src
#elif defined(__clang__)  // __GNUC__
#define REBUILD_CMD(dst, src) "clang", "-Wall", "-Wextra", "-Wpedantic", "-o", dst, src
#elif defined(_MSC_VER)  // __clang__
#define REBUILD_CMD(dst, src) "cl.exe", tmp_sprintf("/Fe:%s", dst), src
#endif  // _MSC_VER
#else   // _WIN32
#define REBUILD_CMD(dst, src) "cc", "-Wall", "-Wextra", "-Wpedantic", "-o", dst, src
#endif  // _WIN32
#endif  // _REBUILD_CMD

#ifndef TMP_BUF_CAP
#define TMP_BUF_CAP 8 * 1024 * 1024
#endif  // TMP_BUF_CAP
static struct {
    char buf[TMP_BUF_CAP];
    size_t size;
} tmp;

void str_append_cstr(Str* str, const char* cstr) {
    assert(str != NULL);
    assert(cstr != NULL);

    if (*cstr == '\0')
        return;
    size_t n = strlen(cstr);
    arr_append_many(str, cstr, n);
    arr_append(str, '\0');
    str->count -= 1;
}

bool proc_wait(Proc proc) {
    if (proc == PROC_INVALID)
        return false;

#ifdef _WIN32
    if (WaitForSingleObject(proc, INFINITE) == WAIT_FAILED) {
        log_print("[ERROR] could not wait for process");
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        log_print("[ERROR] could not get process exit code");
        return false;
    }

    if (exit_status != EXIT_SUCCESS) {
        log_print("[ERROR] process exited with exit code %d", exit_status);
        return false;
    }

    CloseHandle(proc);
#else   // _WIN32
    for (;;) {
        int status = 0;
        if (waitpid(proc, &status, 0) < 0) {
            log_print("[ERROR] could not wait for process (pid %d): %s", proc, strerror(errno));
            return false;
        } else if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != EXIT_SUCCESS) {
                log_print("[ERROR] process exited with exit code %d", exit_status);
                return false;
            }

            break;
        } else if (WIFSIGNALED(status)) {
            log_print("[ERROR] process was terminated by signal %d", WTERMSIG(status));
            return false;
        }
    }
#endif  // _WIN32

    return true;
}

bool procs_wait_many(const Proc* procs, size_t procs_count) {
    bool result = true;
    for (const Proc* proc = procs; proc < procs + procs_count; ++proc)
        result = proc_wait(*proc) && result;
    return result;
}

void cmd_str(Cmd cmd, Str* str) {
    assert(str != NULL);

    arr_foreach(cmd, const char*) {
        if (*it == NULL)
            continue;
        if (**it == '\0')
            continue;
        if (it != cmd.items && str->count > 0)
            str_append_cstr(str, " ");
        if (strchr(*it, ' ') == NULL) {
            str_append_cstr(str, *it);
        } else {
            arr_append(str, '\'');
            str_append_cstr(str, *it);
            arr_append(str, '\'');
        }
    }
}

Proc cmd_exec_async(Cmd cmd) {
    assert(cmd.count > 0);

    Cmd cmd_copy = {0};
    arr_foreach(cmd, const char*) {
        if (**it == '\0')
            continue;
        arr_append(&cmd_copy, *it);
    }
    arr_append(&cmd_copy, NULL);
    assert(cmd_copy.count > 0);

    Str str = {0};
    cmd_str(cmd_copy, &str);
    log_print("[INFO] CMD: %s", str.items);

    Proc result = PROC_INVALID;

#ifdef _WIN32
    STARTUPINFO start_info;
    GetStartupInfo(&start_info);
    PROCESS_INFORMATION proc_info;
    bool created =
        CreateProcessA(NULL, str.items, NULL, NULL, true, 0, NULL, NULL, &start_info, &proc_info);
    if (!created) {
        log_print("[ERROR] could not create process for %s", cmd_copy.items[0]);
        RETURN_DEFER(PROC_INVALID);
    }

    CloseHandle(proc_info.hThread);
    RETURN_DEFER(proc_info.hProcess);
#else   // _WIN32
    Proc pid = fork();
    if (pid < 0) {
        log_print("[ERROR] could not fork process: %s", strerror(errno));
        RETURN_DEFER(PROC_INVALID);
    } else if (pid == 0) {  // child process
        if (execvp(cmd_copy.items[0], (char**)cmd_copy.items) < 0) {
            log_print("[ERROR] could not exec process for %s: %s", cmd_copy.items[0],
                      strerror(errno));
            free(str.items);
            free(cmd_copy.items);
            exit(EXIT_FAILURE);
        }
    }
    RETURN_DEFER(pid);
#endif  // _WIN32

defer:
    free(str.items);
    free(cmd_copy.items);
    return result;
}

int fs_outdated_many(const char* dst, const char** srcs, size_t srcs_count) {
    assert(dst != NULL);
    assert(srcs != NULL);
    assert(srcs_count > 0);

#ifdef _WIN32
    HANDLE dst_fd =
        CreateFile(dst, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (dst_fd == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return 1;  // dst not exists, assume outdated
        log_print("[ERROR] could not open file %s", dst);
        return -1;
    }

    FILETIME dst_time;
    BOOL got_dst_time = GetFileTime(dst_fd, NULL, NULL, &dst_time);
    CloseHandle(dst_fd);
    if (!got_dst_time) {
        log_print("[ERROR] could not get time of %s", dst);
        return -1;
    }

    for (size_t i = 0; i < srcs_count; ++i) {
        HANDLE src_fd = CreateFile(srcs[i], GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_READONLY, NULL);
        if (src_fd == INVALID_HANDLE_VALUE) {
            log_print("[WARN] could not open file %s", srcs[i]);
            continue;  // src not exists, ignore
        }

        FILETIME src_time;
        BOOL got_src_time = GetFileTime(src_fd, NULL, NULL, &src_time);
        CloseHandle(src_fd);
        if (!got_src_time) {
            log_print("[ERROR] could not get time of %s", srcs[i]);
            return -1;
        }

        if (CompareFileTime(&src_time, &dst_time) == 1)
            return 1;
    }
#else   // _WIN32
    struct stat statbuf = {0};
    if (stat(dst, &statbuf) < 0) {
        if (errno == ENOENT)
            return 1;  // dst not exists, assume outdated
        log_print("[ERROR] could not stat %s: %s", dst, strerror(errno));
        return -1;
    }

    int dst_mtime = statbuf.st_mtime;
    for (size_t i = 0; i < srcs_count; ++i) {
        if (stat(srcs[i], &statbuf) < 0) {
            log_print("[WARN] could not stat %s: %s", srcs[i], strerror(errno));
            continue;  // src not exists, ignore
        }

        if (statbuf.st_mtime > dst_mtime)
            return 1;
    }
#endif  // _WIN32

    log_print("[INFO] %s is up-to-date", dst);
    return 0;
}

bool fs_mkdir(const char* path) {
    assert(path != NULL);

#ifdef _WIN32
    if (CreateDirectory(path, NULL) == 0) {
        if (GetLastError() == ERROR_ALREADY_EXISTS)
            return true;
        log_print("[ERROR] could not create directory %s", path);
        return false;
    }
#else   // _WIN32
    if (mkdir(path, 0755) < 0) {
        if (errno == EEXIST)
            return true;
        log_print("[ERROR] could not create directory %s: %s", path, strerror(errno));
        return false;
    }
#endif  // _WIN32

    log_print("[INFO] create directory %s", path);
    return true;
}

bool fs_mkdir_recursive(const char* path) {
    assert(path != NULL);

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", path);

    char* slash = buf;
    while ((slash = strchr(slash, '/')) != NULL) {
        *slash = '\0';
        if (!fs_mkdir(buf))
            return false;
        *slash = '/';
        ++slash;
    }

    return true;
}

bool fs_write(const char* path, const char* data, size_t size) {
    assert(path != NULL);
    assert(data != NULL);

    bool result = true;
    const char* buf = data;

    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        log_print("[ERROR] could not open file %s for writing: %s", path, strerror(errno));
        RETURN_DEFER(false);
    }

    while (size > 0) {
        size_t n = fwrite(buf, 1, size, file);
        if (ferror(file)) {
            log_print("[ERROR] error writing file %s: %s", path, strerror(errno));
            RETURN_DEFER(false);
        }

        size -= n;
        buf += n;
    }

defer:
    if (file != NULL)
        fclose(file);
    return result;
}

bool fs_current_dir(char* dst, size_t size) {
    assert(dst != NULL);

#ifdef _WIN32
    if (GetCurrentDirectory(size, dst) == 0) {
        log_print("[ERROR] could not get current directory");
        return false;
    }
#else   // _WIN32
    if (getcwd(dst, size) == NULL) {
        log_print("[ERROR] could not get current directory: %s", strerror(errno));
        return false;
    }
#endif  // _WIN32

    return true;
}

void rebuild_myself_many(int argc, char* argv[], const char** src, size_t src_count) {
    assert(src != NULL);
    assert(src_count > 0);

    char* dst = args_shift(argc, argv);
    int dst_outdated = fs_outdated_many(dst, src, src_count);
    if (dst_outdated < 0) {
        exit(EXIT_FAILURE);
    } else if (dst_outdated == 0) {
        return;
    }

    log_print("[INFO] rebuild myself");

    Cmd cmd = {0};
    cmd_append(&cmd, REBUILD_CMD(dst, src[0]));
    Proc proc = cmd_exec_async(cmd);
    if (!proc_wait(proc))
        exit(EXIT_FAILURE);

    cmd.count = 0;
    cmd_append(&cmd, dst);
    arr_append_many(&cmd, argv, argc);
    proc = cmd_exec_async(cmd);
    if (!proc_wait(proc))
        exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}

void* tmp_alloc(size_t size) {
    if (tmp.size + size > sizeof(tmp.buf)) {
        assert(false);
        return NULL;
    }

    void* buf = tmp.buf + tmp.size;
    tmp.size += size;
    return buf;
}

void tmp_reset(void) {
    tmp.size = 0;
}

char* tmp_sprintf(const char* fmt, ...) {
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    assert(n >= 0);
    va_end(args);

    char* result = (char*)tmp_alloc(n + 1);
    va_start(args, fmt);
    vsnprintf(result, n + 1, fmt, args);
    va_end(args);
    return result;
}

bool compile_db_append(Cmd cmd) {
    if (compile_db.count == 0)
        str_append_cstr(&compile_db, "[\n");

    char path[PATH_MAX];
    fs_current_dir(path, sizeof(path));
    Str str = {0};
    cmd_str(cmd, &str);

    const char* file = NULL;
    arr_foreach(cmd, const char*) {
        const char* ext = strrchr(*it, '.');
        if (ext == NULL)
            continue;

        if (strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0 ||
            strcmp(ext, ".cxx") == 0) {
            file = *it;
            break;
        }
    }

    bool result = true;
    char* json = tmp_sprintf("{\"directory\": \"%s\", \"command\": \"%s\", \"file\": \"%s\"},\n",
                             path, str.items, file);

    if (file == NULL) {
        log_print("[WARN] could not find file in compile command %s", str.items);
        RETURN_DEFER(false);
    }

    if (json == NULL)
        RETURN_DEFER(false);

    str_append_cstr(&compile_db, json);

defer:
    free(str.items);
    return result;
}

bool compile_db_write(const char* path) {
    assert(path != NULL);

    if (compile_db.count == 0)
        return true;

    assert(compile_db.count >= 2);
    compile_db.count -= 2;  // remove trailing newline and comma
    str_append_cstr(&compile_db, "\n]\n");
    return fs_write(path, compile_db.items, compile_db.count);
}

void log_print(const char* fmt, ...) {
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#endif  // NOB_IMPLEMENTATION
