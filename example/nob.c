#define NOB_IMPLEMENTATION
#include "../nob.h"

#define BUILD_DIR "build/"

const char* CFLAGS[] = {
    "-Wall",       //
    "-Wextra",     //
    "-Wpedantic",  //
    "-std=c99",    //
    "-O2",         //
    "-g",          //
};

int main(int argc, char* argv[]) {
    log_print("init");
    rebuild_myself(argc, argv);

    const char* CC = getenv("CC");
    CC = CC == NULL ? "cc" : CC;

    Cmd cmd = {0};

    log_print("build");
    const char* src = "main.c";
    const char* obj = BUILD_DIR "main.o";
    fs_mkdir_recursive(obj);
    cmd_append(&cmd, CC, "-c", src, "-o", obj);
    arr_append_many(&cmd, CFLAGS, ARRAY_COUNT(CFLAGS));
    compile_db_append(cmd);

    // compile only if obj is outdated
    if (fs_outdated(obj, src)) {
        if (!proc_wait(cmd_exec_async(cmd)))
            return EXIT_FAILURE;
    }

    log_print("write compilation db");
    compile_db_write(BUILD_DIR "compile_commands.json");

    log_print("link");
    cmd.count = 0;  // reuse previous command memory
#ifdef _WIN32
    cmd_append(&cmd, CC, "-o", BUILD_DIR "example.exe", obj);
#else   // _WIN32
    cmd_append(&cmd, CC, "-o", BUILD_DIR "example", obj);
#endif  // _WIN32
    if (!proc_wait(cmd_exec_async(cmd)))
        return EXIT_FAILURE;

    free(cmd.items);
    log_print("success");

    return EXIT_SUCCESS;
}
